#-------------------------------------------------
#
# Project created by QtCreator 2011-10-28T22:01:49
#
#-------------------------------------------------

QT       += core gui opengl

TARGET = beatnect
TEMPLATE = app

CONFIG += link_pkgconfig
PKGCONFIG += opencv libfreenect glu

SOURCES += main.cpp\
        mainwindow.cpp \
    glwidget.cpp

HEADERS  += mainwindow.h \
    glwidget.h
