#include "audio_settings.h"

using namespace mmolch::qtutil;

AudioData AudioData::fromJson(const QJsonObject& obj) {
    return {
        obj.value("volume").toInt(50),
        obj.value("muted").toBool(false)
    };
}

AudioSettings::AudioSettings(QObject* parent)
    : QObject(parent)
    , m_config(
        "example/data/audio.schema.json",
        {
            "example/data/audio.default.json",
            "example/data/audio.user.json",
            "build/audio.demo.json"
        },
        this
    )
    , m_data()
{
    // Wire up the internal config provider
    connect(&m_config, &ConfigProvider::configChanged, this, &AudioSettings::onConfigChanged);
    connect(&m_config, &ConfigProvider::errorOccurred, this, &AudioSettings::errorOccurred);
}

bool AudioSettings::init()
{
    return m_config.init();
}

void AudioSettings::setVolume(int volume) {
    // Push the partial change to the config provider
    m_config.updateConfig({{"volume", volume}});
}

void AudioSettings::setMuted(bool muted) {
    // Push the partial change to the config provider
    m_config.updateConfig({{"muted", muted}});
}

void AudioSettings::onConfigChanged(const QJsonObject& /*diff*/) {
    // 1. Parse the newly merged configuration
    AudioData newData = AudioData::fromJson(m_config.currentConfig());

    // 2. Prevent redundant UI updates if the change didn't affect our fields
    if (m_data == newData) {
        return;
    }

    // 3. Update internal state and notify the UI
    m_data = newData;
    emit dataChanged();
}
