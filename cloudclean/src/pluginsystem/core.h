#ifndef CORE_H
#define CORE_H

#include <QObject>
#include "pluginsystem/export.h"

class CloudList;
class LayerList;
class MainWindow;
class QUndoStack;

class DLLSPEC Core : public QObject{
    Q_OBJECT
 public:
    Core();
    ~Core();

 signals:
    void endEdit();

 public:
    CloudList * cl_;
    LayerList * ll_;
    MainWindow * mw_;
    QUndoStack * us_;
};

#endif // CORE_H