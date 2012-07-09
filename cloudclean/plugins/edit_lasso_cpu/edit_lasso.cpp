#define GL3_PROTOTYPES
#include <../../external/gl3.h>
#include "edit_lasso.h"
#include <QIcon>
#include <QDebug>
#include <QResource>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "utilities.h"

EditLasso::EditLasso()
{
    lasso_active = false;
    editLasso = new QAction(QIcon(":/images/lasso.png"), "Lasso select (CPU)", this);
    actionList << editLasso;
    foreach(QAction *editAction, actionList)
        editAction->setCheckable(true);

}

EditLasso::~EditLasso()
{
}


void EditLasso::moveLasso(Eigen::Vector2f point){
    if(!lasso.empty() && lasso_active){
        lasso.back() = point;
    }
}

void EditLasso::addLassoPoint(Eigen::Vector2f point){

    if(!lasso_active)
        return;

    if(lasso.empty()){
        lasso.push_back(point);
    }
    else{
        Eigen::Vector2f end = lasso.back();
        lasso.back() = point;
        lasso.push_back(end);
    }

}

/////// CL KERNEL STUFF

struct float2{
    float x, y;
};

int rand(int value)
{
    const int a = 1103515245;
    const int c = 12345;

    return (a*value) + c;
}

float cross2D(float2 lineA, float2 lineB, float2 other)
{
    float2 dA, dB;
    dA.x = lineA.x - other.x; dA.y = lineA.y - other.y;
    dB.x = lineB.x - other.x; dB.y = lineB.y - other.y;

    return dA.x*dB.y - dA.y*dB.x;
}

int side(float a)
{
    if(a < -1e-6)
        return -1;
    if(a > 1e-6)
        return 1;
    return 0;
}

bool oppositeSides(float2 lineA, float2 lineB, float2 pointC, float2 pointD)
{
    float crossC = cross2D(lineA, lineB, pointC);
    float crossD = cross2D(lineA, lineB, pointD);

    int sideC = side(crossC);
    int sideD = side(crossD);

    return sideC != sideD;
}

float pointDistance(float2 pointA, float2 pointB)
{
    float dx = pointA.x - pointB.x;
    float dy = pointA.y - pointB.y;

    return sqrt(dx*dx + dy*dy);
}

bool pointOnLineSegment(float2 lineA, float2 lineB, float2 pointC)
{
    float lineLength = pointDistance(lineA, lineB);
    float viaPoint = pointDistance(lineA, pointC) + pointDistance(lineB, pointC);

    return fabs(viaPoint - lineLength) < 1e-6;
}

bool intersects(float2 lineA, float2 lineB, float2 lineC, float2 lineD)
{
    if(oppositeSides(lineA, lineB, lineC, lineD) && oppositeSides(lineC, lineD, lineA, lineB))
        return true; /// Lines intersect in obvious manner

    /// Line segments either don't intersect or are parallel
    /*if(pointOnLineSegment(lineA, lineB, lineC) || pointOnLineSegment(lineA, lineB, lineD) ||
       pointOnLineSegment(lineC, lineD, lineA) || pointOnLineSegment(lineC, lineD, lineB))
        return true;
    */
    return false;
}

float randomAngle(int* lastRandom)
{
    *lastRandom = rand(*lastRandom);
    return 2.0f*M_PI*(*lastRandom % 10000)/10000.0f;
}

float2 randomLineSegment(float2 origin, int* lastRandom)
{
    float angle = randomAngle(lastRandom);
    float2 endPoint;
    endPoint.x = 10000.0f*cos(angle) + origin.x;
    endPoint.y = 10000.0f*sin(angle) + origin.y;
    return endPoint;
}

bool pointInsidePolygon(float2* polygon, int n, float2 point)
{
    int lastRandom = rand(1);

    while(true)
    {
        float2 endPoint = randomLineSegment(point, &lastRandom);

        for(int i = 0; i < n; ++i)
            if(pointOnLineSegment(point, endPoint, polygon[i]))
                continue;

        int hits = 0;

         //qDebug("CHECK!");

        for(int i = 0; i < n; ++i){

            //qDebug("POLY: (%f, %f) -- (%f, %f), (%f, %f) -- (%f, %f)", polygon[i].x, polygon[i].y, polygon[(i + 1) % n].x, polygon[(i + 1) % n].y, point.x, point.y, endPoint.x, endPoint.y);

            if(intersects(polygon[i], polygon[(i + 1) % n], point, endPoint)){
                ++hits;
                //qDebug("HIT %d!", hits);
            }
        }

        if(hits % 2 == 1){
            //qDebug("Point (%f, %f) is inside. Hits = %d", point.x, point.y, hits);
        }

        return (hits % 2 == 1);
    }
}

//// END CL KERNEL STUFF


