#include "plugins/visualisedepth/visualisedepth.h"
#include <iostream>
#include <QDebug>
#include <QAction>
#include <QToolBar>
#include <QtWidgets>
#include <QScrollArea>

#include <pcl/point_types.h>
#include <pcl/features/fpfh.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/features/principal_curvatures.h>
#include <pcl/octree/octree.h>
#include <pcl/octree/octree_iterator.h>
#include <pcl/octree/octree_container.h>
#include <boost/serialization/shared_ptr.hpp>

#include "model/layerlist.h"
#include "model/cloudlist.h"
#include "gui/glwidget.h"
#include "gui/flatview.h"
#include "gui/mainwindow.h"
#include "commands/select.h"
#include "pluginsystem/core.h"
#include "utilities/cv.h"
#include "utilities/utils.h"
#include "plugins/normalestimation/normalestimation.h"
#include "plugins/visualisedepth/utils.h"

QString VDepth::getName(){
    return "visualisedepth";
}

void VDepth::initialize(Core *core){
    settings_ = nullptr;
    tab_idx_ = -1;

    image_container_ = nullptr;
    image_ = nullptr;

    core_= core;
    cl_ = core_->cl_;
    ll_ = core_->ll_;
    glwidget_ = core_->mw_->glwidget_;
    flatview_ = core_->mw_->flatview_;
    mw_ = core_->mw_;
    myaction_ = new QAction(QIcon(":/images/visualisedepth.png"), "Visualisedepth", 0);

    depth_widget_ = new QWidget(0);
    tab_idx_ = core_->mw_->addTab(depth_widget_, "Depth map");

    QVBoxLayout * layout = new QVBoxLayout(depth_widget_);
    QScrollArea * scrollarea = new QScrollArea();
    scrollarea->setBackgroundRole(QPalette::Dark);
    layout->addWidget(scrollarea);
    image_container_ = new QLabel();
    scrollarea->setWidget(image_container_);

    // Nonsense
    connect(myaction_,&QAction::triggered, [this] (bool on) {
        qDebug() << "Click!";
    });
    connect(myaction_, SIGNAL(triggered()), this, SLOT(curve_vis()));
    mw_->toolbar_->addAction(myaction_);
	time = 0;

}

void VDepth::initialize2(PluginManager * pm) {
    ne_ = pm->findPlugin<NormalEstimator>();
    if (ne_ == nullptr) {
        qDebug() << "Normal estimator plugin needed for normal viz";
        return;
    }
}

void VDepth::cleanup(){
    mw_->toolbar_->removeAction(myaction_);
    mw_->removeTab(tab_idx_);
    delete myaction_;
    delete depth_widget_;
}

VDepth::~VDepth(){
    if(image_ != nullptr)
        delete image_;
}

int gridToCloudIdx(int x, int y, boost::shared_ptr<PointCloud> pc, int * lookup){
    if(x < 0 || x > pc->scan_width())
        return -1;
    else if(y < 0 || y > pc->scan_height())
        return -1;
    return lookup[x + y*pc->scan_width()];
}

void VDepth::drawFloats(boost::shared_ptr<const std::vector<float> > out_img, boost::shared_ptr<PointCloud> cloud){
    qDebug() << "DRAW!";
    // translates grid idx to cloud idx
    boost::shared_ptr<const std::vector<int>> lookup = cloud->gridToCloudMap();

    if(image_ != nullptr)
        delete image_;
    image_ = new QImage(cloud->scan_width(), cloud->scan_height(), QImage::Format_Indexed8);

    for(int i = 0; i < 256; i++) {
        image_->setColor(i, qRgb(i, i, i));
    }

    float min, max;
    min_max(*out_img, min, max);
    qDebug() << "Minmax" << min << max;

    // Draw image
    auto select = boost::make_shared<std::vector<int> >();
    for(int y = 0; y < cloud->scan_height(); y++){
        for(int x = 0; x < cloud->scan_width(); x++){
            int i = (cloud->scan_height() -1 - y) + x * cloud->scan_height();

            // Mask disabled
            if(lookup->at(i) == -2) {
                image_->setPixel(x, y, 0);
                continue;
            }

            //int intensity = 255 * (1 - distmap[i]/max_dist);
            float mag = (*out_img)[i];
            //int intensity = 255 * (1 - (mag - min)/(max - min));

            int intensity = 255 * (mag - min)/(max - min);

            if(intensity > 255 || intensity < 0) {
                qDebug() << "Nope, sorry > 255 || < 0: " << mag;
                qDebug() << "Mag: " << mag;
                qDebug() << "Intensity" << intensity;
                return;
            }

            // Select
            if(lookup->at(i) != -1 && intensity > 100) {
                select->push_back(lookup->at(i));
            }
/*
            if(intensity < 40) {
                intensity = 0;
            } else {
                intensity = 255;
            }
*/
            image_->setPixel(x, y, intensity);
        }
    }

    qDebug() << "Done";

    core_->us_->beginMacro("Experiment");
    core_->us_->push(new Select(cloud, select));
    core_->us_->endMacro();
    qDebug() << "Done1";
    image_container_->setPixmap(QPixmap::fromImage(*image_));
    qDebug() << "Done1.1";
    image_container_->resize(image_->size());
    qDebug() << "Done 2";

}

