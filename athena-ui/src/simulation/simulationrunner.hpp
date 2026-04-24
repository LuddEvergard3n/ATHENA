/**
 * @file simulationrunner.hpp
 * @brief Qt wrapper for athena-core Monte Carlo simulation
 * 
 * Runs simulation in background thread, emits progress signals.
 */

#ifndef ATHENA_UI_SIMULATIONRUNNER_HPP
#define ATHENA_UI_SIMULATIONRUNNER_HPP

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QString>
#include <memory>
#include <atomic>

// Forward declarations - athena-core
namespace athena {
    struct Scenario;
    struct SimulationResult;
}

namespace athena::ui {

/**
 * @brief Configuration for simulation run
 */
struct SimulationConfig {
    int iterations = 1000;          // Number of Monte Carlo iterations
    int threadCount = 0;            // 0 = auto-detect
    bool enableSobol = false;       // Enable Sobol sensitivity analysis
    uint64_t seed = 0;              // 0 = random seed
    
    // Confidence interval settings
    double targetConfidence = 0.95;
    double maxCIWidth = 0.05;       // Stop early if CI width < 5%
};

/**
 * @brief Results from simulation run
 */
/**
 * @brief Single iteration result data
 */
struct IterationData {
    int iteration = 0;
    int bluforSurviving = 0;
    int opforSurviving = 0;
    int bluforLosses = 0;
    int opforLosses = 0;
    int ticksToCompletion = 0;
};

/**
 * @brief Aggregated results from Monte Carlo simulation
 */
struct SimulationResults {
    // Basic statistics
    int totalRuns = 0;
    int completedRuns = 0;
    int bluforWins = 0;
    int opforWins = 0;
    int draws = 0;
    
    // Win rates with confidence intervals
    double bluforWinRate = 0.0;
    double bluforWinRateLow = 0.0;   // 95% CI lower bound
    double bluforWinRateHigh = 0.0;  // 95% CI upper bound
    
    // Casualties
    double avgBluforCasualties = 0.0;
    double avgOpforCasualties = 0.0;
    
    // Timing
    double avgDurationMinutes = 0.0;
    double totalRuntimeSeconds = 0.0;
    
    // Status
    bool completed = false;
    bool cancelled = false;
    QString errorMessage;
    
    // Per-iteration data (for export)
    int initialBlufor = 0;
    int initialOpfor = 0;
    QVector<IterationData> iterations;
};

/**
 * @brief Runs Monte Carlo simulation in background thread
 * 
 * Usage:
 *   SimulationRunner* runner = new SimulationRunner(this);
 *   connect(runner, &SimulationRunner::progressChanged, this, &MyClass::onProgress);
 *   connect(runner, &SimulationRunner::finished, this, &MyClass::onFinished);
 *   runner->start(scenario, config);
 */
class SimulationRunner : public QObject
{
    Q_OBJECT

public:
    explicit SimulationRunner(QObject* parent = nullptr);
    ~SimulationRunner() override;

    /**
     * @brief Start simulation with given scenario and config
     * @param scenario Scenario to simulate (takes ownership)
     * @param config Simulation configuration
     * @return true if started successfully
     */
    bool start(std::unique_ptr<athena::Scenario> scenario, const SimulationConfig& config);

    /**
     * @brief Request simulation to stop
     * 
     * Simulation will stop at next iteration boundary.
     * finished() signal will be emitted with cancelled=true.
     */
    void cancel();

    /**
     * @brief Check if simulation is currently running
     */
    bool isRunning() const;

    /**
     * @brief Get current results (thread-safe snapshot)
     */
    SimulationResults currentResults() const;

    /**
     * @brief Update results (called from worker thread)
     * @internal
     */
    void updateResults(const SimulationResults& results);

signals:
    /**
     * @brief Emitted when progress changes
     * @param current Current iteration number
     * @param total Total iterations planned
     * @param etaSeconds Estimated seconds remaining (-1 if unknown)
     */
    void progressChanged(int current, int total, int etaSeconds);

    /**
     * @brief Emitted when a batch of iterations complete
     * @param results Current results snapshot
     */
    void batchComplete(const SimulationResults& results);

    /**
     * @brief Emitted when simulation finishes (success, error, or cancelled)
     * @param results Final results
     */
    void finished(const SimulationResults& results);

    /**
     * @brief Emitted on error
     * @param message Error description
     */
    void error(const QString& message);

private slots:
    void onThreadFinished();

private:
    class WorkerThread;
    std::unique_ptr<WorkerThread> m_thread;
    
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancelRequested{false};
    
    mutable QMutex m_resultsMutex;
    SimulationResults m_results;
};

} // namespace athena::ui

#endif // ATHENA_UI_SIMULATIONRUNNER_HPP
