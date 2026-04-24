/**
 * @file unititem.cpp
 * @brief Unit graphics item implementation
 * 
 * v0.6.10: Added context menu support
 * v0.6.11: Added tooltips
 */

#include "unititem.hpp"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QStyleOptionGraphicsItem>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QCursor>
#include <QDebug>

namespace athena::ui {

namespace {
    QString typeToString(UnitItem::UnitType type) {
        switch (type) {
            case UnitItem::UnitType::Infantry: return "Infantry";
            case UnitItem::UnitType::Armor: return "Armor";
            case UnitItem::UnitType::Artillery: return "Artillery";
            case UnitItem::UnitType::AirDefense: return "Air Defense";
            case UnitItem::UnitType::Aviation: return "Aviation";
            case UnitItem::UnitType::Naval: return "Naval";
            default: return "Unknown";
        }
    }
    
    QString sideToString(UnitItem::Side side) {
        switch (side) {
            case UnitItem::Side::Blue: return "BLUFOR";
            case UnitItem::Side::Red: return "OPFOR";
            default: return "Neutral";
        }
    }
}

UnitItem::UnitItem(const QString& id, Side side, UnitType type, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_id(id)
    , m_side(side)
    , m_type(type)
{
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setFlag(ItemSendsGeometryChanges);
    setCursor(Qt::OpenHandCursor);
    setAcceptHoverEvents(true);
    
    updateTooltip();
}

UnitItem::~UnitItem() = default;

QRectF UnitItem::boundingRect() const
{
    const double margin = 5.0;
    return QRectF(-SIZE/2 - margin, -SIZE/2 - margin, 
                  SIZE + 2*margin, SIZE + 2*margin + 15);  // +15 for label
}

void UnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    const QRectF rect(-SIZE/2, -SIZE/2, SIZE, SIZE);
    
    // Selection highlight
    if (m_selected || (option && (option->state & QStyle::State_Selected))) {
        painter->setPen(QPen(Qt::yellow, 3));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect.adjusted(-4, -4, 4, 4));
    }
    
    // Draw NATO symbol
    drawNatoSymbol(painter, rect);
    
    // Quantity badge (top-right)
    if (m_quantity > 1) {
        const QRectF badgeRect(SIZE/2 - BADGE_SIZE/2, -SIZE/2 - BADGE_SIZE/2, 
                               BADGE_SIZE, BADGE_SIZE);
        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::white);
        painter->drawEllipse(badgeRect);
        painter->setPen(Qt::black);
        painter->setFont(QFont("Arial", 8, QFont::Bold));
        painter->drawText(badgeRect, Qt::AlignCenter, QString::number(m_quantity));
    }
    
    // Label (below symbol)
    if (!m_displayName.isEmpty()) {
        painter->setPen(Qt::black);
        painter->setFont(QFont("Arial", 7));
        const QRectF labelRect(-SIZE, SIZE/2 + 2, SIZE*2, 12);
        
        // Truncate long names
        QString label = m_displayName;
        if (label.length() > 12) {
            label = label.left(10) + "...";
        }
        painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, label);
    }
}

