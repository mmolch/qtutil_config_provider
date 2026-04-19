#pragma once

#include "mmolch/qtutil_json.h" // Needed for JsonPipelineOptions

#include <QJsonObject>
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QThread>
#include <QDateTime>
#include <expected>
#include <optional>

class QFileSystemWatcher;

namespace mmolch::qtutil {

Q_DECLARE_LOGGING_CATEGORY(lcConfigProvider)

class ConfigValidator {
public:
    virtual ~ConfigValidator() = default;
    // Allows you to add constraints that can't be expressed in JSON schema
    virtual std::expected<void, QString> validate(const QJsonObject &config) const = 0;
};

class ConfigProvider : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(ConfigProvider)

public:
    class ValidatedConfig {
        friend class ConfigProvider;
        QJsonObject data;
        explicit ValidatedConfig(QJsonObject d) : data(std::move(d)) {}
    public:
        const QJsonObject& json() const { return data; }
    };

    static std::expected<ConfigProvider*, QString> create(
        const QStringList &configPaths,
        const QStringList &schemaPaths,
        std::unique_ptr<ConfigValidator> validator = nullptr,
        QObject *parent = nullptr);

    ~ConfigProvider() override;

    QJsonObject currentConfig() const;

    bool updateConfig(const QJsonObject &diff);
    bool updateConfig(ValidatedConfig&& validated);
    std::expected<ValidatedConfig, QString> previewUpdate(const QJsonObject &diff) const;

    bool autoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);

    bool fileWatcherEnabled() const;
    void setFileWatcherEnabled(bool enabled);

public slots:
    bool reload();
    bool save();

signals:
    void configChanged(const QJsonObject &diff);
    void errorOccurred(const QString &errorMessage);

private slots:
    void onFileChanged(const QString &path);

private:
    explicit ConfigProvider(QStringList configPaths,
                            std::optional<QJsonObject> schema,
                            std::unique_ptr<ConfigValidator> validator,
                            QObject *parent);

    const QStringList m_configPaths;
    const std::optional<QJsonObject> m_schema;
    const std::unique_ptr<ConfigValidator> m_validator;

    QJsonObject m_currentConfig;
    bool m_isDirty = false;

    bool m_autoSaveEnabled;
    bool m_fileWatcherEnabled;

    QDateTime m_lastSaveTime;

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_saveTimer;

    void setupFileWatching();
    bool loadAndMergeInternal(QJsonObject &outConfig, QJsonObject &outDiff);
};

} // namespace mmolch::qtutil
