#pragma once

#include <QJsonObject>
#include <QLoggingCategory>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>
#include <QTimer>

class QFileSystemWatcher;

namespace mmolch::qtutil {

Q_DECLARE_LOGGING_CATEGORY(lcConfigProvider)

class ConfigProvider : public QObject {
    Q_OBJECT

public:
    enum class Option {
        None = 0,
        EnableAutoSave = 1 << 0,
        EnableFileWatcher = 1 << 1
    };
    Q_DECLARE_FLAGS(Options, Option)
    Q_FLAG(Options)

    // Default: Both AutoSave and FileWatcher are enabled
    static constexpr Options DefaultOptions = Options(Option::EnableAutoSave) | Option::EnableFileWatcher;

    explicit ConfigProvider(const QString &schemaPath,
                            const QStringList &configPaths,
                            Options options = DefaultOptions,
                            QObject *parent = nullptr);
    ~ConfigProvider() override;

    /**
     * @brief Thread-safe access to the current configuration.
     */
    QJsonObject currentConfig() const;

    /**
     * @brief Updates config in memory and schedules save if EnableAutoSave is set.
     * @return True if validation succeeded and in-memory state was updated.
     */
    bool updateConfig(const QJsonObject &diff);

public slots:
    void reload();

    /**
     * @brief Flushes any pending changes to disk immediately.
     */
    bool save();

signals:
    /**
     * @brief Emitted when the configuration changes.
     * @param diff A JSON object containing ONLY the keys that were altered.
     */
    void configChanged(const QJsonObject &diff);
    void errorOccurred(const QString &errorMessage);

private slots:
    void onFileChanged(const QString &path);

private:
    const QJsonObject m_schema;
    const QStringList m_configPaths;
    const Options m_options;

    mutable QReadWriteLock m_lock;

    QJsonObject m_currentConfig;
    QJsonObject m_pendingDiff; // Changes in memory not yet flushed to disk

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_saveTimer;

    void setupFileWatching();
    bool loadAndMergeInternal(QJsonObject &outConfig, QJsonObject &outDiff);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ConfigProvider::Options)

} // namespace mmolch::qtutil
