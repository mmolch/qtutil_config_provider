#include <QApplication>
#include "mainwindow.h"

#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app{argc, argv};


    MainWindow w;
    w.show();

    return app.exec();
    //return 0;
}
