/**
 * @file simulationrunner.cpp
 * @brief Qt wrapper for athena-core Monte Carlo simulation
 * 
 * v0.6.5: Improved progress reporting with proper cross-thread signaling
 */

#include "simulationrunner.hpp"

#include <QMutexLocker>
#include <QElapsedTimer>
#include <QDebug>
#include <QDateTime>
#include <QMetaObject>

// ATHENA Core
#include <athena/analysis/montecarlo.hpp>
#include <athena/scenario.hpp>

namespace athena::ui {

// =============================================================================
// Worker Thread Implementation
// =============================================================================

class SimulationRunner::WorkerThread : public QThread
{
public:
    WorkerThread(SimulationRunner* runner, 
                 std::unique_ptr<athena::Scenario> scenario,
                 const SimulationConfig& config)
        : m_runner(runner)
        , m_scenario(std::move(scenario))
        , m_config(config)
    {
    }

    void requestCancel() { m_cancelRequested.store(true); }
    bool isCancelRequested() const { return m_cancelRequested.load(); }

protected:
    void run() override
    {
        QElapsedTimer timer;
        timer.start();
        
        SimulationResults results;
        results.totalRuns = m_config.iterations;
        
        try {
            executeSimulation(results, timer);
        } catch (const std::exception& e) {
            results.completed = false;
            results.errorMessage = QString("Exception: %1").arg(e.what());
            emitError(results.errorMessage);
        }
        
        // Always emit finished
        emitFinished(results);
    }

private:
    void executeSimulation(SimulationResults& results, QElapsedTimer& timer)
    {
        // Configure Monte Carlo executor
        athena::analysis::BatchConfig batchConfig;
        batchConfig.num_iterations = static_cast<athena::u32>(m_config.iterations);
        batchConfig.master_seed = m_config.seed == 0 
            ? static_cast<athena::Seed>(QDateTime::currentMSecsSinceEpoch())
            : m_config.seed;
        batchConfig.thread_count = m_config.threadCount == 0
            ? static_cast<athena::u32>(QThread::idealThreadCount())
            : static_cast<athena::u32>(m_config.threadCount);
        
        qDebug() << "SimulationRunner: Configuring Monte Carlo"
                 << "iterations:" << batchConfig.num_iterations
                 << "threads:" << batchConfig.thread_count
                 << "seed:" << batchConfig.master_seed;
        
        // Progress tracking
        int lastReportedPercent = -1;
        qint64 lastReportTime = 0;
        const qint64 minReportIntervalMs = 100;  // Report at most every 100ms
        
        // Progress callback - called from executor thread(s)
        batchConfig.progress_callback = [this, &results, &timer, 
                                         &lastReportedPercent, &lastReportTime,
                                         minReportIntervalMs]
            (athena::u32 completed, athena::u32 total) 
        {
            // Check for cancel
            if (m_cancelRequested.load()) {
                return;
            }
            
            // Update results atomically
            results.completedRuns = static_cast<int>(completed);
            
            // Throttle progress updates
            const qint64 now = timer.elapsed();
            const int percent = (total > 0) ? (completed * 100 / total) : 0;
            
            // Only report if: 1% change OR 100ms elapsed OR first/last
            const bool isFirstOrLast = (completed == 1 || completed == total);
            const bool percentChanged = (percent > lastReportedPercent);
            const bool timeElapsed = (now - lastReportTime >= minReportIntervalMs);
            
            if (isFirstOrLast || (percentChanged && timeElapsed)) {
                lastReportedPercent = percent;
                lastReportTime = now;
                
                // Calculate ETA
                int etaSeconds = -1;
                if (completed > 0 && completed < total) {
                    const double msPerIteration = static_cast<double>(now) / completed;
                    const int remaining = static_cast<int>(total - completed);
                    etaSeconds = static_cast<int>(msPerIteration * remaining / 1000.0);
                }
                
                // Emit via queued connection (thread-safe)
                emitProgress(static_cast<int>(completed), static_cast<int>(total), etaSeconds);
            }
        };
        
        // Create and configure executor
        athena::analysis::MonteCarloExecutor executor;
        executor.configure(batchConfig);
        executor.set_scenario(*m_scenario);
        
        qDebug() << "SimulationRunner: Starting execution";
        
        // Execute
        auto status = executor.execute();
        
        // Check cancel
        if (m_cancelRequested.load() || executor.is_cancelled()) {
            results.cancelled = true;
            results.completed = false;
            results.totalRuntimeSeconds = timer.elapsed() / 1000.0;
            qDebug() << "SimulationRunner: Cancelled after" 
                     << results.completedRuns << "iterations";
            return;
        }
        
        // Check error
        if (!status.ok()) {
            results.completed = false;
            results.errorMessage = QString::fromStdString(status.message());
            emitError(results.errorMessage);
            return;
        }
        
        // Collect results
        collectResults(results, executor, timer);
    }
    
