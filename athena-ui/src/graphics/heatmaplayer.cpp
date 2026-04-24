/**
 * @file heatmaplayer.cpp
 * @brief Heatmap overlay layer implementation
 */

#include "heatmaplayer.hpp"

#include <QPainter>
#include <QLinearGradient>

namespace athena::ui {

HeatmapLayer::HeatmapLayer()
    : m_bounds(-5000, -5000, 10000, 10000)
{
    setZValue(100);  // Above terrain, below units
    setOpacity(0.6);
}

HeatmapLayer::~HeatmapLayer() = default;

QRectF HeatmapLayer::boundingRect() const
{
    return m_bounds;
}

void HeatmapLayer::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    if (m_data.empty()) {
        return;
    }
    
    // Render heatmap from data
    const int rows = static_cast<int>(m_data.size());
    const int cols = rows > 0 ? static_cast<int>(m_data[0].size()) : 0;
    
    if (rows == 0 || cols == 0) {
        return;
    }
    
    const double cellWidth = m_bounds.width() / cols;
    const double cellHeight = m_bounds.height() / rows;
    
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const float value = m_data[y][x];
            if (value > 0.01f) {
                // Color gradient: blue (low) -> red (high)
                const int r = static_cast<int>(255 * value);
                const int b = static_cast<int>(255 * (1.0f - value));
                
                painter->fillRect(
                    QRectF(m_bounds.left() + x * cellWidth,
                           m_bounds.top() + y * cellHeight,
                           cellWidth, cellHeight),
                    QColor(r, 0, b, 150)
                );
            }
        }
    }
}

void HeatmapLayer::setData(const std::vector<std::vector<float>>& data)
{
    m_data = data;
    update();
}

void HeatmapLayer::clear()
{
    m_data.clear();
    update();
}

} // namespace athena::ui
