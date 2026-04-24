/**
 * @file platformdetails.cpp
 * @brief Platform details panel implementation
 */

#include "platformdetails.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QTabWidget>
#include <QGroupBox>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace athena::ui {

PlatformDetailsPanel::PlatformDetailsPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void PlatformDetailsPanel::setDataPath(const QString& path)
{
    m_dataPath = path;
}

void PlatformDetailsPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    
    // Header section
    auto* headerLayout = new QVBoxLayout();
    
    m_titleLabel = new QLabel("No Platform Selected");
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    headerLayout->addWidget(m_titleLabel);
    
    m_typeLabel = new QLabel("");
    m_typeLabel->setStyleSheet("color: #666;");
    headerLayout->addWidget(m_typeLabel);
    
    m_countryLabel = new QLabel("");
    m_countryLabel->setStyleSheet("color: #888;");
    headerLayout->addWidget(m_countryLabel);
    
    layout->addLayout(headerLayout);
    
    // Tab widget for detailed specs
    m_tabWidget = new QTabWidget();
    m_tabWidget->setDocumentMode(true);
    
    // General tab
    m_generalTab = new QTextBrowser();
    m_generalTab->setOpenExternalLinks(false);
    m_tabWidget->addTab(m_generalTab, "General");
    
    // Mobility tab
    m_mobilityTab = new QTextBrowser();
    m_tabWidget->addTab(m_mobilityTab, "Mobility");
    
    // Armament tab
    m_armamentTab = new QTextBrowser();
    m_tabWidget->addTab(m_armamentTab, "Armament");
    
    // Protection tab
    m_protectionTab = new QTextBrowser();
    m_tabWidget->addTab(m_protectionTab, "Protection");
    
    // Sensors tab
    m_sensorsTab = new QTextBrowser();
    m_tabWidget->addTab(m_sensorsTab, "Sensors");
    
    layout->addWidget(m_tabWidget, 1);
}

void PlatformDetailsPanel::showPlatform(const QString& platformId)
{
    if (platformId.isEmpty()) {
        clear();
        return;
    }
    
    loadPlatformData(platformId);
}

void PlatformDetailsPanel::clear()
{
    m_titleLabel->setText("No Platform Selected");
    m_typeLabel->setText("");
    m_countryLabel->setText("");
    
    m_generalTab->clear();
    m_mobilityTab->clear();
    m_armamentTab->clear();
    m_protectionTab->clear();
    m_sensorsTab->clear();
}

void PlatformDetailsPanel::loadPlatformData(const QString& platformId)
{
    if (m_dataPath.isEmpty()) {
        qWarning() << "PlatformDetailsPanel: No data path set";
        return;
    }
    
    // Search for platform in JSON files
    QDir dataDir(m_dataPath);
    QStringList subdirs = dataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString& subdir : subdirs) {
        QDir categoryDir(dataDir.filePath(subdir));
        QStringList jsonFiles = categoryDir.entryList({"*.json"}, QDir::Files);
        
        for (const QString& jsonFile : jsonFiles) {
            QFile file(categoryDir.filePath(jsonFile));
            if (!file.open(QIODevice::ReadOnly)) continue;
            
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (!doc.isObject()) continue;
            
            QJsonObject root = doc.object();
            QJsonArray platforms = root["platforms"].toArray();
            
            for (const QJsonValue& val : platforms) {
                QJsonObject platform = val.toObject();
                if (platform["id"].toString() == platformId) {
                    displayPlatformData(platform);
                    return;
                }
            }
        }
    }
    
    // Platform not found
    m_titleLabel->setText(platformId);
    m_typeLabel->setText("Platform data not found");
    m_countryLabel->setText("");
}

void PlatformDetailsPanel::displayPlatformData(const QJsonObject& data)
{
    // Header
    m_titleLabel->setText(data["name"].toString());
    m_typeLabel->setText(data["type"].toString());
    m_countryLabel->setText(data["country"].toString());
    
    // General tab
    QString general;
    general += "<h3>Overview</h3>";
    general += QString("<p><b>ID:</b> %1</p>").arg(data["id"].toString());
    general += QString("<p><b>Type:</b> %1</p>").arg(data["type"].toString());
    general += QString("<p><b>Country:</b> %1</p>").arg(data["country"].toString());
    
    if (data.contains("crew")) {
        general += QString("<p><b>Crew:</b> %1</p>").arg(data["crew"].toInt());
    }
    if (data.contains("weight_tons")) {
        general += QString("<p><b>Weight:</b> %1 tons</p>").arg(data["weight_tons"].toDouble(), 0, 'f', 1);
    }
    
    general += formatServiceHistory(data);
    m_generalTab->setHtml(general);
    
    // Mobility tab
    m_mobilityTab->setHtml(formatMobility(data));
    
    // Armament tab
    m_armamentTab->setHtml(formatArmament(data));
    
    // Protection tab
    m_protectionTab->setHtml(formatProtection(data));
    
    // Sensors tab
    m_sensorsTab->setHtml(formatSensors(data));
}

