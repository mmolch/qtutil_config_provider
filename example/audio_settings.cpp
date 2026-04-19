#include "audio_settings.h"

using namespace mmolch::qtutil;

AudioData AudioData::fromJson(const QJsonObject& obj) {
    return {
        obj.value("volume").toInt(27),
        obj.value("muted").toBool(false)
    };
}

AudioSettings::AudioSettings(QObject *parent)
    : QObject(parent)
{}

std::expected<AudioSettings *, QString> AudioSettings::create(QObject *parent)
{
    auto config = ConfigProvider::create({
                                             "example/data/audio.default.json",
                                             "example/data/audio.user.json",
                                             "build/audio.demo.json"
                                         },
                                         {"example/data/audio.schema.json"},
                                         {.processOptions {.inputValidationMode = JsonValidationMode::Partial}});
    if (!config)
        return std::unexpected(config.error());

    auto* settings = new AudioSettings{parent};
    config.value()->setParent(settings);
    settings->m_config = config.value();
    settings->m_data = AudioData::fromJson(settings->m_config->currentConfig());

    // Wire up the internal config provider
    connect(settings->m_config, &ConfigProvider::configChanged, settings, &AudioSettings::onConfigChanged);
    connect(settings->m_config, &ConfigProvider::errorOccurred, settings, &AudioSettings::errorOccurred);

    settings->m_config->setAutoSaveEnabled(true);
    settings->m_config->setFileWatcherEnabled(true);

    return settings;
}

const AudioData &AudioSettings::data() const noexcept {
#ifndef NDEBUG
    if (QThread::currentThread() != thread()) {
        qWarning() << "AudioSettings::data() accessed from wrong thread!";
        Q_ASSERT(false);
    }
#endif
    return m_data;
}

void AudioSettings::setVolume(int volume) {
    // Push the partial change to the config provider
    m_config->updateConfig({{"volume", volume}});
}

void AudioSettings::setMuted(bool muted) {
    // Push the partial change to the config provider
    m_config->updateConfig({{"muted", muted}});
}

void AudioSettings::onConfigChanged(const QJsonObject& diff) {
    // 1. Parse the newly merged configuration
    AudioData newData = AudioData::fromJson(m_config->currentConfig());

    // 2. Prevent redundant UI updates if the change didn't affect our fields
    if (m_data == newData) {
        return;
    }

    // 3. Update internal state and notify the UI
    m_data = newData;
    emit dataChanged();
}
