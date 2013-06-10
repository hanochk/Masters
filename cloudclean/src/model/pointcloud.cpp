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
    deleting_ = false;
    visible_ = true;
}

PointCloud::~PointCloud() {
    qDebug() << "Cloud deleted";
}

inline bool PointCloud::point_matches_label(int idx, std::vector<uint16_t> & labels) {
        for(uint16_t l : labels) {
            if(labels_[idx] == l)
                return true;
        }
    return false;
}

bool PointCloud::save_ptx(const char* filename, std::vector<uint16_t> labels){
    //pc_mutex->lock();
    ed_->updateProgress(0);
    setlocale(LC_NUMERIC,"C");

    FILE * pfile;
    pfile = fopen(filename , "w");
    if (pfile == NULL){
        qDebug("Error opening file");
        return false;
    }

    fprintf(pfile, "%d\n%d\n", scan_width_, scan_height_);

    fprintf(pfile, "%f %f %f\n", this->sensor_origin_[0],
            this->sensor_origin_[1], this->sensor_origin_[2]);

    // File is column major
    Eigen::Matrix3f rmat(this->sensor_orientation_);


    fprintf(pfile, "%f %f %f\n", rmat(0, 0), rmat(1, 0), rmat(2, 0));
    fprintf(pfile, "%f %f %f\n", rmat(0, 1), rmat(1, 1), rmat(2, 1));
    fprintf(pfile, "%f %f %f\n", rmat(0, 2), rmat(1, 2), rmat(2, 2));

    fprintf(pfile, "%f %f %f %f\n", rmat(0, 0), rmat(1, 0), rmat(2, 0), 0.0f);
    fprintf(pfile, "%f %f %f %f\n", rmat(0, 1), rmat(1, 1), rmat(2, 1), 0.0f);
    fprintf(pfile, "%f %f %f %f\n", rmat(0, 2), rmat(1, 2), rmat(2, 2), 0.0f);
    fprintf(pfile, "%f %f %f %f\n", this->sensor_origin_[0],
            this->sensor_origin_[1], this->sensor_origin_[2], 1.0f);

    // Write points
    uint point_count = scan_height_*scan_width_;
    int update_interval = point_count/100;
    if(update_interval == 0)
        update_interval = 1;


    uint cloud_idx = 0;
    uint next_grid_idx = this->cloud_to_grid_map_[cloud_idx];

    for(uint grid_idx = 0; grid_idx < point_count; grid_idx++) {
        if(grid_idx % update_interval == 0)
            ed_->updateProgress(100*grid_idx/static_cast<float>(point_count));

        if(next_grid_idx != grid_idx) {
            fprintf(pfile, "%f %f %f %f\n", 0.0f, 0.0f, 0.0f, 0.0f);
            continue;
        }

        next_grid_idx = this->cloud_to_grid_map_[++cloud_idx];

        if(labels.size() != 0 && !point_matches_label(cloud_idx, labels)) {
            fprintf(pfile, "%f %f %f %f\n", 0.0f, 0.0f, 0.0f, 0.0f);
            continue;
        }

        fprintf(pfile, "%f %f %f %f\n", this->points[cloud_idx].x,
                this->points[cloud_idx].y, this->points[cloud_idx].z,
                this->points[cloud_idx].intensity);

    }

    fclose(pfile);
    ed_->updateProgress(100);
    //pc_mutex->unlock();
    return true;

}

