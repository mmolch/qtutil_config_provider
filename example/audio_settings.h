#pragma once

#include <QObject>
#include "mmolch/qtutil_config_provider.h"

// 1. Strongly typed data model
struct AudioData {
    int volume = 50;
    bool muted = false;

    // C++20 auto-generated equality operator for easy change detection
    bool operator==(const AudioData& other) const = default;

    static AudioData fromJson(const QJsonObject& obj);
};

// 2. The Controller object the UI will actually interact with
class AudioSettings : public QObject {
    Q_OBJECT

public:
    explicit AudioSettings(QObject* parent = nullptr);

    // Read-only access to the current state
    const AudioData& data() const { return m_data; }

    // API for the UI to request changes
    void setVolume(int volume);
    void setMuted(bool muted);

signals:
    // Emitted only when the parsed data ACTUALLY changes
    void dataChanged();

private slots:
    // Internal handler for the ConfigProvider
    void onConfigChanged(const QJsonObject& diff);

private:
    mmolch::qtutil::ConfigProvider m_config;
    AudioData m_data;
};
