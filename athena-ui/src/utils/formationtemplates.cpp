/**
 * @file formationtemplates.cpp
 * @brief Formation templates implementation
 * 
 * v0.7.1: Line, column, wedge, vee, echelon, box, circle formations
 */

#include "formationtemplates.hpp"

#include <cmath>
#include <QtMath>

namespace athena::ui {

QVector<QPointF> FormationTemplates::generate(FormationType type, const FormationParams& params)
{
    switch (type) {
        case FormationType::Line:
            return generateLine(params);
        case FormationType::Column:
            return generateColumn(params);
        case FormationType::Wedge:
            return generateWedge(params);
        case FormationType::Vee:
            return generateVee(params);
        case FormationType::Echelon:
            return generateEchelon(params);
        case FormationType::Box:
            return generateBox(params);
        case FormationType::Circle:
            return generateCircle(params);
    }
    return {};
}

QString FormationTemplates::typeName(FormationType type)
{
    switch (type) {
        case FormationType::Line:    return "Line";
        case FormationType::Column:  return "Column";
        case FormationType::Wedge:   return "Wedge";
        case FormationType::Vee:     return "Vee";
        case FormationType::Echelon: return "Echelon";
        case FormationType::Box:     return "Box";
        case FormationType::Circle:  return "Circle";
    }
    return "Unknown";
}

QString FormationTemplates::iconName(FormationType type)
{
    switch (type) {
        case FormationType::Line:    return "formation-line";
        case FormationType::Column:  return "formation-column";
        case FormationType::Wedge:   return "formation-wedge";
        case FormationType::Vee:     return "formation-vee";
        case FormationType::Echelon: return "formation-echelon";
        case FormationType::Box:     return "formation-box";
        case FormationType::Circle:  return "formation-circle";
    }
    return "formation-unknown";
}

QPointF FormationTemplates::rotatePoint(const QPointF& point, double headingDeg)
{
    // Convert heading to radians (heading is clockwise from north)
    const double rad = qDegreesToRadians(headingDeg);
    const double cosH = std::cos(rad);
    const double sinH = std::sin(rad);
    
    // Rotate: x' = x*cos - y*sin, y' = x*sin + y*cos
    // But for heading (clockwise from north), adjust
    return QPointF(
        point.x() * cosH + point.y() * sinH,
        -point.x() * sinH + point.y() * cosH
    );
}

QVector<QPointF> FormationTemplates::generateLine(const FormationParams& params)
{
    QVector<QPointF> positions;
    positions.reserve(params.unitCount);

    if (params.unitCount <= 0) return positions;

    // Line perpendicular to heading
    const double halfWidth = (params.unitCount - 1) * params.spacing / 2.0;

    for (int i = 0; i < params.unitCount; ++i) {
        // Position relative to center (perpendicular to heading)
        const double offset = -halfWidth + i * params.spacing;
        QPointF relPos(offset, 0);
        
        // Rotate by heading
        QPointF rotated = rotatePoint(relPos, params.heading);
        
        // Add center offset (lon = x, lat = y)
        positions.append(QPointF(
            params.center.x() + rotated.x(),
            params.center.y() + rotated.y()
        ));
    }

    return positions;
}

QVector<QPointF> FormationTemplates::generateColumn(const FormationParams& params)
{
    QVector<QPointF> positions;
    positions.reserve(params.unitCount);

    if (params.unitCount <= 0) return positions;

    // Column along heading direction
    const double halfDepth = (params.unitCount - 1) * params.spacing / 2.0;

    for (int i = 0; i < params.unitCount; ++i) {
        // Position relative to center (along heading)
        const double offset = halfDepth - i * params.spacing;
        QPointF relPos(0, offset);
        
        // Rotate by heading
        QPointF rotated = rotatePoint(relPos, params.heading);
        
        positions.append(QPointF(
            params.center.x() + rotated.x(),
            params.center.y() + rotated.y()
        ));
    }

    return positions;
}

QVector<QPointF> FormationTemplates::generateWedge(const FormationParams& params)
{
    QVector<QPointF> positions;
    positions.reserve(params.unitCount);

    if (params.unitCount <= 0) return positions;

    // Lead element at point, others spread behind
    // V shape with point forward (pointing in heading direction)
    
    positions.append(params.center);  // Leader at front
    
    int remaining = params.unitCount - 1;
    int row = 1;
    int placed = 0;
    
    while (placed < remaining) {
        // Each row has 2 more units than previous (2, 4, 6, ...)
        int unitsInRow = qMin(row * 2, remaining - placed);
        
        for (int i = 0; i < unitsInRow; ++i) {
            // Alternate left and right
            const double sideOffset = ((i / 2) + 0.5) * params.spacing * (i % 2 == 0 ? -1 : 1);
            const double backOffset = -row * params.spacing;
            
            QPointF relPos(sideOffset, backOffset);
            QPointF rotated = rotatePoint(relPos, params.heading);
            
            positions.append(QPointF(
                params.center.x() + rotated.x(),
                params.center.y() + rotated.y()
            ));
            
            placed++;
            if (placed >= remaining) break;
        }
        row++;
    }

    return positions;
}

QVector<QPointF> FormationTemplates::generateVee(const FormationParams& params)
{
    QVector<QPointF> positions;
    positions.reserve(params.unitCount);

    if (params.unitCount <= 0) return positions;

    // V shape with opening forward (inverse wedge)
    // Lead elements on flanks, trailing element at center rear
    
    const int halfCount = params.unitCount / 2;
    const bool hasCenter = (params.unitCount % 2 == 1);
    
    // Left flank (forward left to rear center)
    for (int i = 0; i < halfCount; ++i) {
        const double sideOffset = -(halfCount - i) * params.spacing;
        const double forwardOffset = (halfCount - i) * params.spacing;
        
        QPointF relPos(sideOffset, forwardOffset);
        QPointF rotated = rotatePoint(relPos, params.heading);
        
        positions.append(QPointF(
            params.center.x() + rotated.x(),
            params.center.y() + rotated.y()
        ));
    }
    
    // Center (if odd number)
    if (hasCenter) {
        positions.append(params.center);
    }
    
    // Right flank (rear center to forward right)
    for (int i = 0; i < halfCount; ++i) {
        const double sideOffset = (i + 1) * params.spacing;
        const double forwardOffset = (i + 1) * params.spacing;
        
        QPointF relPos(sideOffset, forwardOffset);
        QPointF rotated = rotatePoint(relPos, params.heading);
        
        positions.append(QPointF(
            params.center.x() + rotated.x(),
            params.center.y() + rotated.y()
        ));
    }

    return positions;
}

QVector<QPointF> FormationTemplates::generateEchelon(const FormationParams& params)
{
    QVector<QPointF> positions;
    positions.reserve(params.unitCount);

    if (params.unitCount <= 0) return positions;

    // Diagonal line (echelon right by default)
    // Each unit is behind and to the right of the previous
    
    for (int i = 0; i < params.unitCount; ++i) {
        const double sideOffset = i * params.spacing * 0.7;  // Right
        const double backOffset = -i * params.spacing * 0.7; // Back
        
        QPointF relPos(sideOffset, backOffset);
        QPointF rotated = rotatePoint(relPos, params.heading);
        
        positions.append(QPointF(
            params.center.x() + rotated.x(),
            params.center.y() + rotated.y()
        ));
    }

    return positions;
}

QVector<QPointF> FormationTemplates::generateBox(const FormationParams& params)
{
    QVector<QPointF> positions;
    positions.reserve(params.unitCount);

    if (params.unitCount <= 0) return positions;

    // Arrange in a square/rectangular grid
    const int cols = static_cast<int>(std::ceil(std::sqrt(params.unitCount)));
    const int rows = (params.unitCount + cols - 1) / cols;
    
    const double halfWidth = (cols - 1) * params.spacing / 2.0;
    const double halfHeight = (rows - 1) * params.spacing / 2.0;
    
    int placed = 0;
    for (int r = 0; r < rows && placed < params.unitCount; ++r) {
        for (int c = 0; c < cols && placed < params.unitCount; ++c) {
            const double x = -halfWidth + c * params.spacing;
            const double y = halfHeight - r * params.spacing;
            
            QPointF relPos(x, y);
            QPointF rotated = rotatePoint(relPos, params.heading);
            
            positions.append(QPointF(
                params.center.x() + rotated.x(),
                params.center.y() + rotated.y()
            ));
            
            placed++;
        }
    }

    return positions;
}

QVector<QPointF> FormationTemplates::generateCircle(const FormationParams& params)
{
    QVector<QPointF> positions;
    positions.reserve(params.unitCount);

    if (params.unitCount <= 0) return positions;
    
    if (params.unitCount == 1) {
        positions.append(params.center);
        return positions;
    }

    // Arrange in a circle
    const double radius = params.spacing * params.unitCount / (2.0 * M_PI);
    const double angleStep = 2.0 * M_PI / params.unitCount;
    
    for (int i = 0; i < params.unitCount; ++i) {
        // Start from top (north) and go clockwise
        const double angle = -M_PI / 2.0 + i * angleStep;
        const double x = radius * std::cos(angle);
        const double y = radius * std::sin(angle);
        
        QPointF relPos(x, y);
        QPointF rotated = rotatePoint(relPos, params.heading);
        
        positions.append(QPointF(
            params.center.x() + rotated.x(),
            params.center.y() + rotated.y()
        ));
    }

    return positions;
}

} // namespace athena::ui