void VDepth::drawVector3f(boost::shared_ptr<const std::vector<Eigen::Vector3f> > out_img, boost::shared_ptr<PointCloud> cloud){

    // translates grid idx to cloud idx
    boost::shared_ptr<const std::vector<int>> lookup = cloud->gridToCloudMap();

    if(image_ != nullptr)
        delete image_;
    image_ = new QImage(cloud->scan_width(), cloud->scan_height(), QImage::Format_RGB32);

    // Draw image
    for(int y = 0; y < cloud->scan_height(); y++){
        for(int x = 0; x < cloud->scan_width(); x++){
            int i = (cloud->scan_height() -1 - y) + x * cloud->scan_height();

            // Mask disabled
            if(lookup->at(i) == -2) {
                image_->setPixel(x, y, 0);
                continue;
            }

            int r = (*out_img)[i][0] * 255;
            int g = (*out_img)[i][1] * 255;
            int b = (*out_img)[i][2] * 255;

            QRgb value = qRgb(r, g, b);

            image_->setPixel(x, y, value);
        }
    }


    image_container_->setPixmap(QPixmap::fromImage(*image_));
    image_container_->resize(image_->size());
}

// Kullback-Leibler distance

inline double KLDist(float * feature1, float * feature2, int size) {
    double kl = 0;

    for(int i = 0; i < size; i++){
        float p = feature1[i];
        float q = feature2[i];
        double tmp = p*log(p/q);

        if(tmp != tmp || tmp >= FLT_MAX || tmp <= FLT_MIN)
            continue;
        kl += tmp;
    }

    /*
    for(int i = 0; i < size; i++){
        std::cout << feature1[i] << "/" << feature2[i] << " ";
    }

    std::cout << std::endl << "feature:" << kl << std::endl;
    fflush(stdout);
    */
    return kl;
}

inline float EuclidianDist(float * feature1, float * feature2, int size) {
    float dist = 0;

    for(int i = 0; i < size; i++){
        dist += pow(feature1[i] - feature2[i], 2.0);
    }

    dist = sqrt(dist);

    if(dist != dist || dist >= FLT_MAX || dist <= FLT_MIN) {
        return 0;
    }

    return dist;
}

pcl::PointCloud<pcl::PointXYZINormal>::Ptr VDepth::gridDownsample(boost::shared_ptr<PointCloud> input, float resolution, std::vector<int>& sub_idxs) {
    pcl::PointCloud<pcl::Normal>::Ptr normals = ne_->getNormals(input);

    // HACK: Make sure normals are not NaN or inf
    for (pcl::Normal & n : *normals) {
      if (!pcl::isFinite<pcl::Normal>(n)){
          n.normal_x = 0;
          n.normal_y = 0;
          n.normal_z = 1;
      }
    }


    // Zipper up normals and xyzi
    pcl::PointCloud<pcl::PointXYZINormal>::Ptr cloud = zipNormals(*input, *normals);

    QTime t; t.start(); //qDebug() << "Timer started (Subsample)";
    /////////////////////////

    pcl::PointCloud<pcl::PointXYZINormal>::Ptr output(new pcl::PointCloud<pcl::PointXYZINormal>());
    sub_idxs.resize(input->size(), 0);

    pcl::VoxelGrid<pcl::PointXYZINormal> sor;
    sor.setInputCloud(cloud);
    sor.setLeafSize(resolution, resolution, resolution);
    sor.setDownsampleAllData (true);
    sor.setSaveLeafLayout(true);
    sor.filter(*output);


    for(uint i = 0; i < input->size(); i++) {
        pcl::PointXYZINormal &p = (*cloud)[i];
        int idx = sor.getCentroidIndex(p);
        sub_idxs[i] = idx;
    }


    /////////////////////////
    //qDebug() << "Output size: " << output->size() << ", orig size: " << input->size();
    time = t.elapsed();
    //qDebug() << "Subsample cloud in " << time << " ms";
    //qDebug() << "Enter to comntinue"; std::string enter; std::cin >> enter;
    //qDebug() << "Ok";

    return output;

}

pcl::PointCloud<pcl::PointXYZINormal>::Ptr VDepth::downsample(
        boost::shared_ptr<PointCloud> input,
        float resolution,
        std::vector<int>& sub_idxs){

    pcl::PointCloud<pcl::Normal>::Ptr normals = ne_->getNormals(input);

    // HACK: Make sure normals are not NaN or inf
    for (pcl::Normal & n : *normals) {
      if (!pcl::isFinite<pcl::Normal>(n)){
          n.getNormalVector3fMap() << 0, 0, 1;
      }
    }

    // Zipper up normals and xyzi
    pcl::PointCloud<pcl::PointXYZINormal>::Ptr cloud = zipNormals(*input, *normals);

    pcl::PointCloud<pcl::PointXYZINormal>::Ptr output = octreeDownsample(cloud.get(), resolution, sub_idxs);

    return output;

}


