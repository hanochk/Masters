#ifndef GLDATA_H
#define GLDATA_H

#include "glheaders.h"
#include <memory>
#include <mutex>
#include <QObject>
#include <QGLBuffer>
#include "model/pointcloud.h"
#include "gui/cloudgldata.h"
#include "model/cloudlist.h"
#include "model/layerlist.h"

class GLData : public QObject {
    Q_OBJECT
 public:
    explicit GLData(QGLContext * glcontext,
                    std::shared_ptr<CloudList> &cl,
                    std::shared_ptr<LayerList> &ll,
                    QObject *parent = 0);
 signals:
    void update();
    
 public slots:
    void reloadCloud(std::shared_ptr<PointCloud> cloud);
    void reloadColorLookupBuffer();
    
 public:
    std::shared_ptr<QGLBuffer> color_lookup_buffer_;
    std::map<std::shared_ptr<PointCloud>, std::shared_ptr<CloudGLData> > cloudgldata_;
    float selection_color_[4];


 private:
    std::shared_ptr<CloudList> cl_;
    std::shared_ptr<LayerList> ll_;
    QGLContext * glcontext_;
    std::mutex * clb_mutex_;
};

#endif // GLDATA_H