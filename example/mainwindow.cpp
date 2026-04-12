#include "mainwindow.h"
#include <QVBoxLayout>
#include <QSlider>
#include <QProgressBar>
#include <QCheckBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    layout->addWidget(m_volumeSlider);

    m_muteCheckBox = new QCheckBox("Mute Audio", this);
    layout->addWidget(m_muteCheckBox);

    m_volumeIndicator = new QProgressBar(this);
    m_volumeIndicator->setRange(0, 100);
    layout->addWidget(m_volumeIndicator);

    setCentralWidget(central);

    // --- 1. Wire UI Interactions TO the Settings Object ---
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_audio.setVolume(value);
    });

    connect(m_muteCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_audio.setMuted(checked);
    });

    // --- 2. Wire Settings Object TO the UI ---
    connect(&m_audio, &AudioSettings::dataChanged, this, &MainWindow::updateUiState);

    // --- 3. Initial UI Sync ---
    updateUiState();
}

void MainWindow::updateUiState()
{
    const AudioData& currentData = m_audio.data();

    // Block signals so updating the UI doesn't re-trigger the sliders -> updateConfig loop
    m_volumeSlider->blockSignals(true);
    m_volumeSlider->setValue(currentData.volume);
    m_volumeSlider->blockSignals(false);

    m_muteCheckBox->blockSignals(true);
    m_muteCheckBox->setChecked(currentData.muted);
    m_muteCheckBox->blockSignals(false);

    // The reaction element updates based on the typed domain data
    m_volumeIndicator->setValue(currentData.muted ? 0 : currentData.volume);
    m_volumeIndicator->setFormat(currentData.muted ? "MUTED" : "%p%");
}