template<typename PointT, typename NormalT>
pcl::PointCloud<pcl::Normal>::Ptr don(pcl::PointCloud<PointT> & cloud,
            pcl::PointCloud<NormalT> & normals, float radius1 = 0.2,
            float radius2 = 0.5){

    pcl::PointCloud<pcl::Normal>::Ptr donormals;
    donormals.reset(new pcl::PointCloud<pcl::Normal>());
    donormals->resize(cloud.size());

    typename pcl::search::Search<PointT>::Ptr tree;
    tree.reset (new pcl::search::KdTree<PointT> (false));

    typename pcl::PointCloud<PointT>::ConstPtr cptr(&cloud, boost::serialization::null_deleter());
    tree->setInputCloud(cptr);

    std::vector<int> idxs;
    std::vector<float> sq_dists;

    auto avg_normal = [] (std::vector<int> idxs, pcl::PointCloud<NormalT> & normals) {
        pcl::Normal sum;
        for(int idx : idxs) {
            sum.getNormalVector3fMap() += normals[idx].getNormalVector3fMap();
        }
        sum.getNormalVector3fMap() /= idxs.size();
        return sum;
    };

    QTime t;
    t.start();

    for(uint idx = 0; idx < cloud.size(); idx++){
        std::vector<float> rads{radius1, radius2};
        std::vector<pcl::Normal> avgs;
        for(float rad : rads){
            idxs.clear();
            sq_dists.clear();
            tree->radiusSearch(idx, rad, idxs, sq_dists);
            idxs.push_back(idx);
            avgs.push_back(avg_normal(idxs, normals));
        }

        (*donormals)[idx].getNormalVector3fMap() = (avgs[0].getNormalVector3fMap() - avgs[1].getNormalVector3fMap())/2.0;
    }

    return donormals;
}

void VDepth::don_vis(){
    boost::shared_ptr<PointCloud> _cloud = core_->cl_->active_;
    if(_cloud == nullptr)
        return;

    int w = _cloud->scan_width();
    int h = _cloud->scan_height();

    pcl::PointCloud<pcl::Normal>::Ptr normals = ne_->getNormals(_cloud);

    float res1 = 0.5;
    float res2 = 1;

    // Downsample
    pcl::PointCloud<pcl::PointXYZINormal>::Ptr smaller_cloud;
    std::vector<int> sub_idxs;
    smaller_cloud = downsample(_cloud, res1, sub_idxs);

    pcl::PointCloud<pcl::Normal>::Ptr donormals = don(*smaller_cloud, *smaller_cloud, res1, res2);

    pcl::PointCloud<pcl::Normal> big_donormals;

    map((*donormals).points, big_donormals.points, sub_idxs);

    const std::vector<int> & cloudtogrid = _cloud->cloudToGridMap();

    boost::shared_ptr<std::vector<Eigen::Vector3f> > grid = boost::make_shared<std::vector<Eigen::Vector3f> >(w*h, Eigen::Vector3f(0.0f, 0.0f, 0.0f));
    for(uint i = 0; i < normals->size(); i++){
        int grid_idx = cloudtogrid[i];
        (*grid)[grid_idx] = big_donormals[i].getNormalVector3fMap();
    }

    drawVector3f(grid, _cloud);
}

