/**
 * @file platformdetails.hpp
 * @brief Panel showing detailed platform specifications
 * 
 * v0.6.12: Initial implementation
 */

#ifndef ATHENA_UI_PLATFORMDETAILS_HPP
#define ATHENA_UI_PLATFORMDETAILS_HPP

#include <QWidget>
#include <QString>
#include <QJsonObject>

class QLabel;
class QTextBrowser;
class QTabWidget;
class QGroupBox;

namespace athena::ui {

/**
 * @brief Panel displaying detailed platform specifications
 * 
 * Shows comprehensive information about a selected platform:
 * - General info (name, type, country, crew)
 * - Mobility (speed, range, engine)
 * - Armament (weapons, ammunition)
 * - Protection (armor, APS, ERA)
 * - Sensors (thermals, radar, rangefinder)
 */
class PlatformDetailsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PlatformDetailsPanel(QWidget* parent = nullptr);
    ~PlatformDetailsPanel() override = default;

    /**
     * @brief Set the data path for loading platform JSON files
     */
    void setDataPath(const QString& path);

public slots:
    /**
     * @brief Display details for a platform by ID
     */
    void showPlatform(const QString& platformId);
    
    /**
     * @brief Clear the panel
     */
    void clear();

private:
    void setupUi();
    void loadPlatformData(const QString& platformId);
    void displayPlatformData(const QJsonObject& data);
    
    QString formatMobility(const QJsonObject& data);
    QString formatArmament(const QJsonObject& data);
    QString formatProtection(const QJsonObject& data);
    QString formatSensors(const QJsonObject& data);
    QString formatServiceHistory(const QJsonObject& data);

    QString m_dataPath;
    
    // UI Elements
    QLabel* m_titleLabel = nullptr;
    QLabel* m_typeLabel = nullptr;
    QLabel* m_countryLabel = nullptr;
    QLabel* m_imageLabel = nullptr;
    
    QTabWidget* m_tabWidget = nullptr;
    QTextBrowser* m_generalTab = nullptr;
    QTextBrowser* m_mobilityTab = nullptr;
    QTextBrowser* m_armamentTab = nullptr;
    QTextBrowser* m_protectionTab = nullptr;
    QTextBrowser* m_sensorsTab = nullptr;
};

} // namespace athena::ui

#endif // ATHENA_UI_PLATFORMDETAILS_HPP
