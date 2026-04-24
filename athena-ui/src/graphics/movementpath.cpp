/**
 * @file movementpath.cpp
 * @brief Movement path implementation
 * 
 * v0.7.0: Use centralized geo utilities
 */

#include "movementpath.hpp"
#include "mapscene.hpp"
#include "unititem.hpp"
#include "utils/geoutils.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QDebug>

namespace athena::ui {

// ============================================================================
// MovementPath
// ============================================================================

MovementPath::MovementPath(UnitItem* unit, MapScene* scene)
    : QGraphicsItem()
    , m_unit(unit)
    , m_scene(scene)
{
    setZValue(40);  // Below units, above terrain
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
    
    // Default color based on unit side
    if (unit && unit->side() == UnitItem::Side::Blue) {
        m_pathColor = QColor(30, 144, 255);  // DodgerBlue
    } else {
        m_pathColor = QColor(220, 20, 60);   // Crimson
    }
}

QRectF MovementPath::boundingRect() const
{
    return QRectF(-2000, -2000, 4000, 4000);  // Full scene
}

void MovementPath::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    if (!m_unit || m_waypoints.isEmpty()) return;
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    // Get unit position as start point
    QPointF startPos = geoToScene(m_unit->geoLat(), m_unit->geoLon());
    
    // Build path
    QPainterPath path;
    path.moveTo(startPos);
    
    QVector<QPointF> waypointPositions;
    for (const auto& wp : m_waypoints) {
        QPointF pos = geoToScene(wp.lat, wp.lon);
        waypointPositions.append(pos);
        path.lineTo(pos);
    }
    