    void collectResults(SimulationResults& results, 
                       athena::analysis::MonteCarloExecutor& executor,
                       QElapsedTimer& timer)
    {
        const auto& iterResults = executor.results();
        const auto stats = executor.compute_statistics();
        
        results.completedRuns = static_cast<int>(iterResults.size());
        
        // Get initial force counts from first iteration (before combat)
        if (!iterResults.empty()) {
            results.initialBlufor = iterResults[0].blue_initial;
            results.initialOpfor = iterResults[0].red_initial;
        }
        
        // Collect per-iteration data and count outcomes
        results.iterations.reserve(results.completedRuns);
        int iterNum = 0;
        
        for (const auto& iter : iterResults) {
            // Count outcomes
            if (iter.blue_surviving > iter.red_surviving) {
                results.bluforWins++;
            } else if (iter.red_surviving > iter.blue_surviving) {
                results.opforWins++;
            } else {
                results.draws++;
            }
            
            // Store iteration data
            IterationData data;
            data.iteration = ++iterNum;
            data.bluforSurviving = iter.blue_surviving;
            data.opforSurviving = iter.red_surviving;
            data.bluforLosses = iter.blue_initial - iter.blue_surviving;
            data.opforLosses = iter.red_initial - iter.red_surviving;
            data.ticksToCompletion = iter.ticks_to_completion;
            results.iterations.append(data);
        }
        
        // Calculate win rates
        if (results.completedRuns > 0) {
            results.bluforWinRate = static_cast<double>(results.bluforWins) / results.completedRuns;
            
            // 95% confidence interval (Wilson score interval for better accuracy)
            const double p = results.bluforWinRate;
            const double n = static_cast<double>(results.completedRuns);
            const double z = 1.96;  // 95% CI
            
            // Wilson score interval (better for extreme proportions)
            const double denominator = 1.0 + z * z / n;
            const double center = (p + z * z / (2.0 * n)) / denominator;
            const double margin = z * std::sqrt((p * (1.0 - p) + z * z / (4.0 * n)) / n) / denominator;
            
            results.bluforWinRateLow = std::max(0.0, center - margin);
            results.bluforWinRateHigh = std::min(1.0, center + margin);
            
            // Casualties
            results.avgBluforCasualties = 1.0 - stats.blue_survival_rate.mean;
            results.avgOpforCasualties = 1.0 - stats.red_survival_rate.mean;
            
            // Duration (ticks to minutes, assuming 1 tick = 1 second)
            results.avgDurationMinutes = stats.ticks_to_completion.mean / 60.0;
        }
        
        // Runtime
        results.totalRuntimeSeconds = timer.elapsed() / 1000.0;
        results.completed = true;
        
        qDebug() << "SimulationRunner: Complete"
                 << "| BLUFOR:" << (results.bluforWinRate * 100) << "%"
                 << "| CI: [" << (results.bluforWinRateLow * 100) 
                 << "-" << (results.bluforWinRateHigh * 100) << "]"
                 << "| Time:" << results.totalRuntimeSeconds << "s";
    }
    
    // Thread-safe signal emission via QMetaObject::invokeMethod
    void emitProgress(int current, int total, int eta)
    {
        QMetaObject::invokeMethod(m_runner, [=]() {
            emit m_runner->progressChanged(current, total, eta);
        }, Qt::QueuedConnection);
    }
    
    void emitFinished(const SimulationResults& results)
    {
        // Copy results for thread safety
        SimulationResults resultsCopy = results;
        QMetaObject::invokeMethod(m_runner, [=]() {
            m_runner->updateResults(resultsCopy);
            emit m_runner->finished(resultsCopy);
        }, Qt::QueuedConnection);
    }
    
    void emitError(const QString& message)
    {
        QString msgCopy = message;
        QMetaObject::invokeMethod(m_runner, [=]() {
            emit m_runner->error(msgCopy);
        }, Qt::QueuedConnection);
    }

private:
    SimulationRunner* m_runner;
    std::unique_ptr<athena::Scenario> m_scenario;
    SimulationConfig m_config;
    std::atomic<bool> m_cancelRequested{false};
};

// =============================================================================
// SimulationRunner Implementation
// =============================================================================

SimulationRunner::SimulationRunner(QObject* parent)
    : QObject(parent)
{
}

SimulationRunner::~SimulationRunner()
{
    if (m_thread && m_thread->isRunning()) {
        cancel();
        m_thread->wait(5000);
        if (m_thread->isRunning()) {
            qWarning() << "SimulationRunner: Thread did not stop, terminating";
            m_thread->terminate();
            m_thread->wait(1000);
        }
    }
}

bool SimulationRunner::start(std::unique_ptr<athena::Scenario> scenario, 
                             const SimulationConfig& config)
{
    if (m_running.load()) {
        qWarning() << "SimulationRunner: Already running";
        return false;
    }
    
    if (!scenario) {
        emit error("No scenario provided");
        return false;
    }
    
    // Validate config
    if (config.iterations < 1) {
        emit error("Invalid iteration count");
        return false;
    }
    
    // Reset state
    m_cancelRequested.store(false);
    {
        QMutexLocker lock(&m_resultsMutex);
        m_results = SimulationResults{};
        m_results.totalRuns = config.iterations;
    }
    
    // Create worker thread
    m_thread = std::make_unique<WorkerThread>(this, std::move(scenario), config);
    
    // Connect finished signal to cleanup
    connect(m_thread.get(), &QThread::finished, this, [this]() {
        m_running.store(false);
        qDebug() << "SimulationRunner: Thread finished";
    });
    
    // Start
    m_running.store(true);
    m_thread->start();
    
    qDebug() << "SimulationRunner: Started"
             << "| iterations:" << config.iterations
             << "| threads:" << (config.threadCount == 0 ? QThread::idealThreadCount() : config.threadCount);
    
    return true;
}

void SimulationRunner::cancel()
{
    if (!m_running.load()) {
        return;
    }
    
    m_cancelRequested.store(true);
    if (m_thread) {
        m_thread->requestCancel();
    }
    
    qDebug() << "SimulationRunner: Cancel requested";
}

bool SimulationRunner::isRunning() const
{
    return m_running.load();
}

SimulationResults SimulationRunner::currentResults() const
{
    QMutexLocker lock(&m_resultsMutex);
    return m_results;
}

void SimulationRunner::updateResults(const SimulationResults& results)
{
    QMutexLocker lock(&m_resultsMutex);
    m_results = results;
}

} // namespace athena::ui
