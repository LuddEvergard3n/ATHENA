/**
 * @file simulationpanel.cpp
 * @brief Simulation control panel implementation
 * 
 * v0.6.5: Enhanced progress display with speed indicator
 */

#include "simulationpanel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QThread>
#include <QElapsedTimer>
#include <QStyle>

namespace athena::ui {

SimulationPanel::SimulationPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    m_elapsedTimer = new QElapsedTimer();
}

SimulationPanel::~SimulationPanel()
{
    delete m_elapsedTimer;
}

SimulationConfig SimulationPanel::getConfig() const
{
    SimulationConfig config;
    config.iterations = m_iterationsSpinBox->value();
    config.threadCount = m_threadsSpinBox->value();
    config.enableSobol = m_sobolCheckBox->isChecked();
    config.seed = 0;  // Random seed
    return config;
}

bool SimulationPanel::isRunning() const
{
    return m_running;
}

void SimulationPanel::setProgress(int current, int total, int etaSeconds)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    
    const double percent = (total > 0) ? (100.0 * current / total) : 0.0;
    
    // Calculate iterations per second
    QString speedText;
    if (m_elapsedTimer->isValid() && current > 0) {
        const qint64 elapsedMs = m_elapsedTimer->elapsed();
        if (elapsedMs > 0) {
            const double iterPerSec = (current * 1000.0) / elapsedMs;
            if (iterPerSec >= 1000) {
                speedText = QString(" @ %1k it/s").arg(iterPerSec / 1000.0, 0, 'f', 1);
            } else {
                speedText = QString(" @ %1 it/s").arg(iterPerSec, 0, 'f', 0);
            }
        }
    }
    
    m_statusLabel->setText(QString("Running: %1/%2 (%3%)%4")
        .arg(current)
        .arg(total)
        .arg(percent, 0, 'f', 1)
        .arg(speedText));
    
    // Format ETA
    if (etaSeconds >= 0) {
        if (etaSeconds >= 3600) {
            const int hours = etaSeconds / 3600;
            const int minutes = (etaSeconds % 3600) / 60;
            m_etaLabel->setText(QString("ETA: %1h %2m").arg(hours).arg(minutes));
        } else if (etaSeconds >= 60) {
            const int minutes = etaSeconds / 60;
            const int seconds = etaSeconds % 60;
            m_etaLabel->setText(QString("ETA: %1:%2")
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0')));
        } else {
            m_etaLabel->setText(QString("ETA: %1s").arg(etaSeconds));
        }
    } else {
        m_etaLabel->setText("ETA: calculating...");
    }
}

void SimulationPanel::onSimulationFinished(const SimulationResults& results)
{
    m_running = false;
    updateButtonState(false);
    
    if (results.cancelled) {
        m_statusLabel->setText(QString("Cancelled after %1 runs").arg(results.completedRuns));
        m_progressBar->setStyleSheet("QProgressBar::chunk { background-color: #FFA500; }");
    } else if (!results.errorMessage.isEmpty()) {
        m_statusLabel->setText(QString("Error: %1").arg(results.errorMessage));
        m_progressBar->setStyleSheet("QProgressBar::chunk { background-color: #FF4444; }");
    } else {
        // Success - show summary
        const double ciWidth = (results.bluforWinRateHigh - results.bluforWinRateLow) * 100;
        m_statusLabel->setText(QString("Complete: %1 runs | BLUFOR: %2% ± %3%")
            .arg(results.completedRuns)
            .arg(results.bluforWinRate * 100, 0, 'f', 1)
            .arg(ciWidth / 2, 0, 'f', 1));
        m_progressBar->setStyleSheet("QProgressBar::chunk { background-color: #44BB44; }");
    }
    
    // Show runtime
    if (results.totalRuntimeSeconds >= 60) {
        const int minutes = static_cast<int>(results.totalRuntimeSeconds) / 60;
        const int seconds = static_cast<int>(results.totalRuntimeSeconds) % 60;
        m_etaLabel->setText(QString("Runtime: %1m %2s").arg(minutes).arg(seconds));
    } else {
        m_etaLabel->setText(QString("Runtime: %1s").arg(results.totalRuntimeSeconds, 0, 'f', 1));
    }
    
    m_progressBar->setValue(m_progressBar->maximum());
    
    emit simulationComplete(results);
}

void SimulationPanel::reset()
{
    m_running = false;
    updateButtonState(false);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet("");  // Reset color
    m_statusLabel->setText("Ready");
    m_etaLabel->setText("");
}

