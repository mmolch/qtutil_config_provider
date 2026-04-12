#pragma once

#include <expected>
#include <QMainWindow>
#include <QSlider>
#include <QCheckBox>
#include <QProgressBar>

#include "audio_settings.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY(MainWindow)

public:
    static std::expected<MainWindow*, QString> create(QWidget *parent = nullptr);

private slots:
    void updateUiState();
    void onErrorOccurred(const QString &errorMessage);

private:
    MainWindow(QWidget *parent = nullptr);

    AudioSettings *m_audio;

    QSlider *m_volumeSlider;
    QCheckBox *m_muteCheckBox;
    QProgressBar *m_volumeIndicator;
};
