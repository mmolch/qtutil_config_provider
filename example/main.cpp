#include <QApplication>
#include "contact.h"

#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app{argc, argv};
    Contact contact;

    QObject::connect(&contact, &Contact::dataChanged, [&contact](){
        qInfo() << "Data changed" << contact.data().firstName;
    });

    contact.configProvider().updateConfig({
        {"first_name", "Dieter"}
    });

    return app.exec();
    //return 0;
}
