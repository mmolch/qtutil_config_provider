#include "contact.h"
#include <QDebug>

QJsonObject ContactData::toJson() const
{
    return {
        {"first_name", firstName},
        {"last_name", lastName},
        {"important", important},
        {"address", QJsonObject{
                        {"city", address.city},
                        {"postal_code", address.postalCode},
                        {"street", address.street}
                    }}
    };
}

ContactData ContactData::fromJson(const QJsonObject &obj)
{
    QJsonObject address = obj.value("address").toObject();
    return {
        .firstName = obj.value("first_name").toString(),
        .lastName = obj.value("last_name").toString(),
        .important = obj.value("important").toBool(),
        .address = {
            .city = address.value("city").toString(),
            .postalCode = address.value("postal_code").toInt(),
            .street = address.value("street").toString()
        }
    };
}

Contact::Contact(QObject *parent)
    : QObject{parent}
    , m_configProvider{
          "example/data/contact.schema.json",
          {
              "example/data/contact.application.json",
              "example/data/contact.system.json",
              "example/data/contact.user.json",
              "build/contact.user2.json"
          },
          ConfigProvider::DefaultOptions,
          this
      }
    , m_data{ContactData::fromJson(m_configProvider.currentConfig())}
{
    // Wire up successful reloads
    connect(&m_configProvider, &ConfigProvider::configChanged,
            this, &Contact::onConfigChanged);

    // Wire up error reporting
    connect(&m_configProvider, &ConfigProvider::errorOccurred,
            this, &Contact::onConfigError);

    qInfo().noquote() << "Initial config loaded for:" << m_data.firstName << m_data.lastName;
}

void Contact::onConfigChanged(const QJsonObject &newConfig)
{
    ContactData newData = ContactData::fromJson(newConfig);

    // Best Practice: Prevent redundant UI repaints/signals if the merged config
    // didn't actually alter the final parsed data.
    if (m_data == newData) {
        qDebug() << "Config reloaded, but no relevant fields changed. Ignoring.";
        return;
    }

    // Safely replace the internal data
    m_data = std::move(newData);

    qInfo().noquote() << "Config updated for:" << m_data.firstName << m_data.lastName;
    emit dataChanged();
}

void Contact::onConfigError(const QString &errorMessage)
{
    // Best Practice: Handle schema violations or parse errors gracefully
    qCritical().noquote() << "Failed to apply configuration:\n" << errorMessage;
}