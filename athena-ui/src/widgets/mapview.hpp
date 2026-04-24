/**
 * @file mapview.hpp
 * @brief Tactical map view widget using QGraphicsView
 * 
 * v0.6.8: Unit deployment and drag-drop support
 */

#ifndef ATHENA_UI_MAPVIEW_HPP
#define ATHENA_UI_MAPVIEW_HPP

#include <QGraphicsView>
#include <QPointF>
#include <vector>

class QGraphicsScene;
class QWheelEvent;
class QMouseEvent;
class QKeyEvent;

namespace athena::ui {

class MapScene;
class TerrainLayer;
class UnitItem;
struct ForceUnit;
struct GeoBounds;

/**
 * @brief Tactical map visualization widget
 * 
 * Features:
 * - Smooth zoom with mouse wheel
 * - Pan by dragging
 * - Unit drag-drop deployment
 * - Coordinate display
 */
class MapView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit MapView(QWidget* parent = nullptr);
    ~MapView() override;

    /**
     * @brief Clear all units and reset view
     */
    void clear();

    /**
     * @brief Reset view to show entire scene
     */
    void resetView();

    // Zoom controls
    void zoomIn();
    void zoomOut();
    void setZoom(double factor);
    double zoom() const { return m_zoomFactor; }

    // Layer visibility
    void setGridVisible(bool visible);
    void setTerrainVisible(bool visible);
    void setUnitsVisible(bool visible);
    void setHeatmapVisible(bool visible);

    // Terrain
    bool loadTerrain(const QString& filePath);
    
    // Bounds
    void setBounds(double minLat, double maxLat, double minLon, double maxLon);
    
    // Unit management (delegates to MapScene)
    void loadUnits(const std::vector<ForceUnit>& units);
    std::vector<ForceUnit> getUnits() const;
    void clearUnits();
    
    /**
     * @brief Add unit from ForceTree sync (doesn't emit unitAdded)
     */
    void addUnitFromTree(const ForceUnit& unit);
    
    // Selection
    QList<UnitItem*> selectedUnits() const;
    void selectAllUnits();
    void deleteSelectedUnits();
    void duplicateSelectedUnits();
    void changeSelectedUnitsSide();
    void moveSelectedUnits(double dLat, double dLon);
    
    // Terrain
    void generateTerrain(int width = 100, int height = 100, uint32_t seed = 0);
    
    // Measurement
    void setMeasureMode(bool enabled);
    bool isMeasureMode() const;
    void clearMeasurement();
    
    // Get scene for direct access
    MapScene* mapScene() const { return m_scene; }

signals:
    void mousePositionChanged(const QPointF& scenePos);
    void geoPositionChanged(double lat, double lon);
    void unitSelected(const QString& unitId);
    void unitMoved(const QString& unitId, double lat, double lon);
    void unitAdded(const ForceUnit& unit);
    void unitRemoved(const QString& unitId);
    
    // Context menu signals
    void unitDeleted(const QString& unitId, const QString& platformId, bool wasBlufor);
    void unitDuplicated(const ForceUnit& unit);
    void unitSideChanged(const QString& unitId, bool nowBlufor);
    void unitQuantityChanged(const QString& unitId, int oldQuantity, int newQuantity);
    
    // Measurement
    void measurementChanged(double distanceKm);
    
    void zoomChanged(double factor);
    
    /**
     * @brief Emitted when viewport changes (scroll, zoom, resize)
     * @param viewportRect Visible area in scene coordinates
     */
    void viewportChanged(const QRectF& viewportRect);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void setupScene();
    void updateTransform();

    MapScene* m_scene = nullptr;
    TerrainLayer* m_terrainLayer = nullptr;

    // View state
    double m_zoomFactor = 1.0;
    bool m_panning = false;
    QPoint m_lastPanPoint;

    // Zoom limits
    static constexpr double MIN_ZOOM = 0.1;
    static constexpr double MAX_ZOOM = 10.0;
    static constexpr double ZOOM_STEP = 1.15;
};

} // namespace athena::ui

#endif // ATHENA_UI_MAPVIEW_HPP
