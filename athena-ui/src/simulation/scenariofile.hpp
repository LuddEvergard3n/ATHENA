/**
 * @file scenariofile.hpp
 * @brief Scenario file serialization (JSON format)
 * 
 * Saves and loads force composition to/from JSON files.
 * Format is human-readable and version-controlled.
 */

#ifndef ATHENA_UI_SCENARIOFILE_HPP
#define ATHENA_UI_SCENARIOFILE_HPP

#include <QString>
#include <vector>

namespace athena::ui {

struct ForceUnit;

/**
 * @brief Scenario file metadata
 */
struct ScenarioMetadata {
    QString name;
    QString description;
    QString author;
    QString createdDate;
    QString modifiedDate;
    QString version = "1.0";
};

/**
 * @brief Scenario file data (what gets saved/loaded)
 */
struct ScenarioFileData {
    ScenarioMetadata metadata;
    std::vector<ForceUnit> units;
    
    // Simulation settings
    int maxTicks = 3600;
    int defaultIterations = 1000;
    
    // Map bounds
    double minLat = 49.0;
    double maxLat = 51.0;
    double minLon = 9.0;
    double maxLon = 11.0;
};

/**
 * @brief Handles scenario file I/O
 * 
 * File format (.athena or .json):
 * {
 *   "athena_version": "0.6.7",
 *   "metadata": { ... },
 *   "simulation": { ... },
 *   "bounds": { ... },
 *   "forces": {
 *     "blufor": [ ... ],
 *     "opfor": [ ... ]
 *   }
 * }
 */
class ScenarioFile
{
public:
    ScenarioFile() = default;
    
    /**
     * @brief Save scenario to file
     * @param filePath Path to save (*.athena or *.json)
     * @param data Scenario data to save
     * @return true on success
     */
    bool save(const QString& filePath, const ScenarioFileData& data);
    
    /**
     * @brief Load scenario from file
     * @param filePath Path to load
     * @param data Output: loaded scenario data
     * @return true on success
     */
    bool load(const QString& filePath, ScenarioFileData& data);
    
    /**
     * @brief Get last error message
     */
    QString lastError() const { return m_lastError; }
    
    /**
     * @brief Get file filter for dialogs
     */
    static QString fileFilter() { 
        return "ATHENA Scenario (*.athena);;JSON Files (*.json);;All Files (*)"; 
    }
    
    /**
     * @brief Get default extension
     */
    static QString defaultExtension() { return ".athena"; }

private:
    QString m_lastError;
};

} // namespace athena::ui

#endif // ATHENA_UI_SCENARIOFILE_HPP
