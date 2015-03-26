#ifndef EVALUATE_H
#define EVALUATE_H

#include "pluginsystem/iplugin.h"
#include <set>
#include <boost/weak_ptr.hpp>
#include "pluginsystem/iplugin.h"
class QAction;
class QWidget;
class Core;
class CloudList;
class LayerList;
class FlatView;
class GLWidget;
class MainWindow;
class Layer;
class QLineEdit;

class Evaluate : public IPlugin {
    Q_INTERFACES(IPlugin)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "za.co.circlingthesun.cloudclean.evaluate" FILE "evaluate.json")
 public:
    QString getName();
    void initialize(Core * core);
    void cleanup();
    ~Evaluate();

 signals:
   void enabling();

 private slots:
    void enable();
    void disable();
    void eval();

 private:
    std::tuple<std::vector<int>, std::vector<int>> get_false_selections(std::vector<int> & world_idxs, std::vector<bool> & target_mask);
    std::vector<std::vector<int> > cluster(std::vector<int> & idxs);
    std::vector<int> concaveHull(std::vector<int> & idxs);

 private:
    Core * core_;
    CloudList * cl_;
    LayerList * ll_;
    GLWidget * glwidget_;
    FlatView * flatview_;
    MainWindow * mw_;

    QAction * enable_;
    QWidget * settings_;
    bool is_enabled_;

    QLineEdit * precision_;
    QLineEdit * recall_;

    std::vector<boost::weak_ptr<Layer> >  world_layers_;
    std::vector<boost::weak_ptr<Layer> >  target_layers_;
    std::vector<boost::weak_ptr<Layer> >  selection_layers_;
};

#endif  // EVALUATE_H
