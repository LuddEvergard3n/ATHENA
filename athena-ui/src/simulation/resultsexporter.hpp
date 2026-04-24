/**
 * @file resultsexporter.hpp
 * @brief Export simulation results to various formats
 * 
 * v0.6.12: Initial implementation - CSV, JSON export
 * v0.7.0: Uses SimulationResults from simulationrunner.hpp
 */

#ifndef ATHENA_UI_RESULTSEXPORTER_HPP
#define ATHENA_UI_RESULTSEXPORTER_HPP

#include <QString>

namespace athena::ui {

// Forward declaration - defined in simulationrunner.hpp
struct SimulationResults;

/**
 * @brief Exports simulation results to various formats
 * 
 * Supports CSV, JSON, plain text, and HTML export.
 * Uses SimulationResults from SimulationRunner.
 */
class ResultsExporter
{
public:
    /**
     * @brief Export results to CSV file
     * @return true if successful
     */
    static bool exportToCsv(const SimulationResults& results, const QString& filePath);
    
    /**
     * @brief Export results to JSON file
     * @return true if successful
     */
    static bool exportToJson(const SimulationResults& results, const QString& filePath);
    
    /**
     * @brief Export summary report as text
     * @return true if successful
     */
    static bool exportToText(const SimulationResults& results, const QString& filePath);
    
    /**
     * @brief Generate HTML report
     * @return true if successful
     */
    static bool exportToHtml(const SimulationResults& results, const QString& filePath);
    
    /**
     * @brief Get file filter string for save dialog
     */
    static QString getFileFilter();
};

} // namespace athena::ui

#endif // ATHENA_UI_RESULTSEXPORTER_HPP
