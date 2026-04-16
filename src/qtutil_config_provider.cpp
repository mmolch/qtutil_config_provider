#include "mmolch/qtutil_config_provider.h"
#include "mmolch/qtutil_json.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QSaveFile>

namespace mmolch::qtutil {

Q_LOGGING_CATEGORY(lcConfigProvider, "mmolch.qtutil.configprovider")

#ifndef NDEBUG
#define CHECK_THREAD() \
do { \
        if (QThread::currentThread() != this->thread()) { \
            qCWarning(lcConfigProvider) << "Thread affinity violation in:" << Q_FUNC_INFO; \
    } \
} while (false)
#else
#define CHECK_THREAD() do {} while (false)
#endif

std::expected<ConfigProvider*, QString> ConfigProvider::create(
    const QString &schemaPath,
    const QStringList &configPaths,
    std::unique_ptr<ConfigValidator> validator,
    QObject *parent)
{
    auto schemaResult = json_load(schemaPath);
    if (!schemaResult) {
        return std::unexpected(QStringLiteral("Failed to load schema from %1: %2")
                                   .arg(schemaPath, schemaResult.error().message));
    }

    auto currentConfig = json_load_and_merge_with_schema(configPaths, schemaResult.value());
    if (!currentConfig) {
        QString fullError;
        fullError += currentConfig.error().message + "\n";
        for (const auto &err : std::as_const(currentConfig.error().validationErrors)) {
            fullError += "[" + err.pointer + "] " + err.message + "\n";
        }
        return std::unexpected(fullError.trimmed());
    }

    if (validator) {
        auto result = validator->validate(currentConfig.value());
        if (!result) {
            return std::unexpected(result.error());
        }
    }

    ConfigProvider* provider = new ConfigProvider(schemaResult.value(), configPaths, std::move(validator), parent);
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
                               std::unique_ptr<ConfigValidator> validator,
                               QObject *parent)
    : QObject(parent)
    , m_schema{std::move(validatedSchema)}
    , m_configPaths{std::move(configPaths)}
    , m_validator{std::move(validator)}
    , m_autoSaveEnabled(false)
    , m_fileWatcherEnabled(false)
{}

ConfigProvider::~ConfigProvider() {
    CHECK_THREAD();
    if (m_autoSaveEnabled) {
        save();
    }
}

QJsonObject ConfigProvider::currentConfig() const {
    CHECK_THREAD();
    return m_currentConfig;
}

bool ConfigProvider::autoSaveEnabled() const {
    CHECK_THREAD();
    return m_autoSaveEnabled;
}

void ConfigProvider::setAutoSaveEnabled(bool enabled) {
    CHECK_THREAD();
    if (m_autoSaveEnabled == enabled) return;

    m_autoSaveEnabled = enabled;
    if (m_autoSaveEnabled) {
        if (m_isDirty) m_saveTimer.start();
    } else {
        m_saveTimer.stop();
    }
}

bool ConfigProvider::fileWatcherEnabled() const {
    CHECK_THREAD();
    return m_fileWatcherEnabled;
}

void ConfigProvider::setFileWatcherEnabled(bool enabled) {
    CHECK_THREAD();
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
    CHECK_THREAD();
    QJsonObject newConfig;
    QJsonObject diff;

    if (!loadAndMergeInternal(newConfig, diff)) return false;
    if (diff.isEmpty()) return true;

    m_currentConfig = newConfig;
    m_isDirty = false;

    emit configChanged(diff);
    return true;
}

bool ConfigProvider::loadAndMergeInternal(QJsonObject &outConfig, QJsonObject &outDiff) {
    auto result = json_load_and_merge_with_schema(m_configPaths, m_schema);
    if (!result) {
        QString fullError;
        qCWarning(lcConfigProvider) << "Config Error:" << result.error().message;
        fullError += result.error().message + "\n";
        for (const auto &err : std::as_const(result.error().validationErrors)) {
            qCWarning(lcConfigProvider) << "Config Error:" << err.pointer << err.message;
            fullError += "[" + err.pointer + "] " + err.message + "\n";
        }
        emit errorOccurred(fullError.trimmed());
        return false;
    }

    outConfig = result.value();
    outDiff = json_diff(outConfig, m_currentConfig, JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull);
    return true;
}

std::expected<ConfigProvider::ValidatedConfig, QString> ConfigProvider::previewUpdate(const QJsonObject &diff) const
{
    CHECK_THREAD();
    QJsonObject newConfig = json_merge_with_schema(m_currentConfig, diff, m_schema);
    auto result = json_validate(newConfig, m_schema);

    if (!result) {
        QString fullError;
        for (const auto &err : result.error()) {
            fullError += "[" + err.pointer + "] " + err.message + "\n";
        }
        return std::unexpected(fullError.trimmed());
    }

    if (m_validator) {
        auto result = m_validator->validate(newConfig);
        if (!result) {
            return std::unexpected(result.error());
        }
    }

    return ValidatedConfig{newConfig};
}

bool ConfigProvider::updateConfig(const QJsonObject &diff) {
    CHECK_THREAD();

    // Delegate schema merging and validation to previewUpdate to avoid duplication
    auto preview = previewUpdate(diff);
    if (!preview) {
        qCWarning(lcConfigProvider) << "Config Update Error:" << preview.error();
        emit errorOccurred(preview.error());
        return false;
    }

    return updateConfig(std::move(preview.value()));
}

bool ConfigProvider::updateConfig(ValidatedConfig&& validated) {
    CHECK_THREAD();
    auto actualChanges = json_diff(validated.data, m_currentConfig, JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull);

    if (actualChanges.isEmpty()) return true;

    m_currentConfig = std::move(validated.data);
    m_isDirty = true; // Optimization: Just mark dirty, defer heavy diffs to save()

    if (m_autoSaveEnabled) {
        m_saveTimer.start();
    }

    emit configChanged(actualChanges);
    return true;
}

bool ConfigProvider::save() {
    CHECK_THREAD();
    if (!m_isDirty) return true;

    QJsonObject baseConfig;
    if (m_configPaths.size() > 1) {
        QStringList basePaths = m_configPaths;
        basePaths.removeLast();
        auto baseResult = json_load_and_merge_with_schema(basePaths, m_schema,
                                                          JsonMergeOption::Recursive | JsonMergeOption::OverrideNull | JsonMergeOption::SkipNonExisting);
        if (baseResult) baseConfig = baseResult.value(); //
    }

    // Defer the heavy diff calculation until right before we write it
    QJsonObject saveDiff = json_diff(m_currentConfig, baseConfig,
                                     JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull); //

    const QString targetPath = m_configPaths.last();
    QFileInfo targetInfo(targetPath);
    const QString finalWritePath = targetInfo.isSymLink() ? targetInfo.symLinkTarget() : targetPath;
    QDir().mkpath(QFileInfo(finalWritePath).absolutePath());

    QSaveFile file(finalWritePath);
    bool success = false;
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(saveDiff).toJson());
        if (file.commit()) {
            // Record exactly when we saved to prevent watcher echo loops
            m_lastSaveTime = QFileInfo(finalWritePath).lastModified();
            m_isDirty = false;
            success = true;
            qCDebug(lcConfigProvider) << "Saved configuration to" << finalWritePath;
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
    CHECK_THREAD();
    if (!m_fileWatcherEnabled) return;

    if (m_watcher && !m_watcher->files().contains(path) && QFileInfo::exists(path)) {
        m_watcher->addPath(path);
    }

    QFileInfo info(path);
    if (info.lastModified() <= m_lastSaveTime) {
        return;
    }
    // prevent this specific version from triggering again if config file was changed externally
    m_lastSaveTime = info.lastModified();

    reload();
}

} // namespace mmolch::qtutil
