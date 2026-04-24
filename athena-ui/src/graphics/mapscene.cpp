/**
 * @file mapscene.cpp
 * @brief Graphics scene implementation
 * 
 * v0.7.0: Use centralized geo utilities
 */

#include "mapscene.hpp"
#include "unititem.hpp"
#include "terrainlayer.hpp"
#include "engagementlines.hpp"
#include "models/forcemodel.hpp"
#include "utils/geoutils.hpp"

#include <QPainter>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMimeData>
#include <QInputDialog>
#include <QDebug>

namespace athena::ui {

MapScene::MapScene(QObject* parent)
    : QGraphicsScene(parent)
{
    // Set scene rect based on default bounds
    setSceneRect(-SCENE_SIZE/2, -SCENE_SIZE/2, SCENE_SIZE, SCENE_SIZE);
    
    // Create terrain layer (behind everything)
    m_terrainLayer = new TerrainLayer();
    m_terrainLayer->setSceneBounds(sceneRect());
    addItem(m_terrainLayer);
    
    // Create engagement lines layer (between terrain and units)
    m_engagementLines = new EngagementLines(this);
    addItem(m_engagementLines);
}

MapScene::~MapScene() = default;

void MapScene::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    update();
}

void MapScene::setUnitsVisible(bool visible)
{
    m_unitsVisible = visible;
    for (auto* unit : m_units) {
        unit->setVisible(visible);
    }
}

void MapScene::setHeatmapVisible(bool visible)
{
    m_heatmapVisible = visible;
    update();
}

void MapScene::setEngagementsVisible(bool visible)
{
    if (m_engagementLines) {
        m_engagementLines->setVisible(visible);
    }
}

void MapScene::setEngagements(const QVector<Engagement>& engagements)
{
    if (m_engagementLines) {
        m_engagementLines->setEngagements(engagements);
    }
}

void MapScene::clearEngagements()
{
    if (m_engagementLines) {
        m_engagementLines->clear();
    }
}

void MapScene::setBounds(const GeoBounds& bounds)
{
    m_bounds = bounds;
    
    // Update all unit positions
    for (auto* unit : m_units) {
        updateUnitScenePosition(unit);
    }
}

QPointF MapScene::geoToScene(double lat, double lon) const
{
    // Map geo coordinates to scene coordinates
    // lat increases northward (positive Y in scene goes down, so invert)
    // lon increases eastward (positive X)
    
    const double latNorm = (lat - m_bounds.minLat) / m_bounds.latRange();
    const double lonNorm = (lon - m_bounds.minLon) / m_bounds.lonRange();
    
    const double x = (lonNorm - 0.5) * SCENE_SIZE;
    const double y = (0.5 - latNorm) * SCENE_SIZE;  // Invert Y
    
    return QPointF(x, y);
}

void MapScene::sceneToGeo(const QPointF& scenePos, double& lat, double& lon) const
{
    const double lonNorm = scenePos.x() / SCENE_SIZE + 0.5;
    const double latNorm = 0.5 - scenePos.y() / SCENE_SIZE;
    
    lat = m_bounds.minLat + latNorm * m_bounds.latRange();
    lon = m_bounds.minLon + lonNorm * m_bounds.lonRange();
}

UnitItem* MapScene::addUnit(const ForceUnit& unit)
{
    const QString unitId = QString("unit_%1").arg(m_nextUnitId++);
    
    const auto side = unit.isBlufor ? UnitItem::Side::Blue : UnitItem::Side::Red;
    const auto type = UnitItem::detectType(unit.platformId);
    
    auto* item = new UnitItem(unitId, side, type);
    item->setPlatformId(unit.platformId);
    item->setDisplayName(unit.displayName);
    item->setQuantity(unit.quantity);
    item->setGeoPosition(unit.lat, unit.lon);
    
    // Connect all signals
    connectUnitSignals(item);
    
    // Position in scene
    updateUnitScenePosition(item);
    
    // Add to scene and storage
    addItem(item);
    m_units[unitId] = item;
    
    return item;
}

