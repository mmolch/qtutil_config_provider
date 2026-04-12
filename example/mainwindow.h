#pragma once

#include <QMainWindow>
#include <QSlider>
#include <QCheckBox>
#include <QProgressBar>

#include "audio_settings.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void updateUiState();
    void onErrorOccurred(const QString &errorMessage);

private:
    AudioSettings m_audio;

    QSlider *m_volumeSlider;
    QCheckBox *m_muteCheckBox;
    QProgressBar *m_volumeIndicator;
};