void VDepth::hist_vis(){
    boost::shared_ptr<PointCloud> _cloud = core_->cl_->active_;
    if(_cloud == nullptr)
        return;

    pcl::PointCloud<pcl::PointXYZINormal>::Ptr filt_cloud;
    std::vector<int> sub_idxs;
    filt_cloud = downsample(_cloud, 0.01, sub_idxs);


    int bins = 20;
    float radius = 0.05;
    int max_nn = 0;


    boost::shared_ptr<std::vector<std::vector<float> > > hists = calcIntensityHist(*filt_cloud, bins, radius, max_nn);
    //boost::shared_ptr<std::vector<std::vector<float> > > hists = calcDistHist(*filt_cloud, bins, radius, max_nn);

    boost::shared_ptr<std::vector<std::vector<float> > > hists2 = boost::make_shared<std::vector<std::vector<float> > >(_cloud->size());

    // map back to cloud
    for(uint i = 0; i < _cloud->size(); i++) {
        int idx = sub_idxs[i];
        (*hists2)[i] = (*hists)[idx];
    }

    qDebug() << "Crash 1?";
    hists.reset();
    qDebug() << "Nope 1";

    boost::shared_ptr<const std::vector<int>> grid_to_cloud = _cloud->gridToCloudMap();

    int w = _cloud->scan_width();
    int h = _cloud->scan_height();

    // in the grid, subtract (x, y+1) from every (x, y)
    boost::shared_ptr<std::vector<float>> diffs = boost::make_shared<std::vector<float>>(w*h, 0.0f);

    //auto distfunc = EuclidianDist;
    auto distfunc = KLDist;

    /*
    // Lambda
    auto maxval = [] (float * array, int size) {
        float max = FLT_MIN;
        for(int i = 0; i < size; i++) {
            if(array[i] > max)
                max = array[i];
        }
        return max;
    };
    */

    const int feature_size = bins;

    for(int x = 1; x < w-1; x ++) {
        //qDebug() << "yes";
        for(int y = 1; y < h-1; y ++) {
            int center = (*grid_to_cloud)[x*h + y];
            int up = (*grid_to_cloud)[x*h + y-1];
            int down = (*grid_to_cloud)[x*h + y+1];
            int left = (*grid_to_cloud)[(x-1)*h + y];
            int right = (*grid_to_cloud)[(x+1)*h + y];


            (*diffs)[center] = 0;

            if(center == -1){
                continue;
            }

            // NB! assumed histograms are normalised

            if(up != -1 && down != -1){
                float * feature1 = &((*hists2)[up][0]);
                float * feature2 = &((*hists2)[down][0]);
                //float max1 = maxval(feature1, feature_size);
                //float max2 = maxval(feature2, feature_size);
                //(*diffs)[center] += fabs(max1 - max2);
                (*diffs)[center] += distfunc(feature1, feature2, feature_size);
            }

            if(left != -1 && right != -1){
                float * feature1 = &((*hists2)[left][0]);
                float * feature2 = &((*hists2)[right][0]);
                //float max1 = maxval(feature1, feature_size);
                //float max2 = maxval(feature2, feature_size);
                //(*diffs)[center] += fabs(max1 - max2);
                (*diffs)[center] += distfunc(feature1, feature2, feature_size);
            }

        }
    }

    boost::shared_ptr<const std::vector<float>> img = cloudToGrid(_cloud->cloudToGridMap(), w*h, diffs);

    qDebug() << "about to draw";
    drawFloats(img, _cloud);
    qDebug() << "Crash 2?";
    hists2.reset();
    qDebug() << "Nope 2";

}

void VDepth::fpfh_vis(){
    boost::shared_ptr<PointCloud> _cloud = core_->cl_->active_;
    if(_cloud == nullptr)
        return;

    pcl::PointCloud<pcl::PointXYZINormal>::Ptr filt_cloud;
    std::vector<int> sub_idxs;
    filt_cloud = downsample(_cloud, 0.1, sub_idxs);


    // FPFH
    QTime t; t.start(); qDebug() << "Timer started (FPFH)";
    pcl::FPFHEstimation<pcl::PointXYZINormal, pcl::PointXYZINormal, pcl::FPFHSignature33> fpfh;
    fpfh.setInputCloud(filt_cloud);
    fpfh.setInputNormals(filt_cloud);
    pcl::search::KdTree<pcl::PointXYZINormal>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZINormal>);
    fpfh.setSearchMethod(tree);
    fpfh.setRadiusSearch(0.20);
    pcl::PointCloud<pcl::FPFHSignature33>::Ptr fpfhs (new pcl::PointCloud<pcl::FPFHSignature33> ());
    fpfh.compute(*fpfhs);
    qDebug() << "FPFH in " << t.elapsed() << " ms";

    // Map feature to original cloud
    pcl::PointCloud<pcl::FPFHSignature33>::Ptr fpfhs2 (new pcl::PointCloud<pcl::FPFHSignature33> ());
    pcl::FPFHSignature33 blankfpfh;
    for(int i = 0; i < 33; i++){
        blankfpfh.histogram[i] = 0.0f;
    }
    fpfhs2->points.resize(_cloud->size(), blankfpfh);

    qDebug() << "resized";

    for(uint i = 0; i < _cloud->size(); i++) {
        uint idx = sub_idxs[i];

        if(idx != -1u && idx < fpfhs->size())
            (*fpfhs2)[i] = (*fpfhs)[idx];
    }

    boost::shared_ptr<const std::vector<int>> grid_to_cloud = _cloud->gridToCloudMap();

    int w = _cloud->scan_width();
    int h = _cloud->scan_height();

    // in the grid, subtract (x, y+1) from every (x, y)
    boost::shared_ptr<std::vector<float>> diffs = boost::make_shared<std::vector<float>>(_cloud->size(), 0.0f);


    auto distfunc = EuclidianDist;