QString PlatformDetailsPanel::formatMobility(const QJsonObject& data)
{
    QString html = "<h3>Mobility</h3>";
    
    QJsonObject mobility = data["mobility"].toObject();
    if (mobility.isEmpty()) {
        return html + "<p>No mobility data available</p>";
    }
    
    if (mobility.contains("max_speed_kmh")) {
        html += QString("<p><b>Max Speed:</b> %1 km/h</p>")
            .arg(mobility["max_speed_kmh"].toInt());
    }
    if (mobility.contains("road_speed_kmh")) {
        html += QString("<p><b>Road Speed:</b> %1 km/h</p>")
            .arg(mobility["road_speed_kmh"].toInt());
    }
    if (mobility.contains("cross_country_speed_kmh")) {
        html += QString("<p><b>Cross-Country:</b> %1 km/h</p>")
            .arg(mobility["cross_country_speed_kmh"].toInt());
    }
    if (mobility.contains("range_km")) {
        html += QString("<p><b>Range:</b> %1 km</p>")
            .arg(mobility["range_km"].toInt());
    }
    if (mobility.contains("engine_hp")) {
        html += QString("<p><b>Engine Power:</b> %1 hp</p>")
            .arg(mobility["engine_hp"].toInt());
    }
    if (mobility.contains("engine_type")) {
        html += QString("<p><b>Engine Type:</b> %1</p>")
            .arg(mobility["engine_type"].toString());
    }
    if (mobility.contains("power_to_weight")) {
        html += QString("<p><b>Power/Weight:</b> %1 hp/ton</p>")
            .arg(mobility["power_to_weight"].toDouble(), 0, 'f', 1);
    }
    
    return html;
}

QString PlatformDetailsPanel::formatArmament(const QJsonObject& data)
{
    QString html = "<h3>Armament</h3>";
    
    QJsonObject armament = data["armament"].toObject();
    if (armament.isEmpty()) {
        return html + "<p>No armament data available</p>";
    }
    
    // Main gun
    QJsonObject mainGun = armament["main_gun"].toObject();
    if (!mainGun.isEmpty()) {
        html += "<h4>Main Gun</h4>";
        if (mainGun.contains("caliber_mm")) {
            html += QString("<p><b>Caliber:</b> %1mm</p>")
                .arg(mainGun["caliber_mm"].toInt());
        }
        if (mainGun.contains("type")) {
            html += QString("<p><b>Type:</b> %1</p>")
                .arg(mainGun["type"].toString());
        }
        if (mainGun.contains("rounds")) {
            html += QString("<p><b>Ammo:</b> %1 rounds</p>")
                .arg(mainGun["rounds"].toInt());
        }
        if (mainGun.contains("effective_range_m")) {
            html += QString("<p><b>Effective Range:</b> %1m</p>")
                .arg(mainGun["effective_range_m"].toInt());
        }
        if (mainGun.contains("rate_of_fire")) {
            html += QString("<p><b>Rate of Fire:</b> %1 rpm</p>")
                .arg(mainGun["rate_of_fire"].toInt());
        }
    }
    
    // Secondary weapons
    QJsonArray secondary = armament["secondary"].toArray();
    if (!secondary.isEmpty()) {
        html += "<h4>Secondary Weapons</h4>";
        for (const QJsonValue& val : secondary) {
            QJsonObject weapon = val.toObject();
            html += QString("<p>• %1</p>").arg(weapon["type"].toString());
        }
    }
    
    // ATGM
    QJsonObject atgm = armament["atgm"].toObject();
    if (!atgm.isEmpty()) {
        html += "<h4>ATGM</h4>";
        if (atgm.contains("type")) {
            html += QString("<p><b>Type:</b> %1</p>").arg(atgm["type"].toString());
        }
        if (atgm.contains("range_m")) {
            html += QString("<p><b>Range:</b> %1m</p>").arg(atgm["range_m"].toInt());
        }
    }
    
    return html;
}

