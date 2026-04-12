#ifndef CONTACT_H
#define CONTACT_H

#include <QObject>
#include <QCoreApplication>
#include "mmolch/qtutil_config_provider.h"

using namespace mmolch::qtutil;

struct ContactData
{
    QString firstName;
    QString lastName;
    bool important = false;

    struct Address
    {
        QString city;
        int postalCode = 0;
        QString street;

        // C++20 auto-generated equality operator
        bool operator==(const Address& other) const = default;
    } address;

    // C++20 auto-generated equality operator for deep comparisons
    bool operator==(const ContactData& other) const = default;

    QJsonObject toJson() const;
    static ContactData fromJson(const QJsonObject &obj);
};

class Contact : public QObject
{
    Q_OBJECT
public:
    explicit Contact(QObject *parent = nullptr);

    // Provide read-only access to the outside world
    const ContactData& data() const { return m_data; }

    ConfigProvider& configProvider() { return m_configProvider; }

signals:
    // Only emitted when the values ACTUALLY change
    void dataChanged();

private slots:
    void onConfigChanged(const QJsonObject &newConfig);
    void onConfigError(const QString &errorMessage);

private:
    ConfigProvider m_configProvider;

    // Internal data is non-const so it can be swapped safely,
    // but protected from external modification by being private.
    ContactData m_data;
};

#endif // CONTACT_H