/*
    auto maxval = [] (float * array, int size) {
        float max = FLT_MIN;
        for(int i = 0; i < size; i++) {
            if(array[i] > max)
                max = array[i];
        }
        return max;
    };
*/
    const int feature_size = 33;

    for(int x = 1; x < w-1; x ++) {
        for(int y = 1; y < h-1; y ++) {
            int center = (*grid_to_cloud)[x*h + y];
            int up = (*grid_to_cloud)[x*h + y-1];
            int down = (*grid_to_cloud)[x*h + y+1];
            int left = (*grid_to_cloud)[(x-1)*h + y];
            int right = (*grid_to_cloud)[(x+1)*h + y];


            (*diffs)[center] = 0;

            if(center == -1){
                continue;
            }

            // NB! assumed histograms are normalised

            if(up != -1 && down != -1){
                float * feature1 = (*fpfhs2)[up].histogram;
                float * feature2 = (*fpfhs2)[down].histogram;
                //float max1 = maxval(feature1, feature_size);
                //float max2 = maxval(feature2, feature_size);
                //(*diffs)[center] += fabs(max1 - max2);
                (*diffs)[center] += distfunc(feature1, feature2, feature_size);
            }

            if(left != -1 && right != -1){
                float * feature1 = (*fpfhs2)[up].histogram;
                float * feature2 = (*fpfhs2)[down].histogram;
                //float max1 = maxval(feature1, feature_size);
                //float max2 = maxval(feature2, feature_size);
                //(*diffs)[center] += fabs(max1 - max2);
                (*diffs)[center] += distfunc(feature1, feature2, feature_size);
            }

        }
    }

    boost::shared_ptr<const std::vector<float>> img = cloudToGrid(_cloud->cloudToGridMap(), w*h, diffs);

    qDebug() << "about to draw";
    drawFloats(img, _cloud);

}

void VDepth::curve_vis(){
    boost::shared_ptr<PointCloud> _cloud = core_->cl_->active_;
    if(_cloud == nullptr)
        return;

    pcl::PointCloud<pcl::PointXYZINormal>::Ptr filt_cloud;
    std::vector<int> big_to_small;
    filt_cloud = downsample(_cloud, 0.1, big_to_small);


    //pcl::io::savePCDFileASCII ("test_pcd.pcd", *filt_cloud);


    // Setup the principal curvatures computation
    pcl::PrincipalCurvaturesEstimation<pcl::PointXYZINormal, pcl::PointXYZINormal, pcl::PrincipalCurvatures> principal_curvatures_estimation;

    principal_curvatures_estimation.setInputCloud (filt_cloud);
    principal_curvatures_estimation.setInputNormals (filt_cloud);

    pcl::search::KdTree<pcl::PointXYZINormal>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZINormal>);
    principal_curvatures_estimation.setSearchMethod (tree);
    principal_curvatures_estimation.setRadiusSearch (0.5);

    // Actually compute the principal curvatures
    pcl::PointCloud<pcl::PrincipalCurvatures>::Ptr principal_curvatures (new pcl::PointCloud<pcl::PrincipalCurvatures> ());
    principal_curvatures_estimation.compute (*principal_curvatures);


    int w = _cloud->scan_width();
    int h = _cloud->scan_height();

    boost::shared_ptr<std::vector<float>> grid = boost::make_shared<std::vector<float>>(w*h, 0.0f);
    const std::vector<int> & cloudtogrid = _cloud->cloudToGridMap();

    for(uint big_idx = 0; big_idx < _cloud->size(); big_idx++) {
        int small_idx = big_to_small[big_idx];
        int grid_idx = cloudtogrid[big_idx];
        pcl::PrincipalCurvatures & pc = (*principal_curvatures)[small_idx];
        (*grid)[grid_idx] = pc.pc1 + pc.pc2;
    }


    //boost::shared_ptr<const std::vector<float>> img = cloudToGrid(cloudtogrid, w*h, grid);

    qDebug() << "about to draw";
    drawFloats(grid, _cloud);

}

