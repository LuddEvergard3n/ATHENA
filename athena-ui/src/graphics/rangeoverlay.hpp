/**
 * @file rangeoverlay.hpp
 * @brief Weapon range visualization overlay
 * 
 * v0.6.12: Initial implementation
 */

#ifndef ATHENA_UI_RANGEOVERLAY_HPP
#define ATHENA_UI_RANGEOVERLAY_HPP

#include <QGraphicsItem>
#include <QVector>
#include <QColor>

namespace athena::ui {

class UnitItem;
class MapScene;

/**
 * @brief Types of range circles to display
 */
enum class RangeType {
    MainGun,        // Tank gun, artillery
    ATGM,           // Anti-tank guided missiles
    SAM,            // Surface-to-air missiles
    MachineGun,     // Secondary weapons
    All             // Show all ranges
};

/**
 * @brief Single range circle data
 */
struct RangeCircle {
    QString unitId;
    double centerLat;
    double centerLon;
    double rangeKm;
    RangeType type;
    QColor color;
    bool isBlufor;
};

/**
 * @brief Graphics item displaying weapon range circles
 * 
 * Draws semi-transparent circles around units showing their
 * weapon effective ranges. Different colors for different
 * weapon types and sides.
 */
class RangeOverlay : public QGraphicsItem
{
public:
    explicit RangeOverlay(MapScene* scene);
    ~RangeOverlay() override = default;
    
    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    /**
     * @brief Update ranges from current units
     */
    void updateRanges();
    
    /**
     * @brief Set which range types to display
     */
    void setVisibleTypes(RangeType type);
    RangeType visibleTypes() const { return m_visibleType; }
    
    /**
     * @brief Set whether to show BLUFOR ranges
     */
    void setShowBlufor(bool show);
    bool showBlufor() const { return m_showBlufor; }
    
    /**
     * @brief Set whether to show OPFOR ranges
     */
    void setShowOpfor(bool show);
    bool showOpfor() const { return m_showOpfor; }
    
    /**
     * @brief Set opacity for range circles
     */
    void setRangeOpacity(double opacity);
    double rangeOpacity() const { return m_opacity; }
    
    /**
     * @brief Clear all ranges
     */
    void clear();

private:
    void addRangeForUnit(UnitItem* unit);
    QColor colorForType(RangeType type, bool isBlufor) const;
    double getMainGunRange(const QString& platformId) const;
    double getAtgmRange(const QString& platformId) const;
    double getSamRange(const QString& platformId) const;
    
    MapScene* m_scene;
    QVector<RangeCircle> m_ranges;
    
    RangeType m_visibleType = RangeType::All;
    bool m_showBlufor = true;
    bool m_showOpfor = true;
    double m_opacity = 0.3;
    
    // Scene bounds for coordinate conversion
    double m_sceneSize = 2000.0;
};

} // namespace athena::ui

#endif // ATHENA_UI_RANGEOVERLAY_HPP
