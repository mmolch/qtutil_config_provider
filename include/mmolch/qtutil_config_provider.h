#pragma once

#include <QJsonObject>
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QThread>
#include <QDateTime>
#include <expected>

class QFileSystemWatcher;

namespace mmolch::qtutil {

Q_DECLARE_LOGGING_CATEGORY(lcConfigProvider)

class ConfigValidator {
public:
    virtual ~ConfigValidator() = default;
    virtual std::expected<void, QString> validate(const QJsonObject& config) const = 0;
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
        const QString &schemaPath,
        const QStringList &configPaths,
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
    explicit ConfigProvider(QJsonObject validatedSchema,
                            QStringList configPaths,
                            std::unique_ptr<ConfigValidator> validator,
                            QObject *parent);

    const QJsonObject m_schema;
    const QStringList m_configPaths;
    const std::unique_ptr<ConfigValidator> m_validator;

    QJsonObject m_currentConfig;
    bool m_isDirty = false; // Replaces m_pendingDiff for massive performance gains

    bool m_autoSaveEnabled;
    bool m_fileWatcherEnabled;

    QDateTime m_lastSaveTime; // Used to prevent watcher feedback loops

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_saveTimer;

    void setupFileWatching();
    bool loadAndMergeInternal(QJsonObject &outConfig, QJsonObject &outDiff);
};

} // namespace mmolch::qtutil
