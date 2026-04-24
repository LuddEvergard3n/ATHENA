/**
 * @file mapscene.hpp
 * @brief Graphics scene for tactical map
 * 
 * v0.6.8: Unit management and geo coordinate conversion
 * v0.6.10: Unit context menu handling
 */

#ifndef ATHENA_UI_MAPSCENE_HPP
#define ATHENA_UI_MAPSCENE_HPP

#include <QGraphicsScene>
#include <QMap>
#include <memory>

namespace athena::ui {

class UnitItem;
class TerrainLayer;
class EngagementLines;
struct ForceUnit;
struct Engagement;

/**
 * @brief Map bounds in geographic coordinates
 */
struct GeoBounds {
    double minLat = 49.0;
    double maxLat = 51.0;
    double minLon = 9.0;
    double maxLon = 11.0;
    
    double latRange() const { return maxLat - minLat; }
    double lonRange() const { return maxLon - minLon; }
    double centerLat() const { return (minLat + maxLat) / 2.0; }
    double centerLon() const { return (minLon + maxLon) / 2.0; }
};

/**
 * @brief Custom graphics scene for the tactical map
 * 
 * Features:
 * - Geo to scene coordinate conversion
 * - Unit item management
 * - Layer visibility control
 * - Grid background
 * - Unit context menu handling
 */
class MapScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit MapScene(QObject* parent = nullptr);
    ~MapScene() override;

    // Layer visibility
    void setGridVisible(bool visible);
    void setUnitsVisible(bool visible);
    void setHeatmapVisible(bool visible);
    void setEngagementsVisible(bool visible);
    
    // Engagement lines
    void setEngagements(const QVector<Engagement>& engagements);
    void clearEngagements();
    
    // Bounds
    void setBounds(const GeoBounds& bounds);
    GeoBounds bounds() const { return m_bounds; }
    
    // Coordinate conversion
    QPointF geoToScene(double lat, double lon) const;
    void sceneToGeo(const QPointF& scenePos, double& lat, double& lon) const;
    
    // Unit management
    UnitItem* addUnit(const ForceUnit& unit);
    void removeUnit(const QString& unitId);
    void clearUnits();
    UnitItem* findUnit(const QString& unitId);
    QList<UnitItem*> allUnits() const;
    
    // Load units from force composition
    void loadUnits(const std::vector<ForceUnit>& units);
    
    // Get units for saving
    std::vector<ForceUnit> getUnits() const;
    
    /**
     * @brief Add unit from ForceTree (doesn't emit unitAdded)
     */
    void addUnitFromTree(const ForceUnit& unit);
    
    // Terrain
    void generateTerrain(int width = 100, int height = 100, uint32_t seed = 0);
    void setTerrainVisible(bool visible);
    
    // Selection
    QList<UnitItem*> selectedUnits() const;
    void selectAll();
    void clearSelection();
    void deleteSelectedUnits();
    void duplicateSelectedUnits();
    void changeSelectedUnitsSide();
    
    // Measurement
    void setMeasureMode(bool enabled);
    bool isMeasureMode() const { return m_measureMode; }
    void clearMeasurement();
    double measureDistance(double lat1, double lon1, double lat2, double lon2) const;

signals:
    void unitMoved(const QString& unitId, double lat, double lon);
    void unitSelected(const QString& unitId);
    void unitAdded(const ForceUnit& unit);
    void unitRemoved(const QString& unitId);
    void unitDeleted(const QString& unitId, const QString& platformId, bool wasBlufor);
    void unitDuplicated(const ForceUnit& unit);
    void unitSideChanged(const QString& unitId, bool nowBlufor);
    void unitQuantityChanged(const QString& unitId, int oldQuantity, int newQuantity);
    void measurementChanged(double distanceKm);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

private slots:
    void onUnitPositionChanged(const QString& unitId, double lat, double lon);
    void onUnitSelected(const QString& unitId);
    
    // Context menu handlers
    void onUnitDeleteRequested(const QString& unitId);
    void onUnitDuplicateRequested(const QString& unitId);
    void onUnitChangeSideRequested(const QString& unitId);
    void onUnitEditQuantityRequested(const QString& unitId);

private:
    void updateUnitScenePosition(UnitItem* unit);
    void connectUnitSignals(UnitItem* unit);
    
    bool m_gridVisible = true;
    bool m_unitsVisible = true;
    bool m_heatmapVisible = false;
    
    GeoBounds m_bounds;
    
    // Scene size (in pixels)
    static constexpr double SCENE_SIZE = 2000.0;
    
    // Unit storage
    QMap<QString, UnitItem*> m_units;
    int m_nextUnitId = 1;
    
    // Terrain
    TerrainLayer* m_terrainLayer = nullptr;
    
    // Engagement visualization
    EngagementLines* m_engagementLines = nullptr;
    
    // Measurement
    bool m_measureMode = false;
    bool m_measureStartSet = false;
    QPointF m_measureStart;
    QPointF m_measureEnd;
    double m_measureStartLat = 0.0;
    double m_measureStartLon = 0.0;
    double m_measureEndLat = 0.0;
    double m_measureEndLon = 0.0;
};

} // namespace athena::ui

#endif // ATHENA_UI_MAPSCENE_HPP
