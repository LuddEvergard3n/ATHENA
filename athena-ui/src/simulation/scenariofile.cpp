/**
 * @file scenariofile.cpp
 * @brief Scenario file serialization implementation
 */

#include "scenariofile.hpp"
#include "models/forcemodel.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

namespace athena::ui {

// Current file format version
static const QString FILE_VERSION = "0.6.7";

bool ScenarioFile::save(const QString& filePath, const ScenarioFileData& data)
{
    m_lastError.clear();
    
    QJsonObject root;
    
    // File version
    root["athena_version"] = FILE_VERSION;
    
    // Metadata
    QJsonObject metadata;
    metadata["name"] = data.metadata.name;
    metadata["description"] = data.metadata.description;
    metadata["author"] = data.metadata.author;
    metadata["created"] = data.metadata.createdDate;
    metadata["modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["version"] = data.metadata.version;
    root["metadata"] = metadata;
    
    // Simulation settings
    QJsonObject simulation;
    simulation["max_ticks"] = data.maxTicks;
    simulation["default_iterations"] = data.defaultIterations;
    root["simulation"] = simulation;
    
    // Map bounds
    QJsonObject bounds;
    bounds["min_lat"] = data.minLat;
    bounds["max_lat"] = data.maxLat;
    bounds["min_lon"] = data.minLon;
    bounds["max_lon"] = data.maxLon;
    root["bounds"] = bounds;
    
    // Forces
    QJsonObject forces;
    QJsonArray bluforArray;
    QJsonArray opforArray;
    
    for (const auto& unit : data.units) {
        QJsonObject unitObj;
        unitObj["platform_id"] = unit.platformId;
        unitObj["display_name"] = unit.displayName;
        unitObj["quantity"] = unit.quantity;
        unitObj["lat"] = unit.lat;
        unitObj["lon"] = unit.lon;
        
        if (unit.isBlufor) {
            bluforArray.append(unitObj);
        } else {
            opforArray.append(unitObj);
        }
    }
    
    forces["blufor"] = bluforArray;
    forces["opfor"] = opforArray;
    root["forces"] = forces;
    
    // Write to file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file for writing: %1").arg(file.errorString());
        return false;
    }
    
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qDebug() << "ScenarioFile: Saved" << filePath 
             << "| BLUFOR:" << bluforArray.size() 
             << "| OPFOR:" << opforArray.size();
    
    return true;
}

bool ScenarioFile::load(const QString& filePath, ScenarioFileData& data)
{
    m_lastError.clear();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(file.errorString());
        return false;
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        m_lastError = QString("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }
    
    if (!doc.isObject()) {
        m_lastError = "Invalid file format: root is not an object";
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Check version
    QString fileVersion = root["athena_version"].toString();
    if (fileVersion.isEmpty()) {
        m_lastError = "Invalid file format: missing athena_version";
        return false;
    }
    
    // Metadata
    QJsonObject metadata = root["metadata"].toObject();
    data.metadata.name = metadata["name"].toString("Untitled");
    data.metadata.description = metadata["description"].toString();
    data.metadata.author = metadata["author"].toString();
    data.metadata.createdDate = metadata["created"].toString();
    data.metadata.modifiedDate = metadata["modified"].toString();
    data.metadata.version = metadata["version"].toString("1.0");
    
    // Simulation settings
    QJsonObject simulation = root["simulation"].toObject();
    data.maxTicks = simulation["max_ticks"].toInt(3600);
    data.defaultIterations = simulation["default_iterations"].toInt(1000);
    
    // Map bounds
    QJsonObject bounds = root["bounds"].toObject();
    data.minLat = bounds["min_lat"].toDouble(49.0);
    data.maxLat = bounds["max_lat"].toDouble(51.0);
    data.minLon = bounds["min_lon"].toDouble(9.0);
    data.maxLon = bounds["max_lon"].toDouble(11.0);
    
    // Forces
    data.units.clear();
    QJsonObject forces = root["forces"].toObject();
    
    // Load BLUFOR
    QJsonArray bluforArray = forces["blufor"].toArray();
    for (const auto& unitVal : bluforArray) {
        QJsonObject unitObj = unitVal.toObject();
        ForceUnit unit;
        unit.platformId = unitObj["platform_id"].toString();
        unit.displayName = unitObj["display_name"].toString();
        unit.quantity = unitObj["quantity"].toInt(1);
        unit.lat = unitObj["lat"].toDouble(50.0);
        unit.lon = unitObj["lon"].toDouble(10.0);
        unit.isBlufor = true;
        data.units.push_back(unit);
    }
    
    // Load OPFOR
    QJsonArray opforArray = forces["opfor"].toArray();
    for (const auto& unitVal : opforArray) {
        QJsonObject unitObj = unitVal.toObject();
        ForceUnit unit;
        unit.platformId = unitObj["platform_id"].toString();
        unit.displayName = unitObj["display_name"].toString();
        unit.quantity = unitObj["quantity"].toInt(1);
        unit.lat = unitObj["lat"].toDouble(49.5);
        unit.lon = unitObj["lon"].toDouble(10.0);
        unit.isBlufor = false;
        data.units.push_back(unit);
    }
    
    qDebug() << "ScenarioFile: Loaded" << filePath 
             << "| BLUFOR:" << bluforArray.size() 
             << "| OPFOR:" << opforArray.size();
    
    return true;
}

} // namespace athena::ui
