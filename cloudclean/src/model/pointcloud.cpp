#include "model/pointcloud.h"

#include <limits>
#include <string>
#include <cstdlib>
#include <cassert>
#include <cstdio>

#include <boost/serialization/shared_ptr.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <pcl/io/io.h>
#include <Eigen/Dense>
#include <Eigen/Core>
#include <Eigen/Geometry>

#include <QDebug>



using namespace Eigen;

#ifdef _WIN32
#   define INFINITY (DBL_MAX+DBL_MAX)
#   define NAN (INFINITY-INFINITY)
#endif

inline bool isNaN(float val){
    return (val != val);
}

EventDispatcher::EventDispatcher(PointCloud * pc) {
    pc_ = pc;
}

void EventDispatcher::updateProgress(int value){
    emit progress(value);
}

void EventDispatcher::emitlabelUpdate(std::shared_ptr<std::vector<int> > idxs) {
    emit labelUpdate();
}

void EventDispatcher::emitflagUpdate(std::shared_ptr<std::vector<int> > idxs) {
    emit flagUpdate(idxs);
}

void EventDispatcher::resetOrientation() {
    qDebug() << "reset";
    pc_->sensor_orientation_ = pc_->sensor_orientation_.setIdentity();
    emit transformed();
}

PointCloud::PointCloud()
    : pcl::PointCloud<pcl::PointXYZI>() {
    pc_mutex.reset(new std::mutex());
    ed_.reset(new EventDispatcher(this));
    frame_ = CoordinateFrame::Laser;

    min_bounding_box_ = Eigen::Vector3f(INFINITY, INFINITY, INFINITY);
    max_bounding_box_ = Eigen::Vector3f(-INFINITY, -INFINITY, -INFINITY);
}

bool PointCloud::save_ptx(const char* filename){
    /*pc_mutex->lock();
    std::ofstream ptx_file(filename);
    ptx_file << this->width << std::endl;
    ptx_file << this->height << std::endl;
    ptx_file << this->sensor_origin_[0] << " " << this->sensor_origin_[1]
             << " "<< this->sensor_origin_[2] << std::endl;

    // File is column major
    Eigen::Matrix3f rmat(this->sensor_orientation_);
    Eigen::Matrix4f tmat;
    tmat << rmat(0, 0) , rmat(0, 1) , rmat(0, 2) , this->sensor_origin_[0] ,
            rmat(1, 0) , rmat(1, 1) , rmat(1, 2) , this->sensor_origin_[1] ,
            rmat(2, 0) , rmat(2, 1) , rmat(2, 2) , this->sensor_origin_[2] ,
            0 , 0 , 0 , 1;


    for(int c = 0; c < 3; c++){
        for(int r = 0; r < 3; r++){
            ptx_file << rmat(r, c);
            if(r < 2)
                ptx_file << " ";
            else
                ptx_file << std::endl;
        }
    }

    for(int c = 0; c < 4; c++){
        for(int r = 0; r < 4; r++){
            ptx_file << tmat(r, c);
            if(r < 3)
                ptx_file << " ";
            else
                ptx_file << std::endl;
        }
    }

    // Write points
    for(unsigned int i = 0; i < this->points.size(); i++) {
        if(isNaN(this->points[i].x) || isNaN(this->points[i].y)
                || isNaN(this->points[i].z
                || isNaN(this->points[i].intensity))){
            ptx_file << "0 0 0 0.5" << std::endl;
        }
        else{
            ptx_file << this->points[i].x << " " << this->points[i].y
                     << " " << this->points[i].z << " "
                     << this->points[i].intensity << std::endl;
        }
    }

    ptx_file.close();
    pc_mutex->unlock();
    return true;
    */
}

