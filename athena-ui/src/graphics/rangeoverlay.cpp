/**
 * @file rangeoverlay.cpp
 * @brief Weapon range overlay implementation
 */

#include "rangeoverlay.hpp"
#include "mapscene.hpp"
#include "unititem.hpp"

#include <QPainter>
#include <cmath>

namespace athena::ui {

RangeOverlay::RangeOverlay(MapScene* scene)
    : QGraphicsItem()
    , m_scene(scene)
{
    setZValue(50);  // Above terrain, below units
    setFlag(ItemIsSelectable, false);
    setFlag(ItemIsMovable, false);
}

QRectF RangeOverlay::boundingRect() const
{
    return QRectF(-m_sceneSize/2, -m_sceneSize/2, m_sceneSize, m_sceneSize);
}

void RangeOverlay::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    if (!m_scene || m_ranges.isEmpty()) return;
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    for (const auto& range : m_ranges) {
        // Filter by type
        if (m_visibleType != RangeType::All && range.type != m_visibleType) {
            continue;
        }
        
        // Filter by side
        if (range.isBlufor && !m_showBlufor) continue;
        if (!range.isBlufor && !m_showOpfor) continue;
        
        // Convert center to scene coordinates
        QPointF center = m_scene->geoToScene(range.centerLat, range.centerLon);
        
        // Convert range (km) to scene pixels
        // Approximate: 1 degree latitude ≈ 111 km
        // Scene is 2000 pixels for 2 degrees = 1000 pixels/degree = ~9 pixels/km
        const double pixelsPerKm = m_sceneSize / (m_scene->bounds().latRange() * 111.0);
        const double radiusPixels = range.rangeKm * pixelsPerKm;
        
        // Draw filled circle
        QColor fillColor = range.color;
        fillColor.setAlphaF(m_opacity);
        painter->setBrush(fillColor);
        
        // Draw border
        QColor borderColor = range.color;
        borderColor.setAlphaF(m_opacity + 0.3);
        painter->setPen(QPen(borderColor, 1.5, Qt::DashLine));
        
        painter->drawEllipse(center, radiusPixels, radiusPixels);
    }
}

void RangeOverlay::updateRanges()
{
    m_ranges.clear();
    
    if (!m_scene) return;
    
    QList<UnitItem*> units = m_scene->allUnits();
    for (UnitItem* unit : units) {
        addRangeForUnit(unit);
    }
    
    update();
}

void RangeOverlay::addRangeForUnit(UnitItem* unit)
{
    if (!unit) return;
    
    const QString platformId = unit->platformId();
    const bool isBlufor = (unit->side() == UnitItem::Side::Blue);
    const double lat = unit->geoLat();
    const double lon = unit->geoLon();
    
    // Add main gun range for armor/artillery
    if (unit->type() == UnitItem::UnitType::Armor || 
        unit->type() == UnitItem::UnitType::Artillery) {
        double range = getMainGunRange(platformId);
        if (range > 0) {
            RangeCircle circle;
            circle.unitId = unit->unitId();
            circle.centerLat = lat;
            circle.centerLon = lon;
            circle.rangeKm = range;
            circle.type = RangeType::MainGun;
            circle.color = colorForType(RangeType::MainGun, isBlufor);
            circle.isBlufor = isBlufor;
            m_ranges.append(circle);
        }
    }
    
    // Add ATGM range if applicable
    double atgmRange = getAtgmRange(platformId);
    if (atgmRange > 0) {
        RangeCircle circle;
        circle.unitId = unit->unitId();
        circle.centerLat = lat;
        circle.centerLon = lon;
        circle.rangeKm = atgmRange;
        circle.type = RangeType::ATGM;
        circle.color = colorForType(RangeType::ATGM, isBlufor);
        circle.isBlufor = isBlufor;
        m_ranges.append(circle);
    }
    
    // Add SAM range for air defense
    if (unit->type() == UnitItem::UnitType::AirDefense) {
        double samRange = getSamRange(platformId);
        if (samRange > 0) {
            RangeCircle circle;
            circle.unitId = unit->unitId();
            circle.centerLat = lat;
            circle.centerLon = lon;
            circle.rangeKm = samRange;
            circle.type = RangeType::SAM;
            circle.color = colorForType(RangeType::SAM, isBlufor);
            circle.isBlufor = isBlufor;
            m_ranges.append(circle);
        }
    }
}

