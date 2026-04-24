/**
 * @file mapview.cpp
 * @brief Tactical map view implementation
 * 
 * v0.6.8: Unit deployment and drag-drop
 * v0.6.11: Multi-selection with rubber band
 */

#include "mapview.hpp"
#include "graphics/mapscene.hpp"
#include "graphics/terrainlayer.hpp"
#include "graphics/unititem.hpp"
#include "models/forcemodel.hpp"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScrollBar>
#include <QDebug>

namespace athena::ui {

MapView::MapView(QWidget* parent)
    : QGraphicsView(parent)
{
    setupScene();
    
    // View settings
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);
    
    // Enable mouse tracking for position updates
    setMouseTracking(true);
    
    // Disable scrollbars (we handle panning manually)
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Set background
    setBackgroundBrush(QBrush(QColor(30, 30, 30)));
    
    // Enable rubber band selection
    setDragMode(QGraphicsView::RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    qDebug() << "MapView initialized with drop support";
}

MapView::~MapView() = default;

void MapView::clear()
{
    if (m_scene) {
        m_scene->clearUnits();
    }
    resetView();
}

void MapView::resetView()
{
    m_zoomFactor = 1.0;
    updateTransform();
    
    if (m_scene) {
        fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    }
    
    emit zoomChanged(m_zoomFactor);
}

void MapView::zoomIn()
{
    setZoom(m_zoomFactor * ZOOM_STEP);
}

void MapView::zoomOut()
{
    setZoom(m_zoomFactor / ZOOM_STEP);
}

void MapView::setZoom(double factor)
{
    factor = qBound(MIN_ZOOM, factor, MAX_ZOOM);
    
    if (qFuzzyCompare(factor, m_zoomFactor)) {
        return;
    }
    
    m_zoomFactor = factor;
    updateTransform();
    emit zoomChanged(m_zoomFactor);
}

void MapView::setGridVisible(bool visible)
{
    if (m_scene) {
        m_scene->setGridVisible(visible);
    }
}

void MapView::setTerrainVisible(bool visible)
{
    if (m_terrainLayer) {
        m_terrainLayer->setVisible(visible);
    }
}

void MapView::setUnitsVisible(bool visible)
{
    if (m_scene) {
        m_scene->setUnitsVisible(visible);
    }
}

void MapView::setHeatmapVisible(bool visible)
{
    if (m_scene) {
        m_scene->setHeatmapVisible(visible);
    }
}

bool MapView::loadTerrain(const QString& filePath)
{
    Q_UNUSED(filePath)
    qDebug() << "Loading terrain:" << filePath;
    return true;
}

void MapView::setBounds(double minLat, double maxLat, double minLon, double maxLon)
{
    if (m_scene) {
        GeoBounds bounds;
        bounds.minLat = minLat;
        bounds.maxLat = maxLat;
        bounds.minLon = minLon;
        bounds.maxLon = maxLon;
        m_scene->setBounds(bounds);
    }
}

void MapView::loadUnits(const std::vector<ForceUnit>& units)
{
    if (m_scene) {
        m_scene->loadUnits(units);
    }
}

std::vector<ForceUnit> MapView::getUnits() const
{
    if (m_scene) {
        return m_scene->getUnits();
    }
    return {};
}

void MapView::clearUnits()
{
    if (m_scene) {
        m_scene->clearUnits();
    }
}

void MapView::addUnitFromTree(const ForceUnit& unit)
{
    if (m_scene) {
        m_scene->addUnitFromTree(unit);
    }
}

QList<UnitItem*> MapView::selectedUnits() const
{
    return m_scene ? m_scene->selectedUnits() : QList<UnitItem*>();
}

void MapView::selectAllUnits()
{
    if (m_scene) {
        m_scene->selectAll();
    }
}

void MapView::deleteSelectedUnits()
{
    if (m_scene) {
        m_scene->deleteSelectedUnits();
    }
}

void MapView::duplicateSelectedUnits()
{
    if (m_scene) {
        m_scene->duplicateSelectedUnits();
    }
}

void MapView::changeSelectedUnitsSide()
{
    if (m_scene) {
        m_scene->changeSelectedUnitsSide();
    }
}

void MapView::moveSelectedUnits(double dLat, double dLon)
{
    if (!m_scene) return;
    
    QList<UnitItem*> selected = m_scene->selectedUnits();
    if (selected.isEmpty()) return;
    
    for (UnitItem* unit : selected) {
        double newLat = unit->geoLat() + dLat;
        double newLon = unit->geoLon() + dLon;
        
        // Update position
        unit->setGeoPosition(newLat, newLon);
        QPointF scenePos = m_scene->geoToScene(newLat, newLon);
        unit->setPos(scenePos);
        
        // Emit move signal for undo/redo integration
        emit unitMoved(unit->unitId(), newLat, newLon);
    }
}

void MapView::generateTerrain(int width, int height, uint32_t seed)
{
    if (m_scene) {
        m_scene->generateTerrain(width, height, seed);
    }
}

void MapView::setMeasureMode(bool enabled)
{
    if (m_scene) {
        m_scene->setMeasureMode(enabled);
        
        // Change cursor when in measure mode
        if (enabled) {
            setCursor(Qt::CrossCursor);
            setDragMode(QGraphicsView::NoDrag);
        } else {
            setCursor(Qt::ArrowCursor);
            setDragMode(QGraphicsView::RubberBandDrag);
        }
    }
}

bool MapView::isMeasureMode() const
{
    return m_scene ? m_scene->isMeasureMode() : false;
}

void MapView::clearMeasurement()
{
    if (m_scene) {
        m_scene->clearMeasurement();
    }
}

void MapView::wheelEvent(QWheelEvent* event)
{
    const double angle = event->angleDelta().y();
    
    if (angle > 0) {
        zoomIn();
    } else if (angle < 0) {
        zoomOut();
    }
    
    event->accept();
}

void MapView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || 
        (event->button() == Qt::LeftButton && event->modifiers() & Qt::ControlModifier)) {
        m_panning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent* event)
{
    // Update mouse position
    const QPointF scenePos = mapToScene(event->pos());
    emit mousePositionChanged(scenePos);
    
    // Emit geo position
    if (m_scene) {
        double lat, lon;
        m_scene->sceneToGeo(scenePos, lat, lon);
        emit geoPositionChanged(lat, lon);
    }
    
    if (m_panning) {
        const QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();
        
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        
        event->accept();
        return;
    }
    
    QGraphicsView::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    
    QGraphicsView::mouseReleaseEvent(event);
}

