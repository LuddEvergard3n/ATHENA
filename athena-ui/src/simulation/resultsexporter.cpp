/**
 * @file resultsexporter.cpp
 * @brief Results exporter implementation
 * 
 * v0.7.0: Uses SimulationResults from simulationrunner.hpp
 */

#include "resultsexporter.hpp"
#include "simulationrunner.hpp"

#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

namespace athena::ui {

namespace {

QString determineOutcome(const IterationData& iter, int initialBlufor, int initialOpfor)
{
    double bluforLossPercent = initialBlufor > 0 ? 
        (iter.bluforLosses * 100.0 / initialBlufor) : 0.0;
    double opforLossPercent = initialOpfor > 0 ? 
        (iter.opforLosses * 100.0 / initialOpfor) : 0.0;
    
    if (opforLossPercent > bluforLossPercent + 10) {
        return "BLUFOR_WIN";
    } else if (bluforLossPercent > opforLossPercent + 10) {
        return "OPFOR_WIN";
    }
    return "DRAW";
}

} // anonymous namespace

bool ResultsExporter::exportToCsv(const SimulationResults& results, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    
    // Header
    out << "Iteration,BLUFOR_Losses,OPFOR_Losses,BLUFOR_Loss_Percent,OPFOR_Loss_Percent,Outcome\n";
    
    // Data rows
    for (const auto& iter : results.iterations) {
        double bluforLossPercent = results.initialBlufor > 0 ? 
            (iter.bluforLosses * 100.0 / results.initialBlufor) : 0.0;
        double opforLossPercent = results.initialOpfor > 0 ? 
            (iter.opforLosses * 100.0 / results.initialOpfor) : 0.0;
        QString outcome = determineOutcome(iter, results.initialBlufor, results.initialOpfor);
        
        out << iter.iteration << ","
            << iter.bluforLosses << ","
            << iter.opforLosses << ","
            << QString::number(bluforLossPercent, 'f', 2) << ","
            << QString::number(opforLossPercent, 'f', 2) << ","
            << outcome << "\n";
    }
    
    file.close();
    return true;
}

bool ResultsExporter::exportToJson(const SimulationResults& results, const QString& filePath)
{
    QJsonObject root;
    
    // Metadata
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["total_iterations"] = results.totalRuns;
    root["completed_iterations"] = results.completedRuns;
    
    // Initial forces
    QJsonObject initialForces;
    initialForces["blufor"] = results.initialBlufor;
    initialForces["opfor"] = results.initialOpfor;
    root["initial_forces"] = initialForces;
    
    // Aggregate statistics
    double opforWinRate = results.completedRuns > 0 ? 
        static_cast<double>(results.opforWins) / results.completedRuns : 0.0;
    double drawRate = results.completedRuns > 0 ? 
        static_cast<double>(results.draws) / results.completedRuns : 0.0;
    
    QJsonObject stats;
    stats["avg_blufor_casualties"] = results.avgBluforCasualties;
    stats["avg_opfor_casualties"] = results.avgOpforCasualties;
    stats["blufor_win_rate"] = results.bluforWinRate;
    stats["opfor_win_rate"] = opforWinRate;
    stats["draw_rate"] = drawRate;
    stats["blufor_wins"] = results.bluforWins;
    stats["opfor_wins"] = results.opforWins;
    stats["draws"] = results.draws;
    root["statistics"] = stats;
    
    // Iterations
    QJsonArray iterations;
    for (const auto& iter : results.iterations) {
        double bluforLossPercent = results.initialBlufor > 0 ? 
            (iter.bluforLosses * 100.0 / results.initialBlufor) : 0.0;
        double opforLossPercent = results.initialOpfor > 0 ? 
            (iter.opforLosses * 100.0 / results.initialOpfor) : 0.0;
        QString outcome = determineOutcome(iter, results.initialBlufor, results.initialOpfor);
        
        QJsonObject iterObj;
        iterObj["iteration"] = iter.iteration;
        iterObj["blufor_losses"] = iter.bluforLosses;
        iterObj["opfor_losses"] = iter.opforLosses;
        iterObj["blufor_surviving"] = iter.bluforSurviving;
        iterObj["opfor_surviving"] = iter.opforSurviving;
        iterObj["blufor_loss_percent"] = bluforLossPercent;
        iterObj["opfor_loss_percent"] = opforLossPercent;
        iterObj["ticks"] = iter.ticksToCompletion;
        iterObj["outcome"] = outcome;
        iterations.append(iterObj);
    }
    root["iterations"] = iterations;
    
    // Write file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

bool ResultsExporter::exportToText(const SimulationResults& results, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    
    double opforWinRate = results.completedRuns > 0 ? 
        static_cast<double>(results.opforWins) / results.completedRuns : 0.0;
    double drawRate = results.completedRuns > 0 ? 
        static_cast<double>(results.draws) / results.completedRuns : 0.0;
    
    out << "===============================================================\n";
    out << "                  ATHENA SIMULATION REPORT\n";
    out << "===============================================================\n\n";
    
    out << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "Iterations: " << results.completedRuns << " / " << results.totalRuns << "\n";
    out << "Runtime: " << QString::number(results.totalRuntimeSeconds, 'f', 2) << " seconds\n\n";
    
    out << "---------------------------------------------------------------\n";
    out << "                      INITIAL FORCES\n";
    out << "---------------------------------------------------------------\n";
    out << "  BLUFOR: " << results.initialBlufor << " units\n";
    out << "  OPFOR:  " << results.initialOpfor << " units\n\n";
    
    out << "---------------------------------------------------------------\n";
    out << "                       RESULTS SUMMARY\n";
    out << "---------------------------------------------------------------\n";
    out << "  BLUFOR Win Rate: " << QString::number(results.bluforWinRate * 100, 'f', 1) << "%";
    out << " [" << QString::number(results.bluforWinRateLow * 100, 'f', 1);
    out << " - " << QString::number(results.bluforWinRateHigh * 100, 'f', 1) << "] 95% CI\n";
    out << "  OPFOR Win Rate:  " << QString::number(opforWinRate * 100, 'f', 1) << "%\n";
    out << "  Draw Rate:       " << QString::number(drawRate * 100, 'f', 1) << "%\n\n";
    
    out << "  Avg BLUFOR Casualties: " << QString::number(results.avgBluforCasualties * 100, 'f', 1) << "%\n";
    out << "  Avg OPFOR Casualties:  " << QString::number(results.avgOpforCasualties * 100, 'f', 1) << "%\n";
    out << "  Avg Duration: " << QString::number(results.avgDurationMinutes, 'f', 1) << " minutes\n\n";
    
    out << "---------------------------------------------------------------\n";
    out << "                    ITERATION DETAILS\n";
    out << "---------------------------------------------------------------\n";
    out << QString("%1 | %2 | %3 | %4\n")
        .arg("Iter", 6)
        .arg("BLUFOR", 10)
        .arg("OPFOR", 10)
        .arg("Outcome", 12);
    out << "------+------------+------------+-------------\n";
    
    for (const auto& iter : results.iterations) {
        double bluforLossPercent = results.initialBlufor > 0 ? 
            (iter.bluforLosses * 100.0 / results.initialBlufor) : 0.0;
        double opforLossPercent = results.initialOpfor > 0 ? 
            (iter.opforLosses * 100.0 / results.initialOpfor) : 0.0;
        QString outcome = determineOutcome(iter, results.initialBlufor, results.initialOpfor);
        
        out << QString("%1 | %2 (%3%) | %4 (%5%) | %6\n")
            .arg(iter.iteration, 6)
            .arg(iter.bluforLosses, 4)
            .arg(bluforLossPercent, 4, 'f', 0)
            .arg(iter.opforLosses, 4)
            .arg(opforLossPercent, 4, 'f', 0)
            .arg(outcome, 12);
    }
    
    out << "\n===============================================================\n";
    out << "                       END OF REPORT\n";
    out << "===============================================================\n";
    
    file.close();
    return true;
}

bool ResultsExporter::exportToHtml(const SimulationResults& results, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    
    double opforWinRate = results.completedRuns > 0 ? 
        static_cast<double>(results.opforWins) / results.completedRuns : 0.0;
    double drawRate = results.completedRuns > 0 ? 
        static_cast<double>(results.draws) / results.completedRuns : 0.0;
    
    out << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>ATHENA Simulation Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        .container { max-width: 900px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; border-bottom: 3px solid #2196F3; padding-bottom: 10px; }
        h2 { color: #555; margin-top: 30px; }
        .meta { color: #666; margin-bottom: 20px; }
        .stats { display: flex; justify-content: space-around; margin: 20px 0; flex-wrap: wrap; }
        .stat-box { text-align: center; padding: 20px; background: #f8f8f8; border-radius: 8px; min-width: 150px; margin: 5px; }
        .stat-value { font-size: 28px; font-weight: bold; color: #2196F3; }
        .stat-label { color: #666; margin-top: 5px; }
        .blufor { color: #1976D2; }
        .opfor { color: #D32F2F; }
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th { background: #333; color: white; padding: 12px; text-align: left; }
        td { padding: 10px; border-bottom: 1px solid #ddd; }
        tr:hover { background: #f5f5f5; }
        .win { color: #4CAF50; font-weight: bold; }
        .loss { color: #f44336; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ATHENA Simulation Report</h1>
        <div class="meta">
            <p><strong>Generated:</strong> )" << QDateTime::currentDateTime().toString(Qt::ISODate) << R"(</p>
            <p><strong>Iterations:</strong> )" << results.completedRuns << " / " << results.totalRuns << R"(</p>
            <p><strong>Runtime:</strong> )" << QString::number(results.totalRuntimeSeconds, 'f', 2) << R"( seconds</p>
            <p><strong>Initial Forces:</strong> BLUFOR )" << results.initialBlufor << " vs OPFOR " << results.initialOpfor << R"(</p>
        </div>
        
        <h2>Results Summary</h2>
        <div class="stats">
            <div class="stat-box">
                <div class="stat-value blufor">)" << QString::number(results.bluforWinRate * 100, 'f', 0) << R"(%</div>
                <div class="stat-label">BLUFOR Win Rate</div>
            </div>
            <div class="stat-box">
                <div class="stat-value opfor">)" << QString::number(opforWinRate * 100, 'f', 0) << R"(%</div>
                <div class="stat-label">OPFOR Win Rate</div>
            </div>
            <div class="stat-box">
                <div class="stat-value">)" << QString::number(drawRate * 100, 'f', 0) << R"(%</div>
                <div class="stat-label">Draw Rate</div>
            </div>
        </div>
        
        <div class="stats">
            <div class="stat-box">
                <div class="stat-value blufor">)" << QString::number(results.avgBluforCasualties * 100, 'f', 0) << R"(%</div>
                <div class="stat-label">Avg BLUFOR Casualties</div>
            </div>
            <div class="stat-box">
                <div class="stat-value opfor">)" << QString::number(results.avgOpforCasualties * 100, 'f', 0) << R"(%</div>
                <div class="stat-label">Avg OPFOR Casualties</div>
            </div>
        </div>
        
        <h2>Iteration Details</h2>
        <table>
            <thead>
                <tr>
                    <th>Iteration</th>
                    <th>BLUFOR Losses</th>
                    <th>OPFOR Losses</th>
                    <th>Outcome</th>
                </tr>
            </thead>
            <tbody>
)";
    
    for (const auto& iter : results.iterations) {
        double bluforLossPercent = results.initialBlufor > 0 ? 
            (iter.bluforLosses * 100.0 / results.initialBlufor) : 0.0;
        double opforLossPercent = results.initialOpfor > 0 ? 
            (iter.opforLosses * 100.0 / results.initialOpfor) : 0.0;
        QString outcome = determineOutcome(iter, results.initialBlufor, results.initialOpfor);
        QString outcomeClass = (outcome == "BLUFOR_WIN") ? "win" : 
                               (outcome == "OPFOR_WIN") ? "loss" : "";
        
        out << QString(R"(                <tr>
                    <td>%1</td>
                    <td class="blufor">%2 (%3%)</td>
                    <td class="opfor">%4 (%5%)</td>
                    <td class="%6">%7</td>
                </tr>
)")
            .arg(iter.iteration)
            .arg(iter.bluforLosses)
            .arg(bluforLossPercent, 0, 'f', 0)
            .arg(iter.opforLosses)
            .arg(opforLossPercent, 0, 'f', 0)
            .arg(outcomeClass)
            .arg(outcome);
    }
    
    out << R"(            </tbody>
        </table>
    </div>
</body>
</html>
)";
    
    file.close();
    return true;
}

QString ResultsExporter::getFileFilter()
{
    return "CSV Files (*.csv);;JSON Files (*.json);;Text Report (*.txt);;HTML Report (*.html)";
}

} // namespace athena::ui
