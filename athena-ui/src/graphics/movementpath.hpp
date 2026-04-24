/**
 * @file movementpath.hpp
 * @brief Unit movement waypoints and path visualization
 * 
 * v0.6.12: Initial implementation
 */

#ifndef ATHENA_UI_MOVEMENTPATH_HPP
#define ATHENA_UI_MOVEMENTPATH_HPP

#include <QGraphicsItem>
#include <QVector>
#include <QPointF>

namespace athena::ui {

class MapScene;
class UnitItem;

/**
 * @brief A single waypoint in a movement path
 */
struct Waypoint {
    double lat;
    double lon;
    int order;           // Sequence number
    bool reached = false;
    
    // Estimated arrival based on terrain
    double etaMinutes = 0.0;
    double distanceKm = 0.0;
};

/**
 * @brief Movement path for a unit with waypoints
 */
class MovementPath : public QGraphicsItem
{
public:
    explicit MovementPath(UnitItem* unit, MapScene* scene);
    ~MovementPath() override = default;
    
    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    /**
     * @brief Get the associated unit
     */
    UnitItem* unit() const { return m_unit; }
    
    /**
     * @brief Add a waypoint at geo coordinates
     */
    void addWaypoint(double lat, double lon);
    
    /**
     * @brief Insert waypoint at specific position
     */
    void insertWaypoint(int index, double lat, double lon);
    
    /**
     * @brief Remove waypoint by index
     */
    void removeWaypoint(int index);
    
    /**
     * @brief Clear all waypoints
     */
    void clearWaypoints();
    
    /**
     * @brief Get waypoint count
     */
    int waypointCount() const { return m_waypoints.size(); }
    
    /**
     * @brief Get waypoint at index
     */
    Waypoint waypointAt(int index) const;
    
    /**
     * @brief Get all waypoints
     */
    QVector<Waypoint> waypoints() const { return m_waypoints; }
    
    /**
     * @brief Calculate total path distance in km
     */
    double totalDistance() const;
    
    /**
     * @brief Calculate estimated time for entire path
     * @param speedKmh Base speed in km/h
     */
    double estimatedTime(double speedKmh) const;
    
    /**
     * @brief Update path calculations (after terrain changes)
     */
    void updatePath();
    
    /**
     * @brief Set whether to show distance labels
     */
    void setShowDistances(bool show);
    bool showDistances() const { return m_showDistances; }
    
    /**
     * @brief Set whether to show ETA labels
     */
    void setShowEta(bool show);
    bool showEta() const { return m_showEta; }
    
    /**
     * @brief Set path color
     */
    void setPathColor(const QColor& color);
    QColor pathColor() const { return m_pathColor; }
    
    /**
     * @brief Check if point is near a waypoint (for selection)
     * @return Index of waypoint or -1 if not near any
     */
    int waypointAtPoint(const QPointF& scenePos, double tolerance = 10.0) const;

private:
    void recalculateDistances();
    QPointF geoToScene(double lat, double lon) const;
    
    UnitItem* m_unit;
    MapScene* m_scene;
    QVector<Waypoint> m_waypoints;
    
    bool m_showDistances = true;
    bool m_showEta = true;
    QColor m_pathColor;
    
    static constexpr double WAYPOINT_RADIUS = 8.0;
};

/**
 * @brief Manager for all movement paths in the scene
 */
class MovementManager : public QObject
{
    Q_OBJECT

public:
    explicit MovementManager(MapScene* scene);
    ~MovementManager() override;
    
    /**
     * @brief Enable/disable movement order mode
     */
    void setOrderMode(bool enabled);
    bool isOrderMode() const { return m_orderMode; }
    
    /**
     * @brief Set the unit currently receiving orders
     */
    void setActiveUnit(UnitItem* unit);
    UnitItem* activeUnit() const { return m_activeUnit; }
    
    /**
     * @brief Get or create path for a unit
     */
    MovementPath* pathForUnit(UnitItem* unit);
    
    /**
     * @brief Remove path for a unit
     */
    void removePath(UnitItem* unit);
    
    /**
     * @brief Clear all paths
     */
    void clearAllPaths();
    
    /**
     * @brief Handle click in order mode
     */
    void handleClick(const QPointF& scenePos);
    
    /**
     * @brief Set default speed for ETA calculations
     */
    void setDefaultSpeed(double kmh) { m_defaultSpeed = kmh; }
    double defaultSpeed() const { return m_defaultSpeed; }

signals:
    void pathChanged(UnitItem* unit);
    void orderModeChanged(bool enabled);

private:
    MapScene* m_scene;
    QMap<UnitItem*, MovementPath*> m_paths;
    UnitItem* m_activeUnit = nullptr;
    bool m_orderMode = false;
    double m_defaultSpeed = 40.0;  // km/h
};

} // namespace athena::ui

#endif // ATHENA_UI_MOVEMENTPATH_HPP