void VDepth::curve_diff_vis(){
    boost::shared_ptr<PointCloud> _cloud = core_->cl_->active_;
    if(_cloud == nullptr)
        return;

    pcl::PointCloud<pcl::PointXYZINormal>::Ptr filt_cloud;
    std::vector<int> sub_idxs;
    filt_cloud = downsample(_cloud, 0.1, sub_idxs);


    //pcl::io::savePCDFileASCII ("test_pcd.pcd", *filt_cloud);


    // Setup the principal curvatures computation
    pcl::PrincipalCurvaturesEstimation<pcl::PointXYZINormal, pcl::PointXYZINormal, pcl::PrincipalCurvatures> principal_curvatures_estimation;

    principal_curvatures_estimation.setInputCloud (filt_cloud);
    principal_curvatures_estimation.setInputNormals (filt_cloud);

    pcl::search::KdTree<pcl::PointXYZINormal>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZINormal>);
    principal_curvatures_estimation.setSearchMethod (tree);
    principal_curvatures_estimation.setRadiusSearch (0.5);

    // Actually compute the principal curvatures
    pcl::PointCloud<pcl::PrincipalCurvatures>::Ptr principal_curvatures (new pcl::PointCloud<pcl::PrincipalCurvatures> ());
    principal_curvatures_estimation.compute (*principal_curvatures);

    std::cout << "output points.size (): " << principal_curvatures->points.size () << std::endl;


    // Filter out NaNs
    int nans = 0;
    for(pcl::PrincipalCurvatures & pc : principal_curvatures->points) {
        if(pc.principal_curvature_x != pc.principal_curvature_x){
            pc.principal_curvature_x = 0;
            pc.principal_curvature_y = 0;
            pc.principal_curvature_z = 0;
            nans++;
        }
    }

    qDebug() << "Nans" << nans;

    // Display and retrieve the shape context descriptor vector for the 0th point.
    pcl::PrincipalCurvatures descriptor = principal_curvatures->points[0];
    std::cout << descriptor << std::endl;


    pcl::PointCloud<pcl::PrincipalCurvatures>::Ptr principal_curvatures2 (new pcl::PointCloud<pcl::PrincipalCurvatures>());

    pcl::PrincipalCurvatures blank;
    blank.principal_curvature_x = 0;
    blank.principal_curvature_y = 0;
    blank.principal_curvature_z = 0;

    principal_curvatures2->points.resize(_cloud->size(), blank);


    for(uint i = 0; i < _cloud->size(); i++) {
        int idx = sub_idxs[i];
        (*principal_curvatures2)[i] = (*principal_curvatures)[idx];
    }

    boost::shared_ptr<const std::vector<int>> grid_to_cloud = _cloud->gridToCloudMap();

    int w = _cloud->scan_width();
    int h = _cloud->scan_height();

    // in the grid, subtract (x, y+1) from every (x, y)
    boost::shared_ptr<std::vector<float>> diffs = boost::make_shared<std::vector<float>>(w*h, 0.0f);


    //auto distfunc = EuclidianDist;

    auto maxval = [] (float * array, int size) {
        float max = FLT_MIN;
        for(int i = 0; i < size; i++) {
            if(array[i] > max)
                max = array[i];
        }
        return max;
    };

    const int feature_size = 3;

    for(int x = 1; x < w-1; x ++) {
        for(int y = 1; y < h-1; y ++) {
            int center = (*grid_to_cloud)[x*h + y];
            int up = (*grid_to_cloud)[x*h + y-1];
            int down = (*grid_to_cloud)[x*h + y+1];
            int left = (*grid_to_cloud)[(x-1)*h + y];
            int right = (*grid_to_cloud)[(x+1)*h + y];


            (*diffs)[center] = 0;

            if(center == -1){
                continue;
            }

            // NB! assumed histograms are normalised

            if(up != -1 && down != -1){
                float * feature1 = (*principal_curvatures2)[up].principal_curvature;
                float * feature2 = (*principal_curvatures2)[down].principal_curvature;
                float max1 = maxval(feature1, feature_size);
                float max2 = maxval(feature2, feature_size);
                (*diffs)[center] += fabs(max1 - max2);
                //(*diffs)[center] += distfunc(feature1, feature2, feature_size);
            }

            if(left != -1 && right != -1){
                float * feature1 = (*principal_curvatures2)[left].principal_curvature;
                float * feature2 = (*principal_curvatures2)[right].principal_curvature;
                float max1 = maxval(feature1, feature_size);
                float max2 = maxval(feature2, feature_size);
                (*diffs)[center] += fabs(max1 - max2);
                //(*diffs)[center] += distfunc(feature1, feature2, feature_size);
            }

        }
    }

    boost::shared_ptr<const std::vector<float>> img = cloudToGrid(_cloud->cloudToGridMap(), w*h, diffs);

    qDebug() << "about to draw";
    drawFloats(img, _cloud);

}

void VDepth::normalnoise(){
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    int h = cloud->scan_width();
    int w = cloud->scan_height();

    pcl::PointCloud<pcl::Normal>::Ptr normals = ne_->getNormals(cloud);

    boost::shared_ptr<std::vector<float>> stdev = normal_stdev(cloud, normals, 1, 100);

    boost::shared_ptr<const std::vector<float>> img = cloudToGrid(cloud->cloudToGridMap(), w*h, stdev);

    drawFloats(img, cloud);
}


void VDepth::dist_stdev(){
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    int h = cloud->scan_width();
    int w = cloud->scan_height();

    boost::shared_ptr<std::vector<float> > stdev = stdev_dist(cloud, 0.05f, 20, false);

    boost::shared_ptr<const std::vector<float>> img = cloudToGrid(cloud->cloudToGridMap(), w*h, stdev);

    drawFloats(img, cloud);
}

