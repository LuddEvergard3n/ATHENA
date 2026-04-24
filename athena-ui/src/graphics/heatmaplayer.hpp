/**
 * @file heatmaplayer.hpp
 * @brief Heatmap overlay layer for aggregate visualization
 * 
 * NOTE: This is currently a stub for future implementation.
 * Will be used to visualize:
 * - Engagement density from multiple simulation runs
 * - Casualty locations
 * - Movement patterns
 * 
 * TODO v0.8+: Implement data collection from SimulationRunner results
 */

#ifndef ATHENA_UI_HEATMAPLAYER_HPP
#define ATHENA_UI_HEATMAPLAYER_HPP

#include <QGraphicsItem>
#include <vector>

namespace athena::ui {

/**
 * @brief Graphics item for rendering heatmap overlays (STUB)
 */
class HeatmapLayer : public QGraphicsItem
{
public:
    HeatmapLayer();
    ~HeatmapLayer() override;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void setData(const std::vector<std::vector<float>>& data);
    void clear();

private:
    std::vector<std::vector<float>> m_data;
    QRectF m_bounds;
};

} // namespace athena::ui

#endif // ATHENA_UI_HEATMAPLAYER_HPP
