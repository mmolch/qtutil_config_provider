#include "mainwindow.h"
#include <QVBoxLayout>
#include <QSlider>
#include <QProgressBar>
#include <QCheckBox>
#include <QMessageBox>

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
}

std::expected<MainWindow *, QString> MainWindow::create(QWidget *parent)
{
    auto settings = AudioSettings::create(nullptr);
    if (!settings)
        return std::unexpected(settings.error());

    auto win = new MainWindow(parent);
    settings.value()->setParent(win);
    win->m_audio = settings.value();

    // --- 1. Wire UI Interactions TO the Settings Object ---
    connect(win->m_volumeSlider, &QSlider::valueChanged, win, [win](int value) {
        win->m_audio->setVolume(value);
    });

    connect(win->m_muteCheckBox, &QCheckBox::toggled, win, [win](bool checked) {
        win->m_audio->setMuted(checked);
    });

    // --- 2. Wire Settings Object TO the UI ---
    connect(win->m_audio, &AudioSettings::dataChanged, win, &MainWindow::updateUiState);
    connect(win->m_audio, &AudioSettings::errorOccurred, win, &MainWindow::onErrorOccurred);

    // --- 3. Initial UI Sync ---
    win->updateUiState();

    return win;
}

void MainWindow::updateUiState()
{
    const AudioData& currentData = m_audio->data();

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

void MainWindow::onErrorOccurred(const QString &errorMessage)
{
    auto msgBox = new QMessageBox{this};
    msgBox->setText(errorMessage);
    msgBox->setModal(true);
    msgBox->show();
}
