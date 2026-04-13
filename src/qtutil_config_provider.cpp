#include "mmolch/qtutil_config_provider.h"
#include "mmolch/qtutil_json.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QSaveFile>

namespace mmolch::qtutil {

Q_LOGGING_CATEGORY(lcConfigProvider, "mmolch.qtutil.configprovider")

std::expected<ConfigProvider*, QString> ConfigProvider::create(
    const QString &schemaPath,
    const QStringList &configPaths,
    QObject *parent)
{
    // 1. Validate dependencies before constructing the object
    auto schemaResult = json_load(schemaPath);
    if (!schemaResult) {
        return std::unexpected(QStringLiteral("Failed to load schema from %1: %2")
                                   .arg(schemaPath, schemaResult.error().message));
    }

    // 2. Validate current config before constructing the object
    auto currentConfig = json_load_and_merge_with_schema(configPaths, schemaResult.value());
    if (!currentConfig) {
        QString fullError;
        for (const auto &err : currentConfig.error()) {
            fullError += err.message + "\n";
        }
        return std::unexpected(fullError);
    }

    // 3. Create the provider using the private constructor
    ConfigProvider* provider = new ConfigProvider(schemaResult.value(), configPaths, parent);
    provider->m_currentConfig = currentConfig.value();

    provider->m_saveTimer.setSingleShot(true);
    provider->m_saveTimer.setInterval(1000);
    connect(&provider->m_saveTimer, &QTimer::timeout, provider, &ConfigProvider::save);

    if (provider->m_fileWatcherEnabled) {
        provider->m_watcher = new QFileSystemWatcher(provider);
        connect(provider->m_watcher, &QFileSystemWatcher::fileChanged, provider, &ConfigProvider::onFileChanged);
        provider->setupFileWatching();
    }

    return provider;
}

ConfigProvider::ConfigProvider(QJsonObject validatedSchema,
                               QStringList configPaths,
                               QObject *parent)
    : QObject(parent)
    , m_schema{std::move(validatedSchema)}
    , m_configPaths{std::move(configPaths)}
    , m_autoSaveEnabled(true)
    , m_fileWatcherEnabled(true)
{}

ConfigProvider::~ConfigProvider() {
    if (m_autoSaveEnabled) {
        save();
    }
}

QJsonObject ConfigProvider::currentConfig() const {
    QReadLocker locker(&m_lock);
    return m_currentConfig;
}

bool ConfigProvider::autoSaveEnabled() const {
    QReadLocker locker(&m_lock);
    return m_autoSaveEnabled;
}

void ConfigProvider::setAutoSaveEnabled(bool enabled) {
    QWriteLocker locker(&m_lock);
    if (m_autoSaveEnabled == enabled) return;

    m_autoSaveEnabled = enabled;

    if (m_autoSaveEnabled) {
        if (!m_pendingDiff.isEmpty()) m_saveTimer.start();
    } else {
        m_saveTimer.stop();
    }
}

bool ConfigProvider::fileWatcherEnabled() const {
    QReadLocker locker(&m_lock);
    return m_fileWatcherEnabled;
}

void ConfigProvider::setFileWatcherEnabled(bool enabled) {
    QWriteLocker locker(&m_lock);
    if (m_fileWatcherEnabled == enabled) return;

    m_fileWatcherEnabled = enabled;

    if (m_fileWatcherEnabled) {
        if (!m_watcher) {
            m_watcher = new QFileSystemWatcher(this);
            connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &ConfigProvider::onFileChanged);
        }
        setupFileWatching();
    } else {
        if (m_watcher) {
            const QStringList files = m_watcher->files();
            if (!files.isEmpty()) {
                m_watcher->removePaths(files);
            }
        }
    }
}

bool ConfigProvider::reload() {
    QJsonObject newConfig;
    QJsonObject diff;

    {
        QWriteLocker locker(&m_lock);
        if (!loadAndMergeInternal(newConfig, diff)) return false;
        if (diff.isEmpty()) return true;

        m_currentConfig = newConfig;
        m_pendingDiff = QJsonObject{};
    }

    emit configChanged(diff);
    return true;
}

