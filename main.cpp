#include <QtGui/QApplication>
#include "mainwindow.h"

#include "glwidget.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setFixedSize(1280,480);
    w.show();

    return a.exec();

}
