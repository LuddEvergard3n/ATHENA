/**
 * @file engagementlines.hpp
 * @brief Visual representation of unit engagements on the map
 * 
 * v0.7.2: Initial implementation
 */

#ifndef ATHENA_UI_ENGAGEMENTLINES_HPP
#define ATHENA_UI_ENGAGEMENTLINES_HPP

#include <QGraphicsItem>
#include <QVector>
#include <QPair>

namespace athena::ui {

class MapScene;

/**
 * @brief Represents a single engagement between two units
 */
struct Engagement {
    QString attackerId;
    QString targetId;
    double damage = 0.0;      // 0.0-1.0 representing damage dealt
    bool isActive = false;    // Currently firing
};

/**
 * @brief Graphics item that draws engagement lines between units
 * 
 * Shows red lines from attacking units to their targets.
 * Line thickness/opacity based on damage dealt.
 */
class EngagementLines : public QGraphicsItem
{
public:
    explicit EngagementLines(MapScene* scene);
    ~EngagementLines() override = default;

    /**
     * @brief Clear all engagements
     */
    void clear();

    /**
     * @brief Add an engagement to visualize
     */
    void addEngagement(const Engagement& engagement);

    /**
     * @brief Set all engagements at once
     */
    void setEngagements(const QVector<Engagement>& engagements);

    /**
     * @brief Update line positions (call when units move)
     */
    void updatePositions();

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

private:
    MapScene* m_scene = nullptr;
    QVector<Engagement> m_engagements;
};

} // namespace athena::ui

#endif // ATHENA_UI_ENGAGEMENTLINES_HPP