void MapScene::removeUnit(const QString& unitId)
{
    auto it = m_units.find(unitId);
    if (it != m_units.end()) {
        removeItem(*it);
        delete *it;
        m_units.erase(it);
    }
}

void MapScene::clearUnits()
{
    for (auto* unit : m_units) {
        removeItem(unit);
        delete unit;
    }
    m_units.clear();
    m_nextUnitId = 1;
}

UnitItem* MapScene::findUnit(const QString& unitId)
{
    auto it = m_units.find(unitId);
    return (it != m_units.end()) ? *it : nullptr;
}

QList<UnitItem*> MapScene::allUnits() const
{
    return m_units.values();
}

void MapScene::loadUnits(const std::vector<ForceUnit>& units)
{
    clearUnits();
    
    for (const auto& unit : units) {
        addUnit(unit);
    }
    
    qDebug() << "MapScene: Loaded" << m_units.size() << "units";
}

std::vector<ForceUnit> MapScene::getUnits() const
{
    std::vector<ForceUnit> units;
    
    for (auto* item : m_units) {
        ForceUnit unit;
        unit.platformId = item->platformId();
        unit.displayName = item->displayName();
        unit.quantity = item->quantity();
        unit.isBlufor = (item->side() == UnitItem::Side::Blue);
        unit.lat = item->geoLat();
        unit.lon = item->geoLon();
        units.push_back(unit);
    }
    
    return units;
}

void MapScene::addUnitFromTree(const ForceUnit& unit)
{
    // Same as addUnit but doesn't emit unitAdded (to avoid sync loop)
    const QString unitId = QString("unit_%1").arg(m_nextUnitId++);
    
    const auto side = unit.isBlufor ? UnitItem::Side::Blue : UnitItem::Side::Red;
    const auto type = UnitItem::detectType(unit.platformId);
    
    auto* item = new UnitItem(unitId, side, type);
    item->setPlatformId(unit.platformId);
    item->setDisplayName(unit.displayName);
    item->setQuantity(unit.quantity);
    item->setGeoPosition(unit.lat, unit.lon);
    
    // Connect all signals
    connectUnitSignals(item);
    
    updateUnitScenePosition(item);
    
    addItem(item);
    m_units[unitId] = item;
    
    qDebug() << "MapScene: Added from tree" << unit.platformId << "at" << unit.lat << "," << unit.lon;
}

void MapScene::generateTerrain(int width, int height, uint32_t seed)
{
    if (m_terrainLayer) {
        m_terrainLayer->generateTerrain(width, height, seed);
        qDebug() << "MapScene: Generated terrain" << width << "x" << height;
    }
}

void MapScene::setTerrainVisible(bool visible)
{
    if (m_terrainLayer) {
        m_terrainLayer->setVisible(visible);
    }
}

QList<UnitItem*> MapScene::selectedUnits() const
{
    QList<UnitItem*> selected;
    for (auto* item : m_units) {
        if (item->isSelected()) {
            selected.append(item);
        }
    }
    return selected;
}

void MapScene::selectAll()
{
    for (auto* item : m_units) {
        item->setSelected(true);
    }
}

void MapScene::clearSelection()
{
    for (auto* item : m_units) {
        item->setSelected(false);
    }
}

void MapScene::deleteSelectedUnits()
{
    QList<UnitItem*> selected = selectedUnits();
    for (auto* item : selected) {
        const QString unitId = item->unitId();
        const QString platformId = item->platformId();
        const bool wasBlufor = (item->side() == UnitItem::Side::Blue);
        
        removeUnit(unitId);
        emit unitDeleted(unitId, platformId, wasBlufor);
    }
    
    if (!selected.isEmpty()) {
        qDebug() << "MapScene: Deleted" << selected.size() << "selected units";
    }
}

void MapScene::duplicateSelectedUnits()
{
    QList<UnitItem*> selected = selectedUnits();
    double offset = 0.02;
    
    for (auto* item : selected) {
        ForceUnit newUnit;
        newUnit.platformId = item->platformId();
        newUnit.displayName = item->displayName();
        newUnit.quantity = item->quantity();
        newUnit.isBlufor = (item->side() == UnitItem::Side::Blue);
        newUnit.lat = item->geoLat() + offset;
        newUnit.lon = item->geoLon() + offset;
        
        addUnit(newUnit);
        emit unitDuplicated(newUnit);
        
        offset += 0.02;  // Cascade duplicates
    }
    
    if (!selected.isEmpty()) {
        qDebug() << "MapScene: Duplicated" << selected.size() << "selected units";
    }
}

