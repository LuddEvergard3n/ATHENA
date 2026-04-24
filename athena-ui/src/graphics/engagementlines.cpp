/**
 * @file engagementlines.cpp
 * @brief Engagement lines implementation
 * 
 * v0.7.2: Initial implementation
 */

#include "engagementlines.hpp"
#include "mapscene.hpp"
#include "unititem.hpp"

#include <QPainter>
#include <QPen>
#include <cmath>

namespace athena::ui {

EngagementLines::EngagementLines(MapScene* scene)
    : QGraphicsItem()
    , m_scene(scene)
{
    setZValue(-5);  // Below units but above terrain
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable, false);
}

void EngagementLines::clear()
{
    m_engagements.clear();
    update();
}

void EngagementLines::addEngagement(const Engagement& engagement)
{
    m_engagements.append(engagement);
    update();
}

void EngagementLines::setEngagements(const QVector<Engagement>& engagements)
{
    m_engagements = engagements;
    update();
}

void EngagementLines::updatePositions()
{
    prepareGeometryChange();
    update();
}

QRectF EngagementLines::boundingRect() const
{
    if (m_scene) {
        return m_scene->sceneRect();
    }
    return QRectF(-10000, -10000, 20000, 20000);
}

void EngagementLines::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!m_scene || m_engagements.isEmpty()) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);

    for (const Engagement& eng : m_engagements) {
        UnitItem* attacker = m_scene->findUnit(eng.attackerId);
        UnitItem* target = m_scene->findUnit(eng.targetId);

        if (!attacker || !target) {
            continue;
        }

        QPointF startPos = attacker->scenePos();
        QPointF endPos = target->scenePos();

        // Calculate line properties based on damage
        const int alpha = static_cast<int>(100 + 155 * eng.damage);
        const double width = 1.0 + 3.0 * eng.damage;
        
        QColor lineColor;
        if (eng.isActive) {
            // Active engagement: bright red/orange
            lineColor = QColor(255, 100, 50, alpha);
        } else {
            // Past engagement: darker red
            lineColor = QColor(180, 50, 50, alpha);
        }

        QPen pen(lineColor, width);
        pen.setCapStyle(Qt::RoundCap);
        painter->setPen(pen);

        // Draw line
        painter->drawLine(startPos, endPos);

        // Draw arrowhead at target
        const double angle = std::atan2(endPos.y() - startPos.y(), 
                                        endPos.x() - startPos.x());
        const double arrowSize = 8.0 + 4.0 * eng.damage;
        
        QPointF arrowP1 = endPos - QPointF(
            std::cos(angle - M_PI / 6) * arrowSize,
            std::sin(angle - M_PI / 6) * arrowSize
        );
        QPointF arrowP2 = endPos - QPointF(
            std::cos(angle + M_PI / 6) * arrowSize,
            std::sin(angle + M_PI / 6) * arrowSize
        );

        QPolygonF arrowHead;
        arrowHead << endPos << arrowP1 << arrowP2;
        
        painter->setBrush(lineColor);
        painter->drawPolygon(arrowHead);

        // Draw damage indicator if significant
        if (eng.damage > 0.3) {
            QPointF midPoint = (startPos + endPos) / 2.0;
            
            painter->setPen(Qt::white);
            QFont font = painter->font();
            font.setPointSize(8);
            font.setBold(true);
            painter->setFont(font);
            
            QString damageText = QString("%1%").arg(static_cast<int>(eng.damage * 100));
            
            // Draw background
            QRectF textRect = painter->fontMetrics().boundingRect(damageText);
            textRect.moveCenter(midPoint);
            textRect.adjust(-3, -2, 3, 2);
            painter->fillRect(textRect, QColor(0, 0, 0, 150));
            
            // Draw text
            painter->drawText(textRect, Qt::AlignCenter, damageText);
        }
    }
}

} // namespace athena::ui