void VDepth::sutract_lowfreq_noise(){
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    int h = cloud->scan_width();
    int w = cloud->scan_height();

    // Create distance map
    boost::shared_ptr<std::vector<float>> distmap = makeDistmap(cloud);

    //distmap = interpolate(distmap, w, h, 21);

    boost::shared_ptr<std::vector<float> > smooth_grad_image = convolve(distmap, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);

    boost::shared_ptr<std::vector<float>> highfreq = distmap;

    for(uint i = 0; i < distmap->size(); i++){
        (*highfreq)[i] = (*distmap)[i] - (*smooth_grad_image)[i];
    }

    drawFloats(highfreq, cloud);
}

void VDepth::eigen_ratio(){
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    int h = cloud->scan_width();
    int w = cloud->scan_height();



    // Downsample
    std::vector<int> sub_idxs;
    pcl::PointCloud<pcl::PointXYZI>::Ptr smaller_cloud = octreeDownsample(cloud.get(), 0.1, sub_idxs);


    boost::shared_ptr<std::vector<Eigen::Vector3f> > pca = getPCA(smaller_cloud.get(), 0.3f, 0);

    boost::shared_ptr<std::vector<float>> likely_veg =
               boost::make_shared<std::vector<float>>(pca->size(), 0.0f);

    for(uint i = 0; i < pca->size(); i++) {
        Eigen::Vector3f eig = (*pca)[i];

        // Not enough neighbours
        if(eig[1] < eig[2]) {
            (*likely_veg)[i] = 0;
            continue;
        }

        /*
        float eig_sum = eig.sum();
        eig /= eig_sum;
        float fudge_factor = 5.0f;

        bool is_planar = eig[1] < 0.05 * fudge_factor || eig[2] < 0.01 * fudge_factor;

        if(!is_planar) {
            (*likely_veg)[i] = 0;
        } else {
            (*likely_veg)[i] = 1;
        }

        */

        float curvature;
        if(eig[0] > eig[2])
            curvature = eig[1] / (eig[0]);
        else
            curvature = 1;


        (*likely_veg)[i] = curvature;

    }

    /*
    // 2nd check
    pca = getPCA(smaller_cloud.get(), 0.1f, 0);

    for(uint i = 0; i < pca->size(); i++) {
        if((*likely_veg)[i] == 0)
            continue;

        Eigen::Vector3f eig = (*pca)[i];

        // Not enough neighbours
        if(eig[1] < eig[2]) {
            (*likely_veg)[i] = 0;
            continue;
        }

        float eig_sum = eig.sum();

        eig /= eig_sum;

        float fudge_factor = 5.0f;
        if(eig[1] < 0.05 * fudge_factor || eig[2] < 0.01 * fudge_factor) {
            (*likely_veg)[i] = 0;
        } else {
            (*likely_veg)[i] = 1;
        }

    }
    */


    boost::shared_ptr<std::vector<float>> likely_veg2 =
               boost::make_shared<std::vector<float>>(cloud->size(), 0.0f);


    for(uint i = 0; i < cloud->size(); i++) {
        uint subidx = sub_idxs[i];
        (*likely_veg2)[i] = (*likely_veg)[subidx];
    }


    boost::shared_ptr<const std::vector<float>> img = cloudToGrid(cloud->cloudToGridMap(), w*h, likely_veg2);

    drawFloats(img, cloud);
}

void VDepth::pca(){
    qDebug() << "Myfunc";
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    int h = cloud->scan_width();
    int w = cloud->scan_height();

    boost::shared_ptr<std::vector<Eigen::Vector3f> > pca = getPCA(cloud.get(), 0.05f, 20);

    boost::shared_ptr<std::vector<float>> plane_likelyhood =
               boost::make_shared<std::vector<float>>(pca->size(), 0.0f);

    Eigen::Vector3f ideal_plane(1.0f, 0.0f, 0.0f);
    ideal_plane.normalize();

    for(uint i = 0; i < pca->size(); i++) {
        Eigen::Vector3f & val = (*pca)[i];

        // Not enough neighbours
        if(val[1] < val[2]) {
            (*plane_likelyhood)[i] = 0;
            continue;
        }

        float similarity = cosine(val, ideal_plane);
        (*plane_likelyhood)[i] = similarity;
    }
/*
    boost::shared_ptr<std::vector<Eigen::Vector3f> > grid = boost::make_shared<std::vector<Eigen::Vector3f> >(grid_to_cloud->size(), Eigen::Vector3f(0.0f, 0.0f, 0.0f));
    for(int i = 0; i < grid_to_cloud->size(); i++) {
        int idx = (*grid_to_cloud)[i];
        if(idx != -1)
            (*grid)[i] = (*pca)[idx];
    }

    drawVector3f(grid, cloud);
*/

    boost::shared_ptr<const std::vector<float>> img = cloudToGrid(cloud->cloudToGridMap(), w*h, plane_likelyhood);

    drawFloats(img, cloud);
}