void MapScene::changeSelectedUnitsSide()
{
    QList<UnitItem*> selected = selectedUnits();
    
    for (auto* item : selected) {
        const bool wasBlufor = (item->side() == UnitItem::Side::Blue);
        const auto newSide = wasBlufor ? UnitItem::Side::Red : UnitItem::Side::Blue;
        
        item->setSide(newSide);
        emit unitSideChanged(item->unitId(), !wasBlufor);
    }
    
    if (!selected.isEmpty()) {
        qDebug() << "MapScene: Changed side for" << selected.size() << "selected units";
    }
}

void MapScene::setMeasureMode(bool enabled)
{
    m_measureMode = enabled;
    if (!enabled) {
        clearMeasurement();
    }
}

void MapScene::clearMeasurement()
{
    m_measureStartSet = false;
    m_measureStart = QPointF();
    m_measureEnd = QPointF();
    update();
}

double MapScene::measureDistance(double lat1, double lon1, double lat2, double lon2) const
{
    return geo::haversineDistance(lat1, lon1, lat2, lon2);
}

void MapScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_measureMode && event->button() == Qt::LeftButton) {
        if (!m_measureStartSet) {
            // Set start point
            m_measureStart = event->scenePos();
            sceneToGeo(m_measureStart, m_measureStartLat, m_measureStartLon);
            m_measureStartSet = true;
            m_measureEnd = m_measureStart;
            m_measureEndLat = m_measureStartLat;
            m_measureEndLon = m_measureStartLon;
        } else {
            // Set end point and complete measurement
            m_measureEnd = event->scenePos();
            sceneToGeo(m_measureEnd, m_measureEndLat, m_measureEndLon);
            
            const double dist = measureDistance(m_measureStartLat, m_measureStartLon,
                                                m_measureEndLat, m_measureEndLon);
            emit measurementChanged(dist);
            
            // Reset for next measurement
            m_measureStartSet = false;
        }
        update();
        event->accept();
        return;
    }
    
    QGraphicsScene::mousePressEvent(event);
}

void MapScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_measureMode && m_measureStartSet) {
        m_measureEnd = event->scenePos();
        sceneToGeo(m_measureEnd, m_measureEndLat, m_measureEndLon);
        update();
    }
    
    QGraphicsScene::mouseMoveEvent(event);
}

void MapScene::drawForeground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect)
    
    // Draw measurement line if in measure mode
    if (m_measureMode && m_measureStartSet) {
        painter->setRenderHint(QPainter::Antialiasing);
        
        // Draw line
        QPen linePen(QColor(255, 100, 0), 2, Qt::DashLine);
        painter->setPen(linePen);
        painter->drawLine(m_measureStart, m_measureEnd);
        
        // Draw endpoints
        painter->setBrush(QColor(255, 100, 0));
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(m_measureStart, 5, 5);
        painter->drawEllipse(m_measureEnd, 5, 5);
        
        // Calculate and draw distance
        const double dist = measureDistance(m_measureStartLat, m_measureStartLon,
                                            m_measureEndLat, m_measureEndLon);
        
        // Draw label at midpoint
        const QPointF mid = (m_measureStart + m_measureEnd) / 2;
        
        QString label;
        if (dist < 1.0) {
            label = QString("%1 m").arg(dist * 1000, 0, 'f', 0);
        } else {
            label = QString("%1 km").arg(dist, 0, 'f', 2);
        }
        
        // Background for label
        QFont font("Arial", 10, QFont::Bold);
        painter->setFont(font);
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(label);
        textRect.moveCenter(mid.toPoint());
        textRect.adjust(-4, -2, 4, 2);
        
        painter->setBrush(QColor(255, 255, 255, 220));
        painter->setPen(QPen(QColor(255, 100, 0), 1));
        painter->drawRoundedRect(textRect, 3, 3);
        
        painter->setPen(QColor(0, 0, 0));
        painter->drawText(textRect, Qt::AlignCenter, label);
    }
}

void MapScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    // If terrain layer is visible, it handles the background
    if (m_terrainLayer && m_terrainLayer->isVisible() && m_terrainLayer->gridWidth() > 0) {
        // Just clear to a base color, terrain layer draws over it
        painter->fillRect(rect, QColor(100, 150, 200));  // Water color for areas outside terrain
    } else {
        // Default background
        painter->fillRect(rect, QColor(230, 235, 220));
    }
    
    if (!m_gridVisible) {
        return;
    }
    
    // Draw coordinate grid
    painter->setPen(QPen(QColor(200, 200, 200), 0.5));
    
    const double gridStep = SCENE_SIZE / 10.0;  // 10 divisions
    
    // Vertical lines
    for (double x = -SCENE_SIZE/2; x <= SCENE_SIZE/2; x += gridStep) {
        painter->drawLine(QPointF(x, -SCENE_SIZE/2), QPointF(x, SCENE_SIZE/2));
    }
    
    // Horizontal lines
    for (double y = -SCENE_SIZE/2; y <= SCENE_SIZE/2; y += gridStep) {
        painter->drawLine(QPointF(-SCENE_SIZE/2, y), QPointF(SCENE_SIZE/2, y));
    }
    
    // Draw coordinate labels
    painter->setPen(Qt::darkGray);
    painter->setFont(QFont("Arial", 8));
    
    const double latStep = m_bounds.latRange() / 10.0;
    const double lonStep = m_bounds.lonRange() / 10.0;
    
    for (int i = 0; i <= 10; ++i) {
        // Longitude labels (bottom)
        const double lon = m_bounds.minLon + i * lonStep;
        const QPointF pos = geoToScene(m_bounds.minLat, lon);
        painter->drawText(QPointF(pos.x() - 20, SCENE_SIZE/2 - 5), 
                         QString::number(lon, 'f', 2) + "°");
        
        // Latitude labels (left)
        const double lat = m_bounds.minLat + i * latStep;
        const QPointF pos2 = geoToScene(lat, m_bounds.minLon);
        painter->drawText(QPointF(-SCENE_SIZE/2 + 5, pos2.y() + 4), 
                         QString::number(lat, 'f', 2) + "°");
    }
    
    // Draw cardinal directions
    painter->setPen(QPen(Qt::darkGray, 1));
    painter->setFont(QFont("Arial", 12, QFont::Bold));
    painter->drawText(QPointF(-10, -SCENE_SIZE/2 + 20), "N");
}

void MapScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-athena-platform")) {
        event->acceptProposedAction();
    } else {
        QGraphicsScene::dragEnterEvent(event);
    }
}

void MapScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-athena-platform")) {
        event->acceptProposedAction();
    } else {
        QGraphicsScene::dragMoveEvent(event);
    }
}

void MapScene::dropEvent(QGraphicsSceneDragDropEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-athena-platform")) {
        const QString platformId = QString::fromUtf8(
            event->mimeData()->data("application/x-athena-platform"));
        
        // Convert drop position to geo
        double lat, lon;
        sceneToGeo(event->scenePos(), lat, lon);
        
        // Create unit
        ForceUnit unit;
        unit.platformId = platformId;
        unit.displayName = platformId;  // Will be updated if we have database
        unit.quantity = 1;
        unit.isBlufor = true;  // Default to BLUFOR for drops
        unit.lat = lat;
        unit.lon = lon;
        
        addUnit(unit);
        
        // Emit signal for ForceTree sync
        emit unitAdded(unit);
        
        qDebug() << "MapScene: Dropped" << platformId 
                 << "at" << lat << "," << lon;
        
        event->acceptProposedAction();
    } else {
        QGraphicsScene::dropEvent(event);
    }
}

