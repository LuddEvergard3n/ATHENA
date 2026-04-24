/**
 * @file simulationpanel.hpp
 * @brief Simulation control panel widget
 * 
 * Integrates with SimulationRunner for background execution.
 */

#ifndef ATHENA_UI_SIMULATIONPANEL_HPP
#define ATHENA_UI_SIMULATIONPANEL_HPP

#include <QWidget>
#include "simulation/simulationrunner.hpp"

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QProgressBar;
class QPushButton;
class QLabel;
class QElapsedTimer;

namespace athena::ui {

/**
 * @brief Panel for configuring and monitoring Monte Carlo simulations
 */
class SimulationPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SimulationPanel(QWidget* parent = nullptr);
    ~SimulationPanel() override;

    /**
     * @brief Get current simulation configuration
     */
    SimulationConfig getConfig() const;

    /**
     * @brief Check if simulation is running
     */
    bool isRunning() const;

signals:
    /**
     * @brief Emitted when user clicks Run
     * @param config Simulation configuration
     */
    void runRequested(const SimulationConfig& config);

    /**
     * @brief Emitted when user clicks Stop
     */
    void stopRequested();

    /**
     * @brief Emitted when progress changes
     * @param current Current iteration
     * @param total Total iterations
     */
    void progressChanged(int current, int total);

    /**
     * @brief Emitted when simulation completes
     * @param results Final results
     */
    void simulationComplete(const SimulationResults& results);

public slots:
    /**
     * @brief Update progress display
     */
    void setProgress(int current, int total, int etaSeconds);

    /**
     * @brief Called when simulation finishes
     */
    void onSimulationFinished(const SimulationResults& results);

    /**
     * @brief Reset to ready state
     */
    void reset();

private slots:
    void onRunClicked();
    void onStopClicked();

private:
    void setupUi();
    void updateButtonState(bool running);

    // Configuration widgets
    QSpinBox* m_iterationsSpinBox = nullptr;
    QSpinBox* m_threadsSpinBox = nullptr;
    QCheckBox* m_sobolCheckBox = nullptr;
    
    // Control buttons
    QPushButton* m_runButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    
    // Progress display
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_etaLabel = nullptr;
    
    // Timing
    QElapsedTimer* m_elapsedTimer = nullptr;
    
    // State
    bool m_running = false;
};

} // namespace athena::ui

#endif // ATHENA_UI_SIMULATIONPANEL_HPP
