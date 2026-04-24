/**
 * @file unititem.hpp
 * @brief Unit graphics item with NATO symbol rendering
 * 
 * v0.6.8: Added drag support and quantity display
 * v0.6.10: Added context menu (edit, delete, duplicate, change side)
 */

#ifndef ATHENA_UI_UNITITEM_HPP
#define ATHENA_UI_UNITITEM_HPP

#include <QGraphicsItem>
#include <QString>

class QMenu;

namespace athena::ui {

/**
 * @brief Graphics item representing a military unit on the map
 * 
 * Features:
 * - Simplified NATO APP-6 symbology
 * - Side coloring (Blue/Red)
 * - Draggable with position updates
 * - Quantity badge
 * - Selection highlight
 * - Context menu (right-click)
 */
class UnitItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum class Side { Blue, Red, Neutral };
    enum class UnitType { Infantry, Armor, Artillery, AirDefense, Aviation, Naval, Unknown };

    UnitItem(const QString& id, Side side, UnitType type, QGraphicsItem* parent = nullptr);
    ~UnitItem() override;

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    // Accessors
    QString unitId() const { return m_id; }
    QString platformId() const { return m_platformId; }
    QString displayName() const { return m_displayName; }
    Side side() const { return m_side; }
    UnitType type() const { return m_type; }
    int quantity() const { return m_quantity; }

    // Mutators
    void setPlatformId(const QString& id) { m_platformId = id; }
    void setDisplayName(const QString& name);
    void setQuantity(int qty);
    void setSide(Side side);
    void setSelected(bool selected);
    
    // Position in geo coordinates
    void setGeoPosition(double lat, double lon);
    double geoLat() const { return m_geoLat; }
    double geoLon() const { return m_geoLon; }

    // Type detection from platform ID
    static UnitType detectType(const QString& platformId);

signals:
    void positionChanged(const QString& unitId, double lat, double lon);
    void selected(const QString& unitId);
    
    // Context menu actions
    void deleteRequested(const QString& unitId);
    void duplicateRequested(const QString& unitId);
    void changeSideRequested(const QString& unitId);
    void editQuantityRequested(const QString& unitId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    void drawNatoSymbol(QPainter* painter, const QRectF& rect);
    void showContextMenu(const QPoint& screenPos);
    void updateTooltip();

    QString m_id;
    QString m_platformId;
    QString m_displayName;
    Side m_side;
    UnitType m_type;
    int m_quantity = 1;
    bool m_selected = false;
    
    // Geo coordinates
    double m_geoLat = 0.0;
    double m_geoLon = 0.0;
    
    // Dragging state
    bool m_dragging = false;
    QPointF m_dragStartPos;
    
    static constexpr double SIZE = 40.0;
    static constexpr double BADGE_SIZE = 14.0;
};

} // namespace athena::ui

#endif // ATHENA_UI_UNITITEM_HPP
