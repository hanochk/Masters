#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <ctime>

#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#include <GL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include <CL/cl_gl.h>

#include <QGLWidget>
#include <QGLBuffer>
#include <QGLShaderProgram>
#include <QMouseEvent>

#include "appdata.h"
#include "MousePoles.h"

#include <GL/glx.h>
#undef KeyPress // Defined in X11/X.h, interferes with QEvent::KeyPress

class GLWidget : public QGLWidget
{
    Q_OBJECT
public:
    GLWidget( QWidget* parent = 0 );

protected:
    virtual void initializeGL();
    virtual void resizeGL( int w, int h );
    virtual void paintGL();

    void clickity(int x, int y);

    void mouseDoubleClickEvent ( QMouseEvent * event );
    void mouseMoveEvent ( QMouseEvent * event );
    void mousePressEvent ( QMouseEvent * event );
    void mouseReleaseEvent ( QMouseEvent * event );
    void wheelEvent ( QWheelEvent * event );
    void keyPressEvent ( QKeyEvent * event );
    bool eventFilter(QObject *object, QEvent *event);

public slots:
    void reloadCloud();

private:
    bool prepareShaderProgram( const QString& vertexShaderPath,
                               const QString& fragmentShaderPath );

    AppData *               app_data;

    QGLFormat               glFormat;
    QGLShaderProgram        m_shader;
    QGLBuffer               m_vertexBuffer;


    cl_platform_id          platform;         
    cl_device_id            device;
    cl_context              context;
    cl_command_queue        cmd_queue;
    cl_program              program;
    cl_kernel               kernel;
    size_t                  kernelsize;
    
    cl_mem                  p_vbocl;

    float   anim;

    glm::mat4               cameraToClipMatrix;
    glm::mat4               modelview_mat;
    glm::vec4               offsetVec;
    float                   aspectRatio;

    Qt::MouseButton         mouseDown;
    bool                    moved;
    int                     start_x;
    int                     start_y;

    //TODO: Move out of here
    bool                                sampling;
    bool                                filling;
    int                                 vals_in_range;
    std::vector<pcl::FPFHSignature33>   stats;
    pcl::FPFHSignature33                mean;
    pcl::FPFHSignature33                stdev;

    boost::shared_ptr<glutil::ViewPole>     viewPole;
    boost::shared_ptr<glutil::ObjectPole>   objtPole;
};

#endif // GLWIDGET_H
