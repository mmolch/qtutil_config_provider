#include "mmolch/qtutil_config_provider.h"

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
        const QStringList &configPaths,
        const QString &schemaPath,
        JsonPipelineOptions options,
        std::unique_ptr<ConfigValidator> validator,
        QObject *parent)
{
    std::optional<QJsonObject> schemaOpt = std::nullopt;
    const QJsonObject* schemaPtr = nullptr;

    if (!schemaPath.isEmpty()) {
        auto schemaResult = jsonLoad(schemaPath);
        if (!schemaResult) {
            return std::unexpected(QStringLiteral("Failed to load schema from %1: %2")
                                       .arg(schemaPath, schemaResult.error().message));
        }
        schemaOpt = schemaResult.value();
        schemaPtr = &schemaOpt.value();
    }

    auto currentConfigResult = jsonLoadAndProcess(configPaths, schemaPtr, options);
    if (!currentConfigResult) {
        QString fullError;
        fullError += currentConfigResult.error().message + "\n";
        for (const auto &err : std::as_const(currentConfigResult.error().validationErrors)) {
            fullError += "[" + err.pointer + "] " + err.message + "\n";
        }
        return std::unexpected(fullError.trimmed());
    }

    if (validator) {
        auto result = validator->validate(currentConfigResult.value());
        if (!result) {
            return std::unexpected(result.error());
        }
    }

    ConfigProvider* provider = new ConfigProvider(configPaths, std::move(schemaOpt), options, std::move(validator), parent);
    provider->m_currentConfig = currentConfigResult.value();

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

ConfigProvider::ConfigProvider(QStringList configPaths,
                               std::optional<QJsonObject> schema,
                               JsonPipelineOptions options,
                               std::unique_ptr<ConfigValidator> validator,
                               QObject *parent)
    : QObject(parent)
    , m_configPaths{std::move(configPaths)}
    , m_schema{std::move(schema)}
    , m_options{options}
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
    const QJsonObject* schemaPtr = m_schema ? &m_schema.value() : nullptr;
    auto result = jsonLoadAndProcess(m_configPaths, schemaPtr, m_options);

    if (!result) {
        QString fullError;
        qCWarning(lcConfigProvider) << result.error().message;
        fullError += result.error().message + "\n";
        for (const auto &err : std::as_const(result.error().validationErrors)) {
            qCWarning(lcConfigProvider) << err.pointer << err.message;
            fullError += "[" + err.pointer + "] " + err.message + "\n";
        }
        emit errorOccurred(fullError.trimmed());
        return false;
    }

    if (m_validator) {
        auto validation = m_validator->validate(result.value());
        if (!validation) {
            qCWarning(lcConfigProvider) << validation.error();
            emit errorOccurred(validation.error());
            return false;
        }
    }

    outConfig = result.value();
    outDiff = jsonDiff(outConfig, m_currentConfig, JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull);
    return true;
}

std::expected<ConfigProvider::ValidatedConfig, QString> ConfigProvider::previewUpdate(const QJsonObject &diff) const
{
    CHECK_THREAD();

    const QJsonObject* schemaPtr = m_schema ? &m_schema.value() : nullptr;

    // Merge the current config with the diff using configured merge options
    auto mergeResult = jsonMerge(m_currentConfig, diff, schemaPtr, m_options.mergeOptions);
    if (!mergeResult) {
        return std::unexpected(mergeResult.error().message);
    }

    QJsonObject newConfig = mergeResult.value();

    // Re-validate if a schema exists and validation is required for final states
    if (schemaPtr &&
        (m_options.validationMode == JsonValidationMode::FinalResult ||
         m_options.validationMode == JsonValidationMode::Both ||
         m_options.validationMode == JsonValidationMode::PartialPerFileAndFinal))
    {
        auto valResult = jsonValidate(newConfig, *schemaPtr);
        if (!valResult) {
            QString fullError;
            for (const auto &err : valResult.error()) {
                fullError += "[" + err.pointer + "] " + err.message + "\n";
            }
            return std::unexpected(fullError.trimmed());
        }
    }

    if (m_validator) {
        auto valResult = m_validator->validate(newConfig);
        if (!valResult) {
            return std::unexpected(valResult.error());
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
    auto actualChanges = jsonDiff(validated.data, m_currentConfig, JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull);

    if (actualChanges.isEmpty()) return true;

    m_currentConfig = std::move(validated.data);
    m_isDirty = true;
    // Optimization: Just mark dirty, defer heavy diffs to save()

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

        // Temporarily append the SkipNonExisting flag specifically for generating the base merge
        JsonPipelineOptions baseOptions = m_options;
        baseOptions.loadOptions |= JsonLoadOption::SkipNonExisting;

        const QJsonObject* schemaPtr = m_schema ? &m_schema.value() : nullptr;
        auto baseResult = jsonLoadAndProcess(basePaths, schemaPtr, baseOptions);
        if (baseResult) baseConfig = baseResult.value();
    }

    // Defer the heavy diff calculation until right before we write it
    QJsonObject saveDiff = jsonDiff(m_currentConfig, baseConfig,
                                    JsonDiffOption::Recursive | JsonDiffOption::ExplicitNull);

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
