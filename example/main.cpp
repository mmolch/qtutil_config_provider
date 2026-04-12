#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

#include <memory>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app{argc, argv};


    auto w = MainWindow::create();
    if (!w) {
        // FATAL STARTUP ERROR!
        // Show a native OS message box and quit.
        QMessageBox::critical(nullptr,
                              "Fatal Configuration Error",
                              "The application cannot start due to a configuration error:\n\n" +
                                  w.error());
        return 1; // Exit with error code
    }

    auto pw = std::unique_ptr<MainWindow>(w.value());
    pw->show();

    return app.exec();
    //return 0;
}