    // Draw path line
    QPen pathPen(m_pathColor, 2, Qt::DashLine);
    pathPen.setDashPattern({8, 4});
    painter->setPen(pathPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
    
    // Draw direction arrows along path
    painter->setPen(QPen(m_pathColor, 1.5));
    painter->setBrush(m_pathColor);
    
    QPointF prevPos = startPos;
    for (int i = 0; i < waypointPositions.size(); ++i) {
        QPointF pos = waypointPositions[i];
        
        // Draw arrow at midpoint
        QPointF mid = (prevPos + pos) / 2;
        QPointF dir = pos - prevPos;
        double len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
        
        if (len > 30) {  // Only draw arrow if segment is long enough
            dir /= len;  // Normalize
            
            // Arrow head
            const double arrowSize = 8;
            QPointF p1 = mid + dir * arrowSize;
            QPointF perp(-dir.y(), dir.x());
            QPointF p2 = mid - dir * arrowSize / 2 + perp * arrowSize / 2;
            QPointF p3 = mid - dir * arrowSize / 2 - perp * arrowSize / 2;
            
            QPolygonF arrow;
            arrow << p1 << p2 << p3;
            painter->drawPolygon(arrow);
        }
        
        prevPos = pos;
    }
    
    // Draw waypoints
    for (int i = 0; i < waypointPositions.size(); ++i) {
        QPointF pos = waypointPositions[i];
        const auto& wp = m_waypoints[i];
        
        // Waypoint circle
        QColor wpColor = wp.reached ? m_pathColor.lighter(150) : m_pathColor;
        painter->setPen(QPen(Qt::white, 2));
        painter->setBrush(wpColor);
        painter->drawEllipse(pos, WAYPOINT_RADIUS, WAYPOINT_RADIUS);
        
        // Waypoint number
        painter->setPen(Qt::white);
        QFont font("Arial", 8, QFont::Bold);
        painter->setFont(font);
        painter->drawText(QRectF(pos.x() - WAYPOINT_RADIUS, pos.y() - WAYPOINT_RADIUS,
                                  WAYPOINT_RADIUS * 2, WAYPOINT_RADIUS * 2),
                          Qt::AlignCenter, QString::number(i + 1));
        
        // Distance/ETA labels
        if (m_showDistances || m_showEta) {
            QString label;
            if (m_showDistances) {
                label = QString("%1 km").arg(wp.distanceKm, 0, 'f', 1);
            }
            if (m_showEta && wp.etaMinutes > 0) {
                if (!label.isEmpty()) label += "\n";
                if (wp.etaMinutes < 60) {
                    label += QString("%1 min").arg(static_cast<int>(wp.etaMinutes));
                } else {
                    label += QString("%1h%2m")
                        .arg(static_cast<int>(wp.etaMinutes / 60))
                        .arg(static_cast<int>(std::fmod(wp.etaMinutes, 60.0)));
                }
            }
            
            if (!label.isEmpty()) {
                // Background for label
                QFont smallFont("Arial", 7);
                painter->setFont(smallFont);
                QFontMetrics fm(smallFont);
                QRect textRect = fm.boundingRect(QRect(), Qt::AlignCenter, label);
                textRect.moveCenter(QPoint(static_cast<int>(pos.x()), 
                                           static_cast<int>(pos.y() + WAYPOINT_RADIUS + 12)));
                textRect.adjust(-3, -1, 3, 1);
                
                painter->setBrush(QColor(255, 255, 255, 200));
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(textRect, 2, 2);
                
                painter->setPen(Qt::black);
                painter->drawText(textRect, Qt::AlignCenter, label);
            }
        }
    }
}

void MovementPath::addWaypoint(double lat, double lon)
{
    Waypoint wp;
    wp.lat = lat;
    wp.lon = lon;
    wp.order = m_waypoints.size();
    m_waypoints.append(wp);
    
    recalculateDistances();
    update();
}

void MovementPath::insertWaypoint(int index, double lat, double lon)
{
    if (index < 0) index = 0;
    if (index > m_waypoints.size()) index = m_waypoints.size();
    
    Waypoint wp;
    wp.lat = lat;
    wp.lon = lon;
    wp.order = index;
    m_waypoints.insert(index, wp);
    
    // Update order for subsequent waypoints
    for (int i = index + 1; i < m_waypoints.size(); ++i) {
        m_waypoints[i].order = i;
    }
    
    recalculateDistances();
    update();
}

void MovementPath::removeWaypoint(int index)
{
    if (index < 0 || index >= m_waypoints.size()) return;
    
    m_waypoints.remove(index);
    
    // Update order for remaining waypoints
    for (int i = index; i < m_waypoints.size(); ++i) {
        m_waypoints[i].order = i;
    }
    
    recalculateDistances();
    update();
}

void MovementPath::clearWaypoints()
{
    m_waypoints.clear();
    update();
}

Waypoint MovementPath::waypointAt(int index) const
{
    if (index < 0 || index >= m_waypoints.size()) {
        return Waypoint();
    }
    return m_waypoints[index];
}

double MovementPath::totalDistance() const
{
    if (m_waypoints.isEmpty()) return 0.0;
    return m_waypoints.last().distanceKm;
}

double MovementPath::estimatedTime(double speedKmh) const
{
    if (speedKmh <= 0 || m_waypoints.isEmpty()) return 0.0;
    return (totalDistance() / speedKmh) * 60.0;  // Return minutes
}

void MovementPath::updatePath()
{
    recalculateDistances();
    update();
}

void MovementPath::setShowDistances(bool show)
{
    m_showDistances = show;
    update();
}

void MovementPath::setShowEta(bool show)
{
    m_showEta = show;
    update();
}

void MovementPath::setPathColor(const QColor& color)
{
    m_pathColor = color;
    update();
}

int MovementPath::waypointAtPoint(const QPointF& scenePos, double tolerance) const
{
    for (int i = 0; i < m_waypoints.size(); ++i) {
        QPointF wpPos = geoToScene(m_waypoints[i].lat, m_waypoints[i].lon);
        double dist = QLineF(scenePos, wpPos).length();
        if (dist <= tolerance) {
            return i;
        }
    }
    return -1;
}

void MovementPath::recalculateDistances()
{
    if (!m_unit || m_waypoints.isEmpty()) return;
    
    double cumulativeDistance = 0.0;
    double prevLat = m_unit->geoLat();
    double prevLon = m_unit->geoLon();
    
    // Assume 40 km/h average speed for ETA
    const double avgSpeed = 40.0;
    
    for (auto& wp : m_waypoints) {
        double segmentDist = geo::haversineDistance(prevLat, prevLon, wp.lat, wp.lon);
        cumulativeDistance += segmentDist;
        wp.distanceKm = cumulativeDistance;
        wp.etaMinutes = (cumulativeDistance / avgSpeed) * 60.0;
        
        prevLat = wp.lat;
        prevLon = wp.lon;
    }
}

QPointF MovementPath::geoToScene(double lat, double lon) const
{
    if (m_scene) {
        return m_scene->geoToScene(lat, lon);
    }
    return QPointF();
}

// ============================================================================
// MovementManager
// ============================================================================

MovementManager::MovementManager(MapScene* scene)
    : QObject(scene)
    , m_scene(scene)
{
}

MovementManager::~MovementManager()
{
    clearAllPaths();
}

void MovementManager::setOrderMode(bool enabled)
{
    if (m_orderMode != enabled) {
        m_orderMode = enabled;
        if (!enabled) {
            m_activeUnit = nullptr;
        }
        emit orderModeChanged(enabled);
    }
}

void MovementManager::setActiveUnit(UnitItem* unit)
{
    m_activeUnit = unit;
}

MovementPath* MovementManager::pathForUnit(UnitItem* unit)
{
    if (!unit) return nullptr;
    
    auto it = m_paths.find(unit);
    if (it != m_paths.end()) {
        return it.value();
    }
    
    // Create new path
    auto* path = new MovementPath(unit, m_scene);
    m_scene->addItem(path);
    m_paths[unit] = path;
    
    return path;
}

void MovementManager::removePath(UnitItem* unit)
{
    auto it = m_paths.find(unit);
    if (it != m_paths.end()) {
        m_scene->removeItem(it.value());
        delete it.value();
        m_paths.erase(it);
    }
}

void MovementManager::clearAllPaths()
{
    for (auto* path : m_paths) {
        m_scene->removeItem(path);
        delete path;
    }
    m_paths.clear();
}

void MovementManager::handleClick(const QPointF& scenePos)
{
    if (!m_orderMode || !m_activeUnit || !m_scene) return;
    
    // Convert scene position to geo
    double lat, lon;
    m_scene->sceneToGeo(scenePos, lat, lon);
    
    // Get or create path for active unit
    MovementPath* path = pathForUnit(m_activeUnit);
    if (path) {
        // Check if clicking on existing waypoint (to remove it)
        int wpIndex = path->waypointAtPoint(scenePos);
        if (wpIndex >= 0) {
            path->removeWaypoint(wpIndex);
        } else {
            path->addWaypoint(lat, lon);
        }
        
        emit pathChanged(m_activeUnit);
    }
}

} // namespace athena::ui