QString PlatformDetailsPanel::formatProtection(const QJsonObject& data)
{
    QString html = "<h3>Protection</h3>";
    
    QJsonObject protection = data["protection"].toObject();
    if (protection.isEmpty()) {
        return html + "<p>No protection data available</p>";
    }
    
    // Armor
    QJsonObject armor = protection["armor"].toObject();
    if (!armor.isEmpty()) {
        html += "<h4>Armor</h4>";
        if (armor.contains("type")) {
            html += QString("<p><b>Type:</b> %1</p>").arg(armor["type"].toString());
        }
        if (armor.contains("front_mm_rha")) {
            html += QString("<p><b>Front (RHA equiv):</b> %1mm</p>")
                .arg(armor["front_mm_rha"].toInt());
        }
        if (armor.contains("side_mm_rha")) {
            html += QString("<p><b>Side (RHA equiv):</b> %1mm</p>")
                .arg(armor["side_mm_rha"].toInt());
        }
    }
    
    // ERA
    if (protection.contains("era")) {
        QJsonObject era = protection["era"].toObject();
        html += "<h4>ERA</h4>";
        html += QString("<p><b>Type:</b> %1</p>").arg(era["type"].toString());
    }
    
    // APS
    if (protection.contains("aps")) {
        QJsonObject aps = protection["aps"].toObject();
        html += "<h4>Active Protection</h4>";
        html += QString("<p><b>System:</b> %1</p>").arg(aps["type"].toString());
    }
    
    // Smoke
    if (protection.contains("smoke_grenades")) {
        html += QString("<p><b>Smoke Grenades:</b> %1</p>")
            .arg(protection["smoke_grenades"].toInt());
    }
    
    return html;
}

QString PlatformDetailsPanel::formatSensors(const QJsonObject& data)
{
    QString html = "<h3>Sensors & Fire Control</h3>";
    
    QJsonObject sensors = data["sensors"].toObject();
    QJsonObject fireControl = data["fire_control"].toObject();
    
    if (sensors.isEmpty() && fireControl.isEmpty()) {
        return html + "<p>No sensor data available</p>";
    }
    
    // Sensors
    if (!sensors.isEmpty()) {
        html += "<h4>Sensors</h4>";
        
        if (sensors.contains("thermal_gen")) {
            html += QString("<p><b>Thermal:</b> Gen %1</p>")
                .arg(sensors["thermal_gen"].toInt());
        }
        if (sensors.contains("commander_thermal")) {
            html += QString("<p><b>Commander Thermal:</b> %1</p>")
                .arg(sensors["commander_thermal"].toBool() ? "Yes" : "No");
        }
        if (sensors.contains("laser_rangefinder")) {
            html += QString("<p><b>Laser Rangefinder:</b> %1</p>")
                .arg(sensors["laser_rangefinder"].toBool() ? "Yes" : "No");
        }
        if (sensors.contains("radar")) {
            html += QString("<p><b>Radar:</b> %1</p>")
                .arg(sensors["radar"].toString());
        }
    }
    
    // Fire Control
    if (!fireControl.isEmpty()) {
        html += "<h4>Fire Control</h4>";
        
        if (fireControl.contains("fcs_type")) {
            html += QString("<p><b>FCS:</b> %1</p>")
                .arg(fireControl["fcs_type"].toString());
        }
        if (fireControl.contains("hunter_killer")) {
            html += QString("<p><b>Hunter-Killer:</b> %1</p>")
                .arg(fireControl["hunter_killer"].toBool() ? "Yes" : "No");
        }
        if (fireControl.contains("auto_tracker")) {
            html += QString("<p><b>Auto Tracker:</b> %1</p>")
                .arg(fireControl["auto_tracker"].toBool() ? "Yes" : "No");
        }
    }
    
    return html;
}

QString PlatformDetailsPanel::formatServiceHistory(const QJsonObject& data)
{
    QString html;
    
    QJsonObject service = data["service"].toObject();
    if (service.isEmpty()) return html;
    
    html += "<h3>Service History</h3>";
    
    if (service.contains("entered_service")) {
        html += QString("<p><b>Entered Service:</b> %1</p>")
            .arg(service["entered_service"].toInt());
    }
    if (service.contains("produced")) {
        html += QString("<p><b>Units Produced:</b> %1</p>")
            .arg(service["produced"].toInt());
    }
    
    QJsonArray operators = service["operators"].toArray();
    if (!operators.isEmpty()) {
        html += "<p><b>Operators:</b> ";
        QStringList opList;
        for (const QJsonValue& val : operators) {
            opList << val.toString();
        }
        html += opList.join(", ") + "</p>";
    }
    
    return html;
}

} // namespace athena::ui
