#include "plugins/markov/markov.h"
#include <QDebug>
#include <QAction>
#include <QToolBar>
#include <QMessageBox>
#include <functional>
#include <boost/serialization/shared_ptr.hpp>
#include <pcl/search/kdtree.h>
#include "model/layerlist.h"
#include "model/cloudlist.h"
#include "gui/glwidget.h"
#include "gui/flatview.h"
#include "gui/mainwindow.h"
#include "commands/select.h"
#include "pluginsystem/core.h"
#include "plugins/markov/mincut.h"
#include "utilities/filters.h"
#include "utilities/picker.h"

QString Markov::getName(){
    return "markov";
}

void Markov::initialize(Core *core){
    core_= core;
    cl_ = core_->cl_;
    ll_ = core_->ll_;
    glwidget_ = core_->mw_->glwidget_;
    flatview_ = core_->mw_->flatview_;
    mw_ = core_->mw_;

    enable_ = new QAction(QIcon(":/markov.png"), "markov action", 0);
    enable_->setCheckable(true);

    connect(enable_,&QAction::triggered, [this] (bool on) {
        qDebug() << "Click!";
    });

    connect(enable_, SIGNAL(triggered()), this, SLOT(enable()));

    mw_->toolbar_->addAction(enable_);
    std::function<void(int)> func = std::bind(&Markov::graphcut, this, std::placeholders::_1);
    picker_ = new Picker(glwidget_, cl_, func);
    enabled_ = false;
    fg_idx_ = -1;
}

void Markov::cleanup(){
    mw_->toolbar_->removeAction(enable_);
    delete enable_;
    delete picker_;
}

Markov::~Markov(){
    qDebug() << "Markov deleted";
}

void Markov::enable() {
    if(enabled_){
        disable();
        return;
    }

    QTabWidget * tabs = qobject_cast<QTabWidget *>(glwidget_->parent()->parent());
    tabs->setCurrentWidget(glwidget_);
    enable_->setChecked(true);

    emit enabling();

    glwidget_->installEventFilter(picker_);
    connect(core_, SIGNAL(endEdit()), this, SLOT(disable()));

    enabled_ = true;
    // Let the user know what to do
    QMessageBox::information(nullptr, tr("Select foreground"),
                    tr("Select the center of an object..."),
                    QMessageBox::Ok, QMessageBox::Ok);


}

void Markov::disable() {
    enable_->setChecked(false);
    disconnect(core_, SIGNAL(endEdit()), this, SLOT(disable()));
    glwidget_->removeEventFilter(picker_);
    enabled_ = false;
}

void Markov::graphcut(int idx){
    qDebug() << "Myfunc";

    if(fg_idx_ = -1) {
        fg_idx_ = idx;
        QMessageBox::information(nullptr, tr("Select background"),
                        tr("Select background"),
                        QMessageBox::Ok, QMessageBox::Ok);
    }

    std::shared_ptr<PointCloud> cloud = core_->cl_->active_;
    if(cloud == nullptr)
        return;

    // Downsample
    pcl::PointCloud<pcl::PointXYZI>::Ptr smaller_cloud;
    std::vector<int> big_to_small_map;
    smaller_cloud = octreeDownsample(cloud.get(), 0.1, big_to_small_map);


    MinCut mc;
    mc.setInputCloud(smaller_cloud);

    pcl::PointCloud<pcl::PointXYZI>::Ptr foreground_points(new pcl::PointCloud<pcl::PointXYZI> ());
    foreground_points->points.push_back(cl_->active_->points[fg_idx_]);


    pcl::PointCloud<pcl::PointXYZI>::Ptr background_points(new pcl::PointCloud<pcl::PointXYZI> ());
    background_points->points.push_back(cl_->active_->points[idx]);

    double radius = 3.0;
    double sigma = 0.25;
    int neigbours = 10;
    double source_weight = 0.8;

    mc.setForegroundPoints (foreground_points);
    mc.setBackgroundPoints(background_points);
    mc.setRadius (radius);
    mc.setSigma (sigma);
    mc.setNumberOfNeighbours (neigbours);
    mc.setSourceWeight (source_weight);

    std::vector<pcl::PointIndices> clusters;
    mc.extract (clusters);

    auto select = std::make_shared<std::vector<int>>();

    // GAH!!!! inefficient

    std::vector<bool> small_idxs_selected(smaller_cloud->size(), false);
    for(int idx : clusters[1].indices){
        small_idxs_selected[idx] = true;
    }

    for(int i = 0; i < big_to_small_map.size(); i++) {
        int idx = big_to_small_map[i];
        if(small_idxs_selected[idx]) {
            select->push_back(i);
        }
    }

    //std::copy(clusters[1].indices.begin(), clusters[1].indices.end(), select->begin());

    Select * selectCmd = new Select(cl_->active_, select);



    core_->us_->beginMacro("Markov min cut");
    core_->us_->push(selectCmd);
    core_->us_->endMacro();
    core_->cl_->updated();
    core_->mw_->stopBgAction("Cut completed.");


    // We need a layer for the region of interest

    // Need a layer for the pixels pinned to the fg

    // Need a layer for the pixels pinned to the bg
    fg_idx_ = -1;
    disable();
}

Q_PLUGIN_METADATA(IID "za.co.circlingthesun.cloudclean.iplugin")