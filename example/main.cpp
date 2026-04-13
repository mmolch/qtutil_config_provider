#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app{argc, argv};

    auto window = MainWindow::create();
    if (!window) {
        // FATAL STARTUP ERROR!
        // Show a native OS message box and quit.
        QMessageBox::critical(nullptr,
                              "Fatal Configuration Error",
                              "The application cannot start due to a configuration error:\n\n" +
                                  window.error());
        return 1; // Exit with error code
    }

    window.value()->show();

    return app.exec();
}
