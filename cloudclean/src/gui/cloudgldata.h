#ifndef CLOUDGLDATA_H
#define CLOUDGLDATA_H

#include "glheaders.h"
#include "model/pointcloud.h"
#include <QObject>
#include <QGLBuffer>
#include "gui/export.h"

class GUI_DLLSPEC CloudGLData : public QObject{
    Q_OBJECT
 public:
    CloudGLData(std::shared_ptr<PointCloud> pc);
    ~CloudGLData();

    void setVAO(GLuint vao);

    void draw(GLint vao);

    void copyCloud();
    void copyLabels();
    void copyFlags();
    void copyGrid();
    // TOD0: enableGrid()

 public slots:
    void syncCloud();
    void syncLabels(std::shared_ptr<std::vector<int> > idxs = nullptr);
    void syncFlags(std::shared_ptr<std::vector<int> > idxs = nullptr);

 public:
    std::shared_ptr<PointCloud> pc_;
    std::unique_ptr<QGLBuffer> label_buffer_;
    std::unique_ptr<QGLBuffer> point_buffer_;
    std::unique_ptr<QGLBuffer> flag_buffer_;
    std::unique_ptr<QGLBuffer> grid_buffer_;

 private:
    bool dirty_labels_;
    bool dirty_points_;
    bool dirty_flags_;
    bool dirty_grid_;

    std::shared_ptr<std::vector<int> > dirty_label_list_;
    std::shared_ptr<std::vector<int> > dirty_point_list_;
    std::shared_ptr<std::vector<int> > dirty_flag_list_;
    std::shared_ptr<std::vector<int> > dirty_grid_list_;
};


#endif // CLOUDGLDATA_H