void EditLasso::lassoToLayer(CloudModel *cm, GLArea *glarea){

    cm->layerList.newLayer();

    std::vector<Layer> & layers = cm->layerList.layers;

    QGLBuffer & dest = layers[layers.size()-1].gl_index_buffer;
    dest.create();
    dest.setUsagePattern( QGLBuffer::DynamicDraw );
    dest.bind();
    dest.allocate(cm->cloud->size() * sizeof(int) );
    dest.release();

    /// Create and read source index from gpu
    int * dest_indices = new int[cm->cloud->size()];
    int * source_indices = new int[cm->cloud->size()];

    QGLBuffer & source = layers[0].gl_index_buffer;
    source.bind(); /// Bind source
    source.read(0, source_indices, source.size());
    source.release();

    // Create lasso
    int lasso_size = lasso.size();
    float2 lasso_data[lasso_size];
    for(int i = 0; i< lasso_size; i++){
        lasso_data[i].x = lasso[i].x();
        lasso_data[i].y = lasso[i].y();
    }

    /// Perform the lasso selection
    glm::mat4 gmat = glarea->cameraToClipMatrix * glarea->modelview_mat;

    /// for each point
    for(unsigned int i = 0; i < cm->cloud->size(); i++){
        /// get point
        int idx = i;
        pcl::PointXYZI p = cm->cloud->points[idx];
        float point[4] = {p.x, p.y, p.z, p.intensity};

        /// project point
        proj(glm::value_ptr(gmat), point);

        /// make 2d
        float2 vertex = {point[0], point[1]};

        /// do lasso test
        bool in_lasso = pointInsidePolygon(lasso_data, lasso_size, vertex);

        if(in_lasso){
            dest_indices[idx] = source_indices[idx];
            source_indices[idx] = -1;
        }
        else{
            dest_indices[idx] = -1;
        }

    }

    /// Write results gpu
    dest.bind();
    dest.write(0, dest_indices, dest.size());
    dest.release();

    source.bind();
    source.write(0, source_indices, source.size());
    source.release();


    delete [] source_indices;
    delete [] dest_indices;

    lasso.clear();
    fflush(stdout);
}


void EditLasso::paintGL(CloudModel *, GLArea * glarea){
    // Draw lasso shader
    glClear(GL_DEPTH_BUFFER_BIT );
    lasso_shader.bind();
    Eigen::Matrix4f ortho;
    ortho.setIdentity();

    glUniformMatrix4fv(lasso_shader.uniformLocation("ortho"), 1, GL_FALSE, ortho.data());
    glError("300");
    lasso_buffer.bind();
    float lasso_data[4];
    lasso_buffer.write(0, reinterpret_cast<const void *> (lasso_data), sizeof(lasso_data));

    glError("305");

    lasso_shader.enableAttributeArray( "point" );
    lasso_shader.setAttributeBuffer( "point", GL_FLOAT, 0, 2 );
    glError("309");
    if(lasso.size() > 1){
        for(unsigned int i = 0; i < lasso.size()-1; i++){
            lasso_data[0] = lasso[i].x();
            lasso_data[1] = lasso[i].y();
            lasso_data[2] = lasso[i+1].x();
            lasso_data[3] = lasso[i+1].y();
            lasso_buffer.write(0, reinterpret_cast<const void *> (lasso_data), sizeof(lasso_data));
            glDrawArrays( GL_LINES, 0, 4);
        }

        lasso_data[0] = lasso[0].x();
        lasso_data[1] = lasso[0].y();
        lasso_buffer.write(0, reinterpret_cast<const void *> (lasso_data), sizeof(lasso_data));
        glDrawArrays( GL_LINES, 0, 4);

    }

    lasso_buffer.release();
    lasso_shader.release();
    glError("329");
}

bool EditLasso::mouseDoubleClickEvent  (QMouseEvent *event, CloudModel * cm, GLArea * glarea){

    qDebug("DOUBLE CLICK");

    // End lasso
    lasso_active = !lasso_active;
    if(lasso_active){
        addLassoPoint(glarea->normalized_mouse(event->x(), event->y()));
    }
    else if(lasso.size() > 2){
        lasso.pop_back();
        qDebug("LASSO final size: %d", lasso.size());
        for(auto i: lasso){
            qDebug("(%f, %f)", i.x(), i.y());
        }
        lassoToLayer(cm, glarea);
    }

    glarea->updateGL();

    qDebug("DOUBLE CLICK END - Lasso size: %d", lasso.size());

    return true;
}

bool EditLasso::StartEdit(CloudModel * cm, GLArea * glarea){

    if(lasso_buffer.isCreated()){
        qDebug("Already initiated\n");
        return false;
    }

    // OpenGL
    if (!glarea->prepareShaderProgram(lasso_shader, ":/shaders/lasso.vert", ":/shaders/lasso.frag" ))
        return false;

    lasso_shader.bind();
    lasso_buffer.create();
    lasso_buffer.setUsagePattern( QGLBuffer::DynamicDraw );
    if ( !lasso_buffer.bind() )
    {
        qWarning() << "Could not bind vertex buffer to the context";
        return false;
    }
    lasso_buffer.allocate(sizeof(float) * 12);
    if ( !lasso_shader.bind() )
    {
        qWarning() << "Could not bind shader program to context";
        return false;
    }
    lasso_shader.enableAttributeArray( "point" );
    lasso_shader.setAttributeBuffer( "point", GL_FLOAT, 0, 2 );


    lasso_shader.release();
    lasso_buffer.release();

    return true;
}
bool EditLasso::EndEdit(CloudModel *, GLArea *){
    return true;
}

bool EditLasso::mousePressEvent  (QMouseEvent *event, CloudModel *, GLArea * glarea){

    return true;
}

bool EditLasso::mouseMoveEvent   (QMouseEvent *event, CloudModel *, GLArea * glarea){

    if(glarea->moved){
        moveLasso(glarea->normalized_mouse(event->x(), event->y()));
        if(lasso.size())
            glarea->updateGL();
    }

    return true;
}

bool EditLasso::mouseReleaseEvent(QMouseEvent *event, CloudModel *, GLArea * glarea){
    if (!glarea->moved && event->button() == Qt::LeftButton){
            addLassoPoint(glarea->normalized_mouse(event->x(), event->y()));
    }
    qDebug("CLICK - Lasso size: %d", lasso.size());
    return true;
}

QList<QAction *> EditLasso::actions() const{
    return actionList;
}
QString EditLasso::getEditToolDescription(QAction *){
    return "Info";
}

Q_EXPORT_PLUGIN2(pnp_editlasso, EditLasso)
