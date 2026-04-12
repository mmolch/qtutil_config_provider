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
    explicit ConfigProvider(const QString &schemaPath,
                            const QStringList &configPaths,
                            QObject *parent = nullptr);
    ~ConfigProvider() override;

    /**
     * @brief Loads the schema and performs the initial configuration load.
     * @return True if the schema loaded and validation succeeded, false otherwise.
     */
    bool init();

    /**
     * @brief Thread-safe access to the current configuration.
     */
    QJsonObject currentConfig() const;

    /**
     * @brief Updates config in memory and schedules save if EnableAutoSave is set.
     * @return True if validation succeeded and in-memory state was updated.
     */
    bool updateConfig(const QJsonObject &diff);

    // --- Runtime Toggles ---
    bool autoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);

    bool fileWatcherEnabled() const;
    void setFileWatcherEnabled(bool enabled);

public slots:
    /**
     * @brief Reloads configuration from disk.
     * @return True if successful, false on errors (emits errorOccurred).
     */
    bool reload();

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
    const QString m_schemaPath;
    const QStringList m_configPaths;

    mutable QReadWriteLock m_lock;

    QJsonObject m_schema;
    QJsonObject m_currentConfig;
    QJsonObject m_pendingDiff; // Changes in memory not yet flushed to disk

    bool m_autoSaveEnabled;
    bool m_fileWatcherEnabled;

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_saveTimer;

    void setupFileWatching();
    bool loadAndMergeInternal(QJsonObject &outConfig, QJsonObject &outDiff);
};

} // namespace mmolch::qtutil