bool PointCloud::load_ptx(const char* filename, int decimation_factor) {
    pc_mutex->lock();
    ed_->updateProgress(0);
    assert(decimation_factor%2 == 0 || decimation_factor == 1);

    // Makes things faster apparently
    /*std::cin.sync_with_stdio(false,  std::ios::binary);

    std::ifstream ptx_file(filename);
    if (!ptx_file.is_open()) {
        return false;
    }
    */

    /*
    boost::iostreams::mapped_file_source file;
    file.open(filename);
    if(!file.is_open())
        return false;
    boost::iostreams::stream_buffer<boost::iostreams::mapped_file_source> file_stream;
    file_stream.open(file);
    std::istream ptx_file(&file_stream);
    */


    FILE * pfile;
    //char buffer [1024];
    pfile = fopen (filename , "r");
    if (pfile == NULL){
        qDebug("Error opening file");
        return false;
    }

    // Contains nans
    this->is_dense = false;

    // Matrix dimentions
    int file_width, file_height;
    fscanf(pfile, "%d %d", &file_width, &file_height);
    qDebug() << "dims" << file_width << file_height;
    //ptx_file >> file_width;
    //ptx_file >> file_height;

    // Subsample
    this->scan_width_ =  file_width/decimation_factor;
    this->scan_height_ = file_height/decimation_factor;

    // Camera offset
    std::fscanf(pfile, "%f %f %f", &this->sensor_origin_[0], &this->sensor_origin_[1], &this->sensor_origin_[2]);
    qDebug() << "yeah: " << this->sensor_origin_[0];
    //ptx_file >> this->sensor_origin_[0];
    //ptx_file >> this->sensor_origin_[1];
    //ptx_file >> this->sensor_origin_[2];
    this->sensor_origin_[3] = 0.0f;

    Eigen::Matrix3f orientation_mat;
    for(int row = 0; row < 3; row++ )
        for(int col = 0; col < 3; col++ )
            fscanf(pfile, "%f", &orientation_mat(row,col));
            //ptx_file >> orientation_mat(row,col);
    this->sensor_orientation_ = Eigen::Quaternionf(orientation_mat.transpose());

    // Discard registration mat4
    float ign;
    for(int i = 0; i < 16; i++ )
        fscanf(pfile, "%f", &ign);

    //ptx_file >> std::ws;


    unsigned int sampled_idx = 0;
    int file_sample_idx = 0;
    int row = 0, col = 0;

    int line_count = file_width*file_height;
    int update_interval = line_count/100;

    if(update_interval == 0)
        update_interval = 1;

    //char buff[1024];
    //string line;
    pcl::PointXYZI point;
    float & x = point.x;
    float & y = point.y;
    float & z = point.z;
    float & intensity = point.intensity;

    while(file_sample_idx < line_count){
        if(file_sample_idx % update_interval == 0)
            ed_->updateProgress(100*file_sample_idx/static_cast<float>(line_count));

        row = file_sample_idx / file_width;
        col = file_sample_idx % file_width;
        file_sample_idx++;

        // Only process every decimation_factor-ith row and column
        if((row+1)%decimation_factor != 0 || (col+1)%decimation_factor != 0){
            //ptx_file >> x >> y >> z >> intensity;
            //ptx_file.getline(buff, 1024);
            //getline(ptx_file, line);
            for(int i = 0; i < 4; i++ )
                fscanf(pfile, "%f", &ign);
            continue;
        }

        //ptx_file >> x >> y >> z >> intensity;

        //char buff[] = "0.003313 0.805950 -1.675033 0.015869\0jabckjbckab";

        //memset((void *)&buff, '\0', sizeof(buff));
        //ptx_file >> ws;
        //ptx_file.getline(buff, 1024);
        //getline(ptx_file, line);

        if(ferror (pfile))
            return false;

        float x, y, z, intensity;
        int filled = fscanf(pfile, "%f %f %f %f", &x, &y, &z, &intensity);
        if(filled != 4) {
            qDebug() << "--------------";
            qDebug() << x << y << z << intensity;
            qDebug() << filled;
            //qDebug() << line.c_str();
            //printf("'%s'", buff);
            qDebug() << "failed";
            return false;
        }
        else {
            qDebug() << "--------------";
            //qDebug() << buff;
            //qDebug() << line.c_str();
            qDebug() << "success";
        }


        sampled_idx++;

        // Skip points that are invalid
        if((x == 0) && (y == 0) && (z == 0)) {
            continue;
        }

        // Set bounding box
        if(x < min_bounding_box_.x())
            min_bounding_box_.x() = x;
        if(y < min_bounding_box_.y())
            min_bounding_box_.y() = y;
        if(z < min_bounding_box_.z())
            min_bounding_box_.z() = z;

        if(x > max_bounding_box_.x())
            min_bounding_box_.x() = x;
        if(y > max_bounding_box_.y())
            min_bounding_box_.y() = y;
        if(z > max_bounding_box_.z())
            min_bounding_box_.z() = z;

        this->points.push_back(point);
        this->cloud_to_grid_map_.push_back(sampled_idx);
    }

    this->width = this->points.size();
    this->height = 1;
    this->is_dense = true;
    labels_.resize(this->width * this->height, 0);
    flags_.resize(this->width * this->height, PointFlags(0));

    ed_->updateProgress(100);
    pc_mutex->unlock();

    // Start loading octree
    fut_octree = std::async(std::launch::async, [this](){
        double resolution = 0.2;
        qDebug() << "Start octree";
        Octree::Ptr octree = Octree::Ptr(new Octree(resolution));
        pcl::PointCloud<pcl::PointXYZI>::ConstPtr cptr(this, boost::serialization::null_deleter());
        octree->setInputCloud(cptr);
        octree->defineBoundingBox();
        octree->addPointsFromInputCloud();
        qDebug() << "Done with octree";
        return octree;
    });

    fclose (pfile);
    return this;
}

void PointCloud::translate(const Eigen::Vector3f& pos) {
    sensor_origin_ += Vector4f(pos.x(), pos.y(), pos.z(), 0);
}

void PointCloud::rotate2D(float x, float y) {
    AngleAxis<float> rotX(-x, Vector3f::UnitZ());
    AngleAxis<float> rotY(-y, Vector3f::UnitY());
    sensor_orientation_ = (rotX * rotY) * sensor_orientation_;
}

Eigen::Affine3f PointCloud::modelview() {
    Translation3f tr(sensor_origin_.x(), sensor_origin_.y(), sensor_origin_.z());
    return Affine3f::Identity() * sensor_orientation_ * tr;
}

const Octree::Ptr PointCloud::getOctree() {
    if(octree.get() != nullptr)
        return octree;

    octree = fut_octree.get();
    return octree;
}
