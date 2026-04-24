/**
 * @file losshistogram.cpp
 * @brief Loss histogram implementation
 * 
 * v0.7.1: Custom QPainter-based histogram
 */

#include "losshistogram.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <cmath>

namespace athena::ui {

LossHistogram::LossHistogram(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(300, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void LossHistogram::setData(const QVector<double>& bluforLosses, const QVector<double>& opforLosses)
{
    m_bluforLosses = bluforLosses;
    m_opforLosses = opforLosses;
    update();
}

void LossHistogram::clear()
{
    m_bluforLosses.clear();
    m_opforLosses.clear();
    update();
}

void LossHistogram::setBinCount(int bins)
{
    m_binCount = qBound(5, bins, 20);
    update();
}

void LossHistogram::setColors(const QColor& blufor, const QColor& opfor)
{
    m_bluforColor = blufor;
    m_opforColor = opfor;
    update();
}

QSize LossHistogram::minimumSizeHint() const
{
    return QSize(300, 200);
}

QSize LossHistogram::sizeHint() const
{
    return QSize(400, 250);
}

LossHistogram::HistogramData LossHistogram::computeBins(const QVector<double>& values) const
{
    HistogramData data;
    data.bins.resize(m_binCount, 0);
    data.minValue = 0;
    data.maxValue = 100;
    data.maxCount = 0;

    if (values.isEmpty()) {
        return data;
    }

    const double binWidth = (data.maxValue - data.minValue) / m_binCount;

    for (double val : values) {
        // Clamp to valid range
        val = qBound(data.minValue, val, data.maxValue);
        
        int binIndex = static_cast<int>((val - data.minValue) / binWidth);
        binIndex = qBound(0, binIndex, m_binCount - 1);
        
        data.bins[binIndex]++;
    }

    // Find max count
    for (int count : data.bins) {
        data.maxCount = qMax(data.maxCount, count);
    }

    return data;
}

void LossHistogram::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor(250, 250, 250));

    // Calculate plot area
    QRect plotArea(MARGIN_LEFT, MARGIN_TOP,
                   width() - MARGIN_LEFT - MARGIN_RIGHT,
                   height() - MARGIN_TOP - MARGIN_BOTTOM);

    if (plotArea.width() < 50 || plotArea.height() < 50) {
        return;
    }

    // Title
    painter.setPen(Qt::black);
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    painter.setFont(titleFont);
    painter.drawText(QRect(0, 5, width(), 25), Qt::AlignCenter, "Loss Distribution");

    // Check if we have data
    if (m_bluforLosses.isEmpty() && m_opforLosses.isEmpty()) {
        painter.setFont(font());
        painter.setPen(QColor(150, 150, 150));
        painter.drawText(plotArea, Qt::AlignCenter, "No data available\nRun a simulation first");
        return;
    }

    // Compute histograms
    HistogramData bluforData = computeBins(m_bluforLosses);
    HistogramData opforData = computeBins(m_opforLosses);

    int maxCount = qMax(bluforData.maxCount, opforData.maxCount);
    if (maxCount == 0) maxCount = 1;

    // Draw axis
    drawAxis(painter, plotArea);

    // Draw bars (OPFOR behind, BLUFOR in front with offset)
    const int barOffset = plotArea.width() / m_binCount / 4;
    
    // Draw OPFOR bars first (will be partially hidden)
    opforData.maxCount = maxCount;
    drawBars(painter, plotArea, opforData, m_opforColor, barOffset);
    
    // Draw BLUFOR bars on top
    bluforData.maxCount = maxCount;
    drawBars(painter, plotArea, bluforData, m_bluforColor, 0);

    // Draw legend
    drawLegend(painter, QRect(width() - 120, MARGIN_TOP, 100, 50));
}

void LossHistogram::drawAxis(QPainter& painter, const QRect& plotArea)
{
    painter.setPen(QPen(Qt::darkGray, 1));
    painter.setFont(font());

    // Y axis
    painter.drawLine(plotArea.left(), plotArea.top(),
                     plotArea.left(), plotArea.bottom());

    // X axis
    painter.drawLine(plotArea.left(), plotArea.bottom(),
                     plotArea.right(), plotArea.bottom());

    // X axis labels (0%, 25%, 50%, 75%, 100%)
    QFontMetrics fm(font());
    const QStringList xLabels = {"0%", "25%", "50%", "75%", "100%"};
    for (int i = 0; i < 5; ++i) {
        int x = plotArea.left() + (plotArea.width() * i) / 4;
        painter.drawLine(x, plotArea.bottom(), x, plotArea.bottom() + 5);
        
        QRect labelRect(x - 20, plotArea.bottom() + 8, 40, 20);
        painter.drawText(labelRect, Qt::AlignCenter, xLabels[i]);
    }

    // X axis title
    painter.drawText(QRect(plotArea.left(), plotArea.bottom() + 22,
                           plotArea.width(), 20),
                     Qt::AlignCenter, "Losses (%)");

    // Y axis labels (based on max count)
    int maxCount = 1;
    if (!m_bluforLosses.isEmpty() || !m_opforLosses.isEmpty()) {
        HistogramData bluforData = computeBins(m_bluforLosses);
        HistogramData opforData = computeBins(m_opforLosses);
        maxCount = qMax(1, qMax(bluforData.maxCount, opforData.maxCount));
    }

    for (int i = 0; i <= 4; ++i) {
        int y = plotArea.bottom() - (plotArea.height() * i) / 4;
        painter.drawLine(plotArea.left() - 5, y, plotArea.left(), y);
        
        int labelValue = (maxCount * i) / 4;
        QRect labelRect(0, y - 10, MARGIN_LEFT - 8, 20);
        painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(labelValue));
    }

    // Y axis title (rotated)
    painter.save();
    painter.translate(12, plotArea.center().y());
    painter.rotate(-90);
    painter.drawText(QRect(-50, 0, 100, 20), Qt::AlignCenter, "Count");
    painter.restore();
}

void LossHistogram::drawBars(QPainter& painter, const QRect& plotArea,
                              const HistogramData& data, const QColor& color, int offset)
{
    if (data.maxCount == 0) return;

    const double binWidth = static_cast<double>(plotArea.width()) / m_binCount;
    const double barWidth = binWidth * 0.7;

    painter.setPen(QPen(color.darker(120), 1));
    painter.setBrush(color);

    for (int i = 0; i < m_binCount; ++i) {
        if (data.bins[i] == 0) continue;

        const double barHeight = (static_cast<double>(data.bins[i]) / data.maxCount) * plotArea.height();
        
        QRectF barRect(
            plotArea.left() + i * binWidth + (binWidth - barWidth) / 2 + offset,
            plotArea.bottom() - barHeight,
            barWidth - offset,
            barHeight
        );

        painter.drawRect(barRect);
    }
}

void LossHistogram::drawLegend(QPainter& painter, const QRect& area)
{
    painter.setFont(font());

    // BLUFOR
    painter.fillRect(area.left(), area.top(), 15, 15, m_bluforColor);
    painter.setPen(m_bluforColor.darker(120));
    painter.drawRect(area.left(), area.top(), 15, 15);
    painter.setPen(Qt::black);
    painter.drawText(area.left() + 20, area.top() + 12, "BLUFOR");

    // OPFOR
    painter.fillRect(area.left(), area.top() + 22, 15, 15, m_opforColor);
    painter.setPen(m_opforColor.darker(120));
    painter.drawRect(area.left(), area.top() + 22, 15, 15);
    painter.setPen(Qt::black);
    painter.drawText(area.left() + 20, area.top() + 34, "OPFOR");
}

} // namespace athena::ui