void MapView::keyPressEvent(QKeyEvent* event)
{
    // Check for modifiers
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    
    switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            break;
        case Qt::Key_Minus:
            zoomOut();
            break;
        case Qt::Key_0:
            resetView();
            break;
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            deleteSelectedUnits();
            break;
        case Qt::Key_A:
            if (ctrl) {
                selectAllUnits();
            } else {
                QGraphicsView::keyPressEvent(event);
            }
            break;
        case Qt::Key_D:
            if (ctrl) {
                duplicateSelectedUnits();
            } else {
                QGraphicsView::keyPressEvent(event);
            }
            break;
        case Qt::Key_M:
            if (ctrl) {
                changeSelectedUnitsSide();
            } else {
                QGraphicsView::keyPressEvent(event);
            }
            break;
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Left:
        case Qt::Key_Right:
            {
                // Move selected units with arrow keys
                // Shift = larger step, no modifier = small step
                const bool shift = event->modifiers() & Qt::ShiftModifier;
                const double step = shift ? 0.01 : 0.001;  // ~1km or ~100m
                
                double dLat = 0, dLon = 0;
                switch (event->key()) {
                    case Qt::Key_Up:    dLat = step; break;
                    case Qt::Key_Down:  dLat = -step; break;
                    case Qt::Key_Left:  dLon = -step; break;
                    case Qt::Key_Right: dLon = step; break;
                }
                
                moveSelectedUnits(dLat, dLon);
            }
            break;
        default:
            QGraphicsView::keyPressEvent(event);
            break;
    }
}

void MapView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-athena-platform")) {
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragEnterEvent(event);
    }
}

void MapView::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-athena-platform")) {
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void MapView::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-athena-platform") && m_scene) {
        // Forward to scene at correct position
        const QPointF scenePos = mapToScene(event->position().toPoint());
        
        // Create a scene drop event
        QGraphicsSceneDragDropEvent sceneEvent(QEvent::GraphicsSceneDrop);
        sceneEvent.setScenePos(scenePos);
        sceneEvent.setMimeData(event->mimeData());
        
        // Let scene handle it
        m_scene->dropEvent(&sceneEvent);
        
        event->acceptProposedAction();
        
        qDebug() << "MapView: Forwarded drop to scene at" << scenePos;
    } else {
        QGraphicsView::dropEvent(event);
    }
}

void MapView::setupScene()
{
    m_scene = new MapScene(this);
    setScene(m_scene);
    
    // Note: TerrainLayer is created by MapScene
    // m_terrainLayer is used for external reference if needed
    
    // Connect scene signals
    connect(m_scene, &MapScene::unitMoved, this, &MapView::unitMoved);
    connect(m_scene, &MapScene::unitSelected, this, &MapView::unitSelected);
    connect(m_scene, &MapScene::unitAdded, this, &MapView::unitAdded);
    connect(m_scene, &MapScene::unitRemoved, this, &MapView::unitRemoved);
    
    // Context menu signals
    connect(m_scene, &MapScene::unitDeleted, this, &MapView::unitDeleted);
    connect(m_scene, &MapScene::unitDuplicated, this, &MapView::unitDuplicated);
    connect(m_scene, &MapScene::unitSideChanged, this, &MapView::unitSideChanged);
    connect(m_scene, &MapScene::unitQuantityChanged, this, &MapView::unitQuantityChanged);
    
    // Measurement
    connect(m_scene, &MapScene::measurementChanged, this, &MapView::measurementChanged);
    
    // Set default scene size
    m_scene->setSceneRect(-1000, -1000, 2000, 2000);
}

void MapView::updateTransform()
{
    QTransform transform;
    transform.scale(m_zoomFactor, m_zoomFactor);
    setTransform(transform);
    
    // Emit viewport changed after transform update
    emit viewportChanged(mapToScene(viewport()->rect()).boundingRect());
}

void MapView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    emit viewportChanged(mapToScene(viewport()->rect()).boundingRect());
}

void MapView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    emit viewportChanged(mapToScene(viewport()->rect()).boundingRect());
}

} // namespace athena::ui