bool PointCloud::load_ptx(const char* filename, int decimation_factor) {
    pc_mutex->lock();
    ed_->updateProgress(0);
    assert(decimation_factor%2 == 0 || decimation_factor == 1);

    // Avoid using a comma delimiter
    setlocale(LC_NUMERIC,"C");

    FILE * pfile;
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

    // Subsample
    this->scan_width_ =  file_width/decimation_factor;
    this->scan_height_ = file_height/decimation_factor;

    // Camera offset
    std::fscanf(pfile, "%f %f %f", &this->sensor_origin_[0],
            &this->sensor_origin_[1], &this->sensor_origin_[2]);

    this->sensor_origin_[3] = 0.0f;

    Eigen::Matrix3f orientation_mat;
    for(int row = 0; row < 3; row++ )
        for(int col = 0; col < 3; col++ )
            fscanf(pfile, "%f", &orientation_mat(row,col));

    this->sensor_orientation_ = Eigen::Quaternionf(orientation_mat.transpose());

    // Discard newline and redunadant matrix
    char buff[1024];
    for(int i = 0; i < 5; i++ ){
        fgets(buff, 1024, pfile);
        //qDebug("Skipped Mat '%s'", buff);
    }

    unsigned int sampled_idx = 0;
    int file_sample_idx = 0;
    int row = 0, col = 0;

    int line_count = file_width*file_height;
    int update_interval = line_count/100;

    if(update_interval == 0)
        update_interval = 1;

    pcl::PointXYZI point;
    float & x = point.x;
    float & y = point.y;
    float & z = point.z;
    float & intensity = point.intensity;

    // Tokenize the first line
    char * ch;
    long file_pos = ftell(pfile); // Save restart pos
    fgets(buff, 1024, pfile);
    int tokens = 0;

    ch = strtok(buff, " ");
    while (ch != NULL) {
        ++tokens;

        if(ch == NULL)
            break;
        ch = strtok(NULL, " ");
    }

    // Check if rgb channels are present
    bool has_rgb = false;
    if(tokens == 7)
        has_rgb = true;

    // Move reset the reading pos in file
    fseek(pfile, file_pos, SEEK_SET);

    while(file_sample_idx < line_count){
        if(file_sample_idx % update_interval == 0) {
            double prog = 100.0*file_sample_idx/static_cast<double>(line_count);
            //qDebug("%f%% done.", prog);
            ed_->updateProgress(prog);
        }

        row = file_sample_idx / file_width;
        col = file_sample_idx % file_width;
        file_sample_idx++;

        // Only process every decimation_factor-ith row and column
        if((row+1)%decimation_factor != 0 || (col+1)%decimation_factor != 0){
            //for(int i = 0; i < tokens; i++ )
            //    fscanf(pfile, "%f", &ign);
            qDebug() << "Skiped point";
            fgets(buff, 1024, pfile);
            continue;
        }

        if(ferror (pfile) || feof(pfile)){
            qDebug() << "File read fail at line " << file_sample_idx << "with sample idx:" << sampled_idx;
            return false;
        }

        //file_pos = ftell(pfile);
        int filled = fscanf(pfile, "%f %f %f %f\n", &x, &y, &z, &intensity);

        if(filled != 4) {
            qDebug() << "File parse fail at line " << file_sample_idx << "with sample idx" << sampled_idx;
            qDebug() << "File pos: " << file_pos;
            if(ferror(pfile))
                qDebug() << "File read fail.";
            else {
                //fgets(buff, 1024, pfile);
                //qDebug("Could not parse '%s'", buff);
                continue;
            }

            points.clear();
            width = 0;
            height = 0;
            scan_width_ = 0;
            scan_height_ = 0;
            return false;
        }

        // skip the rest of the line
        if(has_rgb) {
            fgets(buff, 1024, pfile);
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
        this->cloud_to_grid_map_.push_back(sampled_idx-1); // Undo the increment
    }

    this->width = this->points.size();
    this->height = 1;
    this->is_dense = true;
    labels_.resize(this->points.size(), 0);
    flags_.resize(this->points.size(), PointFlags(0));

    ed_->updateProgress(100);
    pc_mutex->unlock();

    // Start loading octree
    fut_octree_ = std::async(std::launch::async, [&, this](){
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

    fclose(pfile);
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

    AngleAxis<float> rotation(0, Vector3f(1, 0, 0));
    if(frame_ == CoordinateFrame::Camera){
        rotation = AngleAxis<float>(-M_PI/2, Vector3f(1, 0, 0));
    }

    return  rotation * sensor_orientation_ * tr * Affine3f::Identity();
}

const Octree::Ptr PointCloud::octree() {
    if(octree_.get() != nullptr)
        return octree_;

    octree_ = fut_octree_.get();
    return octree_;
}

std::shared_ptr<const std::vector<int> > PointCloud::gridToCloudMap() {
    // Weak pointer serves as a cache while in use
    std::shared_ptr<std::vector<int> > grid_to_cloud(
                grid_to_cloud_map_.lock());
    if(grid_to_cloud)
        return grid_to_cloud;

    // New map when weak_ptr expired
    grid_to_cloud = std::make_shared<std::vector<int>>(
                scan_width_*scan_height_, -1);

    for(int i = 0; i < size(); i++) {
        int grid_idx = cloud_to_grid_map_[i];
        (*grid_to_cloud)[grid_idx] = i;
    }

    grid_to_cloud_map_ = grid_to_cloud;

    return grid_to_cloud;
}

const std::vector<int> & PointCloud::cloudToGridMap() {
    return cloud_to_grid_map_;
}

bool PointCloud::isVisible() {
    return visible_;
}

void PointCloud::toggleVisible() {
    visible_ = !visible_;
}

const CoordinateFrame PointCloud::coordinateFrame() {
    return frame_;
}

int PointCloud::scan_width() {
    return scan_width_;
}

int PointCloud::scan_height(){
    return scan_height_;
}