void SimulationPanel::onRunClicked()
{
    m_running = true;
    updateButtonState(true);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet("");  // Reset to default color
    m_statusLabel->setText("Starting...");
    m_etaLabel->setText("ETA: --:--");
    
    // Start timer for speed calculation
    m_elapsedTimer->start();
    
    emit runRequested(getConfig());
}

void SimulationPanel::onStopClicked()
{
    m_statusLabel->setText("Stopping...");
    m_stopButton->setEnabled(false);  // Prevent double-click
    emit stopRequested();
}

void SimulationPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    
    // ==========================================================================
    // Monte Carlo Settings Group
    // ==========================================================================
    auto* mcGroup = new QGroupBox("Monte Carlo Settings", this);
    auto* mcLayout = new QFormLayout(mcGroup);
    
    // Iterations
    m_iterationsSpinBox = new QSpinBox(this);
    m_iterationsSpinBox->setRange(10, 100000);
    m_iterationsSpinBox->setValue(1000);
    m_iterationsSpinBox->setSingleStep(100);
    m_iterationsSpinBox->setToolTip("Number of simulation iterations to run.\n"
                                     "More iterations = more accurate results, longer runtime.");
    mcLayout->addRow("Iterations:", m_iterationsSpinBox);
    
    // Threads
    m_threadsSpinBox = new QSpinBox(this);
    m_threadsSpinBox->setRange(1, 64);
    m_threadsSpinBox->setValue(QThread::idealThreadCount());
    m_threadsSpinBox->setToolTip(QString("Number of parallel threads.\n"
                                          "Detected cores: %1")
                                  .arg(QThread::idealThreadCount()));
    mcLayout->addRow("Threads:", m_threadsSpinBox);
    
    mainLayout->addWidget(mcGroup);
    
    // ==========================================================================
    // Sensitivity Analysis Group
    // ==========================================================================
    auto* saGroup = new QGroupBox("Sensitivity Analysis", this);
    auto* saLayout = new QVBoxLayout(saGroup);
    
    m_sobolCheckBox = new QCheckBox("Enable Sobol Analysis", this);
    m_sobolCheckBox->setToolTip("Compute variance-based sensitivity indices.\n"
                                 "Identifies which parameters have the most impact.\n"
                                 "Note: Increases runtime significantly.");
    saLayout->addWidget(m_sobolCheckBox);
    
    mainLayout->addWidget(saGroup);
    
    // ==========================================================================
    // Control Buttons
    // ==========================================================================
    auto* buttonLayout = new QHBoxLayout();
    
    m_runButton = new QPushButton("Run Simulation", this);
    m_runButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_runButton->setMinimumHeight(40);
    m_runButton->setStyleSheet("QPushButton { font-weight: bold; font-size: 14px; }");
    m_runButton->setToolTip("Start Monte Carlo simulation with current settings");
    buttonLayout->addWidget(m_runButton, 2);
    
    m_stopButton = new QPushButton("Stop", this);
    m_stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopButton->setMinimumHeight(40);
    m_stopButton->setEnabled(false);
    m_stopButton->setToolTip("Cancel running simulation");
    buttonLayout->addWidget(m_stopButton, 1);
    
    mainLayout->addLayout(buttonLayout);
    
    // ==========================================================================
    // Progress Group
    // ==========================================================================
    auto* progressGroup = new QGroupBox("Progress", this);
    auto* progressLayout = new QVBoxLayout(progressGroup);
    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setMinimumHeight(24);
    progressLayout->addWidget(m_progressBar);
    
    auto* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; }");
    statusLayout->addWidget(m_statusLabel, 1);
    
    m_etaLabel = new QLabel("", this);
    m_etaLabel->setAlignment(Qt::AlignRight);
    m_etaLabel->setStyleSheet("QLabel { color: #666; }");
    statusLayout->addWidget(m_etaLabel);
    
    progressLayout->addLayout(statusLayout);
    
    mainLayout->addWidget(progressGroup);
    
    // Spacer
    mainLayout->addStretch();
    
    // ==========================================================================
    // Connections
    // ==========================================================================
    connect(m_runButton, &QPushButton::clicked, this, &SimulationPanel::onRunClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &SimulationPanel::onStopClicked);
}

void SimulationPanel::updateButtonState(bool running)
{
    m_runButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_iterationsSpinBox->setEnabled(!running);
    m_threadsSpinBox->setEnabled(!running);
    m_sobolCheckBox->setEnabled(!running);
}

} // namespace athena::ui