QColor RangeOverlay::colorForType(RangeType type, bool isBlufor) const
{
    // Base color by side
    QColor baseBlue(30, 144, 255);   // DodgerBlue
    QColor baseRed(220, 20, 60);     // Crimson
    
    QColor base = isBlufor ? baseBlue : baseRed;
    
    // Adjust hue by weapon type
    switch (type) {
        case RangeType::MainGun:
            return base;
        case RangeType::ATGM:
            return isBlufor ? QColor(0, 191, 255) : QColor(255, 69, 0);  // DeepSkyBlue / OrangeRed
        case RangeType::SAM:
            return isBlufor ? QColor(138, 43, 226) : QColor(255, 105, 180);  // BlueViolet / HotPink
        case RangeType::MachineGun:
            return base.lighter(150);
        default:
            return base;
    }
}

double RangeOverlay::getMainGunRange(const QString& platformId) const
{
    // Default ranges by platform type (in km)
    // These would ideally come from platform JSON data
    
    QString id = platformId.toLower();
    
    // Main Battle Tanks - ~3-4 km effective range
    if (id.contains("m1a") || id.contains("abrams")) return 4.0;
    if (id.contains("leopard")) return 4.0;
    if (id.contains("t-90") || id.contains("t90")) return 3.5;
    if (id.contains("t-80") || id.contains("t80")) return 3.0;
    if (id.contains("t-72") || id.contains("t72")) return 2.5;
    if (id.contains("challenger")) return 4.0;
    if (id.contains("leclerc")) return 4.0;
    if (id.contains("merkava")) return 4.0;
    if (id.contains("type-10") || id.contains("type-90")) return 3.5;
    if (id.contains("ariete")) return 3.5;
    if (id.contains("k2") || id.contains("k1")) return 3.5;
    
    // IFVs - ~2-3 km
    if (id.contains("bradley")) return 2.5;
    if (id.contains("bmp")) return 2.0;
    if (id.contains("warrior")) return 2.0;
    if (id.contains("puma")) return 3.0;
    if (id.contains("cv90")) return 2.5;
    
    // Artillery - 15-40 km
    if (id.contains("paladin") || id.contains("m109")) return 24.0;
    if (id.contains("pzh2000") || id.contains("pzh 2000")) return 40.0;
    if (id.contains("k9")) return 40.0;
    if (id.contains("msta") || id.contains("2s19")) return 29.0;
    if (id.contains("caesar")) return 42.0;
    if (id.contains("archer")) return 40.0;
    
    // MLRS - 30-300 km
    if (id.contains("himars")) return 80.0;
    if (id.contains("mlrs") || id.contains("m270")) return 45.0;
    if (id.contains("smerch")) return 90.0;
    if (id.contains("grad")) return 40.0;
    
    // Default
    if (id.contains("tank")) return 3.0;
    if (id.contains("artillery") || id.contains("sph")) return 25.0;
    
    return 0.0;  // No range data
}

double RangeOverlay::getAtgmRange(const QString& platformId) const
{
    QString id = platformId.toLower();
    
    // Vehicles with ATGMs
    if (id.contains("bradley")) return 3.75;  // TOW
    if (id.contains("bmp-2")) return 4.0;     // Konkurs
    if (id.contains("bmp-3")) return 5.5;     // Kornet
    if (id.contains("t-90")) return 5.0;      // Refleks
    if (id.contains("merkava")) return 4.0;   // LAHAT
    
    return 0.0;
}

double RangeOverlay::getSamRange(const QString& platformId) const
{
    QString id = platformId.toLower();
    
    // Air defense systems
    if (id.contains("patriot")) return 160.0;
    if (id.contains("s-400") || id.contains("s400")) return 400.0;
    if (id.contains("s-300") || id.contains("s300")) return 200.0;
    if (id.contains("nasams")) return 40.0;
    if (id.contains("buk") || id.contains("sa-11")) return 50.0;
    if (id.contains("tor") || id.contains("sa-15")) return 12.0;
    if (id.contains("pantsir")) return 20.0;
    if (id.contains("tunguska")) return 8.0;
    if (id.contains("avenger")) return 5.0;
    if (id.contains("stinger") || id.contains("manpad")) return 4.8;
    
    return 0.0;
}

void RangeOverlay::setVisibleTypes(RangeType type)
{
    m_visibleType = type;
    update();
}

void RangeOverlay::setShowBlufor(bool show)
{
    m_showBlufor = show;
    update();
}

void RangeOverlay::setShowOpfor(bool show)
{
    m_showOpfor = show;
    update();
}

void RangeOverlay::setRangeOpacity(double opacity)
{
    m_opacity = qBound(0.0, opacity, 1.0);
    update();
}

void RangeOverlay::clear()
{
    m_ranges.clear();
    update();
}

} // namespace athena::ui