void VDepth::sobel_erode(){
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    int h = cloud->scan_width();
    int w = cloud->scan_height();

    // Create distance map
    boost::shared_ptr<std::vector<float>> distmap = makeDistmap(cloud);

    boost::shared_ptr<std::vector<float> > grad_image = gradientImage(distmap, w, h);
    boost::shared_ptr<std::vector<float> > smooth_grad_image = convolve(grad_image, w, h, gaussian, 5);

    // Threshold && Erode

    const int strct[] = {
        0, 1, 0,
        1, 0, 1,
        0, 1, 0,
    };

    boost::shared_ptr<std::vector<float> > dilated_image =  morphology(
            smooth_grad_image,
            w, h, strct, 3, Morphology::ERODE,
            grad_image); // <-- reuse

    drawFloats(dilated_image, cloud);
}

void VDepth::sobel_blur(){
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    int h = cloud->scan_width();
    int w = cloud->scan_height();

    // Create distance map
    boost::shared_ptr<std::vector<float>> distmap = makeDistmap(cloud);

    boost::shared_ptr<std::vector<float> > smooth_grad_image = gradientImage(distmap, w, h);
    int blurs = 0;
    for(int i = 0; i < blurs; i++)
        smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);

    drawFloats(smooth_grad_image, cloud);
}

void VDepth::intensity_play() {
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    int h = cloud->scan_width();
    int w = cloud->scan_height();

    boost::shared_ptr<std::vector<float>> intensity = boost::make_shared<std::vector<float>>(cloud->size());

    // Create intensity cloud
    for(uint i = 0; i < intensity->size(); i++){
        (*intensity)[i] = (*cloud)[i].intensity;
    }

    boost::shared_ptr<std::vector<float>> img = cloudToGrid(cloud->cloudToGridMap(), w*h, intensity);


    boost::shared_ptr<std::vector<float> > smooth_grad_image = convolve(img, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);
/*
    boost::shared_ptr<std::vector<float>> highfreq = img;

    for(int i = 0; i < highfreq->size(); i++){
        (*highfreq)[i] = (*highfreq)[i] - (*smooth_grad_image)[i];
    }

    drawFloats(highfreq, cloud);
*/
    drawFloats(smooth_grad_image, cloud);

}


void VDepth::myFunc(){
    qDebug() << "Myfunc";
    boost::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;
    //int h = cloud->scan_width();
    //int w = cloud->scan_height();

    boost::shared_ptr<const std::vector<int>> grid_to_cloud = cloud->gridToCloudMap();

    boost::shared_ptr<std::vector<Eigen::Vector3f> > pca = getPCA(cloud.get(), 1.0f, 50);

    boost::shared_ptr<std::vector<Eigen::Vector3f> > grid = boost::make_shared<std::vector<Eigen::Vector3f> >(grid_to_cloud->size(), Eigen::Vector3f(0.0f, 0.0f, 0.0f));
    for(uint i = 0; i < grid_to_cloud->size(); i++) {
        int idx = (*grid_to_cloud)[i];
        if(idx != -1)
            (*grid)[i] = (*pca)[idx];
    }

    //boost::shared_ptr<std::vector<float> > stdev = stdev_depth(cloud, 1.0f);
    //boost::shared_ptr<const std::vector<float>> img = cloudToGrid(cloud->cloudToGridMap(), w*h, stdev);


    /*
    // Create distance map
    boost::shared_ptr<std::vector<float>> distmap = makeDistmap(cloud);
    //distmap = interpolate(distmap, w, h, 21);

    boost::shared_ptr<std::vector<float> > smooth_grad_image = convolve(distmap, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);
    smooth_grad_image = convolve(smooth_grad_image, w, h, gaussian, 5);

    boost::shared_ptr<std::vector<float>> highfreq = distmap;

    for(int i = 0; i < distmap->size(); i++){
        (*highfreq)[i] = (*distmap)[i] - (*smooth_grad_image)[i];
    }

    //boost::shared_ptr<std::vector<float> > grad_image = gradientImage(distmap, w, h, size);
    boost::shared_ptr<std::vector<float> > int_image = interpolate(distmap, w, h, 50);
    boost::shared_ptr<std::vector<float> > stdev_image = stdev(int_image, w, h, 5);


    boost::shared_ptr<std::vector<float> > grad_image = gradientImage(distmap, w, h, size);
    boost::shared_ptr<std::vector<float> > smooth_grad_image = convolve(grad_image, w, h, gaussian, 5);

    // Threshold && Erode

    const int strct[] = {
        0, 1, 0,
        1, 0, 1,
        0, 1, 0,
    };

    boost::shared_ptr<std::vector<float> > dilated_image =  morphology(
            smooth_grad_image,
            w, h, strct, 3, Morphology::ERODE,
            grad_image); // <-- reuse
*/


    ///////// OUTPUT //////////

    //drawFloats(img, cloud);

    drawVector3f(grid, cloud);

}

Q_PLUGIN_METADATA(IID "za.co.circlingthesun.cloudclean.iplugin")
