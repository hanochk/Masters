include(../../general.pri)

TEMPLATE      = lib
CONFIG       += plugin
TARGET = ../edit_lasso
HEADERS += edit_lasso.h
SOURCES += edit_lasso.cpp
QT       += opengl core gui

INCLUDEPATH += \
        "../.." \
        "/usr/include/pcl-1.5/" \
        "/usr/include/flann/" \
        "/usr/include/eigen3/" \
        "/opt/AMDAPP/include" \
        "/opt/NVIDIA_GPU_Computing_SDK/OpenCL/common/inc"

LIBS += -lpcl_io \
        -lpcl_common  \
        -lpcl_features  \
        -lpcl_kdtree  \
        -lpcl_visualization \
        -lpcl_search \
        -lpcl_filters \
        -lGL \
        -lGLU \
        -lOpenCL
