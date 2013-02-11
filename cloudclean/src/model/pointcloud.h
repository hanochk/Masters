#ifndef MODEL_CPOINTCLOUD_H_
#define MODEL_CPOINTCLOUD_H_

#include <mutex>
#include <QObject>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>


class EventDispatcher : public QObject{
    Q_OBJECT
 public:
    void updateProgress(int value);

 signals:
   void progress(int percentage);
};

enum class PointFlags : int8_t {
    selected = 0x001,
    reserved1 = 0x002,
    reserved2 = 0x006,
    reserved3 = 0x008,
    reserved4 = 0x010,
    reserved5 = 0x020,
    reserved6 = 0x040,
};

class PointCloud : public pcl::PointCloud<pcl::PointXYZI> {
 public:
    explicit PointCloud();
    bool save_ptx(const char* filename);
    bool load_ptx(const char* filename, int subsample = 1);

 public:
    std::shared_ptr<EventDispatcher> ed_;
    std::shared_ptr<std::mutex> pc_mutex;
    std::vector<int> cloud_to_grid_map_;
    int scan_width_;
    int scan_height_;
    bool cloud_dirty_;
    bool labels_dirty_;
    bool flags_dirty_;
    std::vector<int16_t> labels_;
    std::vector<PointFlags> flags_;

    // What needs to be in here?
    // Should normals be a default requirement
    // Maybe store attributes in a map and generate on the fly
};

#endif // MODEL_CPOINTCLOUD_H_