void MapScene::onUnitPositionChanged(const QString& unitId, double lat, double lon)
{
    auto* unit = findUnit(unitId);
    if (unit) {
        // Update geo position from scene position
        double newLat, newLon;
        sceneToGeo(unit->pos(), newLat, newLon);
        unit->setGeoPosition(newLat, newLon);
        
        emit unitMoved(unitId, newLat, newLon);
    }
}

void MapScene::onUnitSelected(const QString& unitId)
{
    emit unitSelected(unitId);
}

void MapScene::updateUnitScenePosition(UnitItem* unit)
{
    if (unit) {
        const QPointF scenePos = geoToScene(unit->geoLat(), unit->geoLon());
        unit->setPos(scenePos);
    }
}

void MapScene::connectUnitSignals(UnitItem* unit)
{
    if (!unit) return;
    
    // Position and selection
    connect(unit, &UnitItem::positionChanged, this, &MapScene::onUnitPositionChanged);
    connect(unit, &UnitItem::selected, this, &MapScene::onUnitSelected);
    
    // Context menu actions
    connect(unit, &UnitItem::deleteRequested, this, &MapScene::onUnitDeleteRequested);
    connect(unit, &UnitItem::duplicateRequested, this, &MapScene::onUnitDuplicateRequested);
    connect(unit, &UnitItem::changeSideRequested, this, &MapScene::onUnitChangeSideRequested);
    connect(unit, &UnitItem::editQuantityRequested, this, &MapScene::onUnitEditQuantityRequested);
}

void MapScene::onUnitDeleteRequested(const QString& unitId)
{
    auto* unit = findUnit(unitId);
    if (unit) {
        const QString platformId = unit->platformId();
        const bool wasBlufor = (unit->side() == UnitItem::Side::Blue);
        
        // Remove from scene
        removeUnit(unitId);
        
        // Emit for ForceTree sync
        emit unitDeleted(unitId, platformId, wasBlufor);
        
        qDebug() << "MapScene: Deleted unit" << unitId;
    }
}

void MapScene::onUnitDuplicateRequested(const QString& unitId)
{
    auto* unit = findUnit(unitId);
    if (unit) {
        // Create ForceUnit from existing
        ForceUnit newUnit;
        newUnit.platformId = unit->platformId();
        newUnit.displayName = unit->displayName();
        newUnit.quantity = unit->quantity();
        newUnit.isBlufor = (unit->side() == UnitItem::Side::Blue);
        // Offset position slightly
        newUnit.lat = unit->geoLat() + 0.02;
        newUnit.lon = unit->geoLon() + 0.02;
        
        // Add new unit
        addUnit(newUnit);
        
        // Emit for ForceTree sync
        emit unitDuplicated(newUnit);
        
        qDebug() << "MapScene: Duplicated unit" << unitId;
    }
}

void MapScene::onUnitChangeSideRequested(const QString& unitId)
{
    auto* unit = findUnit(unitId);
    if (unit) {
        const bool wasBlufor = (unit->side() == UnitItem::Side::Blue);
        const auto newSide = wasBlufor ? UnitItem::Side::Red : UnitItem::Side::Blue;
        
        unit->setSide(newSide);
        
        // Emit for ForceTree sync
        emit unitSideChanged(unitId, !wasBlufor);
        
        qDebug() << "MapScene: Changed side for" << unitId << "to" << (!wasBlufor ? "BLUFOR" : "OPFOR");
    }
}

void MapScene::onUnitEditQuantityRequested(const QString& unitId)
{
    auto* unit = findUnit(unitId);
    if (unit) {
        bool ok;
        const int oldQty = unit->quantity();
        const int newQty = QInputDialog::getInt(
            nullptr,
            "Edit Quantity",
            QString("Quantity for %1:").arg(unit->displayName()),
            oldQty,
            1,      // min
            999,    // max
            1,      // step
            &ok
        );
        
        if (ok && newQty != oldQty) {
            unit->setQuantity(newQty);
            
            // Emit for ForceTree sync (includes old quantity for undo)
            emit unitQuantityChanged(unitId, oldQty, newQty);
            
            qDebug() << "MapScene: Changed quantity for" << unitId << "from" << oldQty << "to" << newQty;
        }
    }
}

} // namespace athena::ui
