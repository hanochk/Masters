#ifndef CAMERA_H
#define CAMERA_H

#include <Eigen/Geometry>

// Contains code from kgllib

class Camera
{
public:
    Camera();

    ~Camera();

    void setViewport(int x, int y, int width, int height);

    void setFoV(float fov);

    void setAspect(float aspect);

    void setDepthRange(float near, float far);

    void setPosition(const Eigen::Vector3f& pos);
    void setPosition(float x, float y, float z)  { setPosition(Eigen::Vector3f(x, y, z)); }

    void adjustPosition(const Eigen::Vector3f& pos);
    void adjustPosition(float x, float y, float z)  { adjustPosition(Eigen::Vector3f(x, y, z)); }

    void setLookAt(const Eigen::Vector3f& lookat);
    void setLookAt(float x, float y, float z)  { setLookAt(Eigen::Vector3f(x, y, z)); }

    void setUp(const Eigen::Vector3f& up);
    void setUp(float x, float y, float z)  { setUp(Eigen::Vector3f(x, y, z)); }
    /**
     * Sets the viewing direction of the camera to @p dir.
     * This method sets lookat point to @ref position() + dir, thus
     *  you will need to set camera's position before using this method.
     **/
    void setDirection(const Eigen::Vector3f& dir);
    void setDirection(float x, float y, float z)  { setDirection(Eigen::Vector3f(x, y, z)); }

    Eigen::Vector3f position() const  { return mPosition; }
    Eigen::Vector3f lookAt() const  { return mLookAt; }
    Eigen::Vector3f up() const  { return mUp; }

    void setObjectOrientationMatrix(const Eigen::Affine3f& objectorient);
    void setModelviewMatrix(const Eigen::Affine3f& modelview);
    void setProjectionMatrix(const Eigen::Affine3f& projection);

    Eigen::Affine3f modelviewMatrix() const;

    Eigen::Affine3f projectionMatrix() const;

    void mouseDown(int x, int y, int button);
    void mouseRelease(int x, int y);
    void mouseMove(int x, int y);
    void mouseWheel(int val);

private:
    void recalculateModelviewMatrix();
    void recalculateProjectionMatrix();

public:
    static const int LEFT_BTN = 1;
    static const int RIGHT_BTN = 2;

private:
    int mouseButtonPressed;
    Eigen::Vector2f mouseStart;
    Eigen::Vector3f savedLookAt;
    Eigen::Affine3f savedObjectOrientationMatrix;
    bool mMouseDown;
    float moveSensitivity;

    Eigen::Vector3f mPosition;
    Eigen::Vector3f mLookAt;
    Eigen::Vector3f mUp;
    float mFoV, mAspect, mDepthNear, mDepthFar;

    Eigen::Affine3f mObjectOrientationMatrix;
    bool mObjectOrientationMatrixDirty;
    Eigen::Affine3f mModelviewMatrix;
    bool mModelviewMatrixDirty;
    Eigen::Affine3f mProjectionMatrix;
    bool mProjectionMatrixDirty;
};

#endif // CAMERA_H