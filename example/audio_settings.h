#pragma once

#include <expected>

#include <QObject>
#include <QThread>
#include "mmolch/qtutil_config_provider.h"

// 1. Strongly typed data model
struct AudioData {
    int volume = 23;
    bool muted = false;

    // C++20 auto-generated equality operator for easy change detection
    bool operator==(const AudioData& other) const = default;

    static AudioData fromJson(const QJsonObject& obj);
};

// 2. The Controller object the UI will actually interact with
class AudioSettings : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(AudioSettings)

public:
    static std::expected<AudioSettings*, QString> create(QObject *parent);

    // Read-only access to the current state
    const AudioData& data() const noexcept;

    // API for the UI to request changes
    void setVolume(int volume);
    void setMuted(bool muted);

signals:
    // Emitted only when the parsed data ACTUALLY changes
    void dataChanged();
    void errorOccurred(const QString &errorMessage);

private slots:
    // Internal handler for the ConfigProvider
    void onConfigChanged(const QJsonObject& diff);

private:
    explicit AudioSettings(QObject *parent);

    mmolch::qtutil::ConfigProvider *m_config = nullptr;
    AudioData m_data;
};
