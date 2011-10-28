#include "mainwindow.h"

#include "glwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    GLWidget* g = new GLWidget(this);
    g->start();
    setCentralWidget(g);

}

MainWindow::~MainWindow()
{

}
