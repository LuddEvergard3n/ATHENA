/**
 * @file formationtemplates.hpp
 * @brief Formation templates for unit positioning
 * 
 * v0.7.1: Initial implementation - line, column, wedge, vee, echelon
 */

#ifndef ATHENA_UI_FORMATIONTEMPLATES_HPP
#define ATHENA_UI_FORMATIONTEMPLATES_HPP

#include <QPointF>
#include <QVector>
#include <QString>

namespace athena::ui {

/**
 * @brief Available formation types
 */
enum class FormationType {
    Line,       // Units in a horizontal line
    Column,     // Units in a vertical column
    Wedge,      // V-shaped with point forward
    Vee,        // V-shaped with opening forward
    Echelon,    // Diagonal line
    Box,        // Rectangular arrangement
    Circle      // Circular arrangement
};

/**
 * @brief Parameters for formation generation
 */
struct FormationParams {
    QPointF center;           // Center point of formation
    double spacing = 0.01;    // Distance between units (in geo degrees, ~1km)
    double heading = 0.0;     // Direction formation faces (degrees, 0=north)
    int unitCount = 1;        // Number of units to arrange
};

/**
 * @brief Utility class for generating unit formations
 * 
 * Calculates positions for units in various military formations.
 * All positions are in geographic coordinates (lat/lon).
 */
class FormationTemplates
{
public:
    /**
     * @brief Generate positions for a formation
     * @param type Formation type
     * @param params Formation parameters
     * @return Vector of positions (lat, lon)
     */
    static QVector<QPointF> generate(FormationType type, const FormationParams& params);

    /**
     * @brief Get display name for formation type
     */
    static QString typeName(FormationType type);

    /**
     * @brief Get icon name for formation type (for future toolbar icons)
     */
    static QString iconName(FormationType type);

private:
    static QVector<QPointF> generateLine(const FormationParams& params);
    static QVector<QPointF> generateColumn(const FormationParams& params);
    static QVector<QPointF> generateWedge(const FormationParams& params);
    static QVector<QPointF> generateVee(const FormationParams& params);
    static QVector<QPointF> generateEchelon(const FormationParams& params);
    static QVector<QPointF> generateBox(const FormationParams& params);
    static QVector<QPointF> generateCircle(const FormationParams& params);

    /**
     * @brief Rotate point around origin by heading
     */
    static QPointF rotatePoint(const QPointF& point, double headingDeg);
};

} // namespace athena::ui

#endif // ATHENA_UI_FORMATIONTEMPLATES_HPP