bool ConfigProvider::loadAndMergeInternal(QJsonObject &outConfig, QJsonObject &outDiff) {
    auto result = json_load_and_merge_with_schema(m_configPaths, m_schema);

    if (!result) {
        QString fullError;
        for (const auto &err : result.error()) {
            qCWarning(lcConfigProvider) << "Config Error:" << err.message;
            fullError += err.message + "\n";
        }
        emit errorOccurred(fullError.trimmed());
        return false;
    }

    outConfig = result.value();
    outDiff = json_diff(outConfig, m_currentConfig, JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull);
    return true;
}

bool ConfigProvider::updateConfig(const QJsonObject &diff) {
    QJsonObject newConfig;
    QJsonObject actualChanges;

    {
        QWriteLocker locker(&m_lock);

        newConfig = json_merge_with_schema(m_currentConfig, diff, m_schema);
        if (!json_validate(newConfig, m_schema)) {
            qCWarning(lcConfigProvider) << "Validation failed for update.";
            return false;
        }

        actualChanges = json_diff(newConfig, m_currentConfig, JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull);

        if (actualChanges.isEmpty()) return true;

        m_currentConfig = newConfig;
        m_pendingDiff = json_merge_with_schema(m_pendingDiff, actualChanges, m_schema);

        if (m_autoSaveEnabled) {
            m_saveTimer.start();
        }
    }

    emit configChanged(actualChanges);
    return true;
}

bool ConfigProvider::save() {
    QWriteLocker locker(&m_lock);

    if (m_pendingDiff.isEmpty()) return true;

    QJsonObject baseConfig;
    if (m_configPaths.size() > 1) {
        QStringList basePaths = m_configPaths;
        basePaths.removeLast();
        auto baseResult = json_load_and_merge_with_schema(basePaths, m_schema,
                                                          JsonMergeOption::Recursive | JsonMergeOption::OverrideNull | JsonMergeOption::SkipNonExisting);
        if (baseResult) baseConfig = baseResult.value();
    }

    QJsonObject saveDiff = json_diff(m_currentConfig, baseConfig,
                                     JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull);

    const QString targetPath = m_configPaths.last();
    QFileInfo targetInfo(targetPath);
    const QString finalWritePath = targetInfo.isSymLink() ? targetInfo.symLinkTarget() : targetPath;
    QDir().mkpath(QFileInfo(finalWritePath).absolutePath());

    if (m_watcher && m_fileWatcherEnabled) m_watcher->blockSignals(true);

    QSaveFile file(finalWritePath);
    bool success = false;
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(saveDiff).toJson());
        if (file.commit()) {
            qCDebug(lcConfigProvider) << "Saved configuration to" << finalWritePath;
            m_pendingDiff = QJsonObject{};
            success = true;
        } else {
            qCCritical(lcConfigProvider) << "Failed to commit config to" << finalWritePath;
        }
    } else {
        qCCritical(lcConfigProvider) << "Failed to open" << finalWritePath << "for saving:" << file.errorString();
    }

    if (m_watcher && m_fileWatcherEnabled) {
        if (!m_watcher->files().contains(finalWritePath) && QFileInfo::exists(finalWritePath)) {
            m_watcher->addPath(finalWritePath);
        }
        m_watcher->blockSignals(false);
    }

    return success;
}

void ConfigProvider::setupFileWatching() {
    if (!m_watcher) return;
    for (const QString &path : m_configPaths) {
        if (QFileInfo::exists(path) && !m_watcher->files().contains(path)) {
            m_watcher->addPath(path);
        }
    }
}

void ConfigProvider::onFileChanged(const QString &path) {
    {
        QReadLocker locker(&m_lock);
        if (!m_fileWatcherEnabled) return;
    }

    if (m_watcher && !m_watcher->files().contains(path) && QFileInfo::exists(path)) {
        m_watcher->addPath(path);
    }
    reload();
}

} // namespace mmolch::qtutil