void UnitItem::drawNatoSymbol(QPainter* painter, const QRectF& rect)
{
    // NATO APP-6 simplified symbology
    // Blue = rectangle, Red = diamond
    
    QColor fillColor, borderColor;
    if (m_side == Side::Blue) {
        fillColor = QColor(100, 150, 255, 200);  // Light blue
        borderColor = QColor(0, 50, 150);        // Dark blue
    } else if (m_side == Side::Red) {
        fillColor = QColor(255, 100, 100, 200);  // Light red
        borderColor = QColor(150, 0, 0);         // Dark red
    } else {
        fillColor = QColor(200, 200, 100, 200);  // Yellow/neutral
        borderColor = QColor(100, 100, 0);
    }
    
    painter->setPen(QPen(borderColor, 2));
    painter->setBrush(fillColor);
    
    // Shape based on affiliation
    if (m_side == Side::Red) {
        // Diamond for hostile
        QPolygonF diamond;
        diamond << QPointF(rect.center().x(), rect.top())
                << QPointF(rect.right(), rect.center().y())
                << QPointF(rect.center().x(), rect.bottom())
                << QPointF(rect.left(), rect.center().y());
        painter->drawPolygon(diamond);
    } else {
        // Rectangle for friendly
        painter->drawRect(rect);
    }
    
    // Unit type symbol inside
    painter->setPen(QPen(borderColor, 2));
    painter->setBrush(Qt::NoBrush);
    
    const double cx = rect.center().x();
    const double cy = rect.center().y();
    const double s = SIZE * 0.3;  // Symbol size
    
    switch (m_type) {
        case UnitType::Armor:
            // Oval for armor
            painter->drawEllipse(QRectF(cx - s, cy - s/2, s*2, s));
            break;
            
        case UnitType::Infantry:
            // X for infantry
            painter->drawLine(QPointF(cx - s, cy - s), QPointF(cx + s, cy + s));
            painter->drawLine(QPointF(cx - s, cy + s), QPointF(cx + s, cy - s));
            break;
            
        case UnitType::Artillery:
            // Circle for artillery
            painter->drawEllipse(QPointF(cx, cy), s * 0.7, s * 0.7);
            break;
            
        case UnitType::AirDefense:
            // Arc for air defense
            painter->drawArc(QRectF(cx - s, cy - s/2, s*2, s*1.5), 0, 180*16);
            break;
            
        case UnitType::Aviation:
            // Propeller for aviation
            painter->drawLine(QPointF(cx, cy - s), QPointF(cx, cy + s));
            painter->drawLine(QPointF(cx - s*0.7, cy - s*0.5), QPointF(cx + s*0.7, cy + s*0.5));
            break;
            
        case UnitType::Naval:
            // Anchor-like for naval
            painter->drawLine(QPointF(cx, cy - s), QPointF(cx, cy + s*0.5));
            painter->drawArc(QRectF(cx - s*0.5, cy, s, s*0.8), 0, 180*16);
            break;
            
        default:
            // Question mark for unknown
            painter->setFont(QFont("Arial", 14, QFont::Bold));
            painter->drawText(rect, Qt::AlignCenter, "?");
            break;
    }
}

void UnitItem::setDisplayName(const QString& name)
{
    m_displayName = name;
    updateTooltip();
    update();
}

void UnitItem::setQuantity(int qty)
{
    m_quantity = qty;
    updateTooltip();
    update();
}

void UnitItem::setSide(Side side)
{
    m_side = side;
    updateTooltip();
    update();
}

void UnitItem::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void UnitItem::setGeoPosition(double lat, double lon)
{
    m_geoLat = lat;
    m_geoLon = lon;
    updateTooltip();
}

UnitItem::UnitType UnitItem::detectType(const QString& platformId)
{
    const QString id = platformId.toLower();
    
    // Armor
    if (id.contains("tank") || id.contains("m1a") || id.contains("leopard") || 
        id.contains("t-72") || id.contains("t-80") || id.contains("t-90") ||
        id.contains("challenger") || id.contains("leclerc") || id.contains("ariete") ||
        id.contains("merkava") || id.contains("abrams") || id.contains("type-")) {
        return UnitType::Armor;
    }
    
    // IFV/APC (also armor-ish)
    if (id.contains("ifv") || id.contains("apc") || id.contains("bradley") || 
        id.contains("bmp") || id.contains("btr") || id.contains("stryker") ||
        id.contains("warrior") || id.contains("puma") || id.contains("cv90")) {
        return UnitType::Armor;
    }
    
    // Artillery
    if (id.contains("artillery") || id.contains("howitzer") || id.contains("mortar") ||
        id.contains("mlrs") || id.contains("rocket") || id.contains("paladin") ||
        id.contains("pzh") || id.contains("m777") || id.contains("caesar") ||
        id.contains("msta") || id.contains("2s") || id.contains("himars") ||
        id.contains("astros")) {
        return UnitType::Artillery;
    }
    
    // Air Defense
    if (id.contains("sam") || id.contains("manpad") || id.contains("patriot") ||
        id.contains("s-300") || id.contains("s-400") || id.contains("pantsir") ||
        id.contains("tor") || id.contains("buk") || id.contains("nasams") ||
        id.contains("gepard") || id.contains("shorad") || id.contains("stinger") ||
        id.contains("igla") || id.contains("air-defense")) {
        return UnitType::AirDefense;
    }
    
    // Aviation
    if (id.contains("helicopter") || id.contains("heli") || id.contains("apache") ||
        id.contains("mi-") || id.contains("ka-") || id.contains("ah-") ||
        id.contains("uh-") || id.contains("ch-") || id.contains("tiger") ||
        id.contains("fighter") || id.contains("aircraft") || id.contains("f-") ||
        id.contains("su-") || id.contains("mig-") || id.contains("uav") ||
        id.contains("drone") || id.contains("gripen") || id.contains("rafale") ||
        id.contains("typhoon") || id.contains("bomber")) {
        return UnitType::Aviation;
    }
    
    // Naval
    if (id.contains("ship") || id.contains("frigate") || id.contains("destroyer") ||
        id.contains("carrier") || id.contains("submarine") || id.contains("corvette") ||
        id.contains("cruiser") || id.contains("amphibious")) {
        return UnitType::Naval;
    }
    
    // Infantry as fallback for ground units
    if (id.contains("infantry") || id.contains("rifle") || id.contains("atgm") ||
        id.contains("javelin") || id.contains("tow") || id.contains("kornet")) {
        return UnitType::Infantry;
    }
    
    return UnitType::Unknown;
}

void UnitItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartPos = pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QGraphicsObject::mousePressEvent(event);
}

void UnitItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsObject::mouseMoveEvent(event);
}

void UnitItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        
        // Emit position change if moved significantly
        if ((pos() - m_dragStartPos).manhattanLength() > 5) {
            emit positionChanged(m_id, m_geoLat, m_geoLon);
        } else {
            emit selected(m_id);
        }
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void UnitItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    showContextMenu(event->screenPos());
    event->accept();
}

void UnitItem::showContextMenu(const QPoint& screenPos)
{
    QMenu menu;
    
    // Header with unit info
    QAction* headerAction = menu.addAction(QString("%1 x%2").arg(m_displayName).arg(m_quantity));
    headerAction->setEnabled(false);
    QFont boldFont = headerAction->font();
    boldFont.setBold(true);
    headerAction->setFont(boldFont);
    
    menu.addSeparator();
    
    // Edit quantity
    QAction* editQtyAction = menu.addAction("Edit Quantity...");
    editQtyAction->setIcon(QIcon::fromTheme("document-edit"));
    
    // Change side
    QString sideText = (m_side == Side::Blue) ? "Move to OPFOR" : "Move to BLUFOR";
    QAction* changeSideAction = menu.addAction(sideText);
    changeSideAction->setIcon(QIcon::fromTheme("object-flip-horizontal"));
    
    // Duplicate
    QAction* duplicateAction = menu.addAction("Duplicate");
    duplicateAction->setIcon(QIcon::fromTheme("edit-copy"));
    duplicateAction->setShortcut(QKeySequence("Ctrl+D"));
    
    menu.addSeparator();
    
    // Delete
    QAction* deleteAction = menu.addAction("Delete");
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    
    // Show menu and handle result
    QAction* selectedAction = menu.exec(screenPos);
    
    if (selectedAction == editQtyAction) {
        emit editQuantityRequested(m_id);
    } else if (selectedAction == changeSideAction) {
        emit changeSideRequested(m_id);
    } else if (selectedAction == duplicateAction) {
        emit duplicateRequested(m_id);
    } else if (selectedAction == deleteAction) {
        emit deleteRequested(m_id);
    }
}

QVariant UnitItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged) {
        // Update geo position based on scene position
        // This will be converted by MapScene
    }
    return QGraphicsObject::itemChange(change, value);
}

void UnitItem::updateTooltip()
{
    QString tooltip = QString("<b>%1</b><br>")
        .arg(m_displayName.isEmpty() ? m_platformId : m_displayName);
    
    tooltip += QString("Type: %1<br>").arg(typeToString(m_type));
    tooltip += QString("Side: %1<br>").arg(sideToString(m_side));
    tooltip += QString("Quantity: %1<br>").arg(m_quantity);
    tooltip += QString("Position: %1°, %2°")
        .arg(m_geoLat, 0, 'f', 4)
        .arg(m_geoLon, 0, 'f', 4);
    
    setToolTip(tooltip);
}

} // namespace athena::ui
