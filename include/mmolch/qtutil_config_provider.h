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
#include <memory>
#include <optional>

class QFileSystemWatcher;

namespace mmolch::qtutil {

Q_DECLARE_LOGGING_CATEGORY(lcConfigProvider)

namespace config_provider::detail {
    struct QObjectDeleter {
        void operator()(QObject *obj) const {
            if (obj) obj->deleteLater();
        }
    };
}

class ConfigProvider;
using ConfigProviderPtr = std::unique_ptr<ConfigProvider, config_provider::detail::QObjectDeleter>;

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

    [[nodiscard]]
    static std::expected<ConfigProviderPtr, QString> create(
        QStringList configPaths,
        QStringList schemaPaths = QStringList(),
        std::unique_ptr<ConfigValidator> validator = nullptr);

    ~ConfigProvider() override;

    [[nodiscard]]
    QJsonObject currentConfig() const ;
    [[nodiscard]]
    const std::optional<QJsonObject> &schema() const noexcept;

    bool changeConfig(const QJsonObject &changes);
    bool changeConfig(ValidatedConfig&& validated);
    [[nodiscard]]
    std::expected<ValidatedConfig, QString> previewChanges(const QJsonObject &changes) const;

    [[nodiscard]]
    bool autoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);

    [[nodiscard]]
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
                            std::unique_ptr<ConfigValidator> validator);

    const QStringList m_configPaths;
    const std::optional<QJsonObject> m_schema;
    const std::unique_ptr<ConfigValidator> m_validator;

    QJsonObject m_currentConfig;
    bool m_isDirty = false;

    bool m_autoSaveEnabled;
    bool m_fileWatcherEnabled;

    QDateTime m_lastSaveTime;

    std::unique_ptr<QFileSystemWatcher, config_provider::detail::QObjectDeleter> m_watcher{nullptr};
    QTimer m_saveTimer;

    void setupFileWatching();
    bool loadAndMergeInternal(QJsonObject &outConfig, QJsonObject &outDiff);
};

} // namespace mmolch::qtutil
