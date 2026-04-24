/**
 * @file losshistogram.hpp
 * @brief Histogram widget for loss distribution visualization
 * 
 * v0.7.1: Initial implementation - custom painted histogram
 */

#ifndef ATHENA_UI_LOSSHISTOGRAM_HPP
#define ATHENA_UI_LOSSHISTOGRAM_HPP

#include <QWidget>
#include <QVector>

namespace athena::ui {

/**
 * @brief Custom widget that draws a histogram of loss distribution
 * 
 * Displays BLUFOR and OPFOR loss percentages as overlapping histograms.
 * Uses QPainter for rendering without Qt Charts dependency.
 */
class LossHistogram : public QWidget
{
    Q_OBJECT

public:
    explicit LossHistogram(QWidget* parent = nullptr);
    ~LossHistogram() override = default;

    /**
     * @brief Set data for the histogram
     * @param bluforLosses Vector of BLUFOR loss percentages (0-100)
     * @param opforLosses Vector of OPFOR loss percentages (0-100)
     */
    void setData(const QVector<double>& bluforLosses, const QVector<double>& opforLosses);

    /**
     * @brief Clear all data
     */
    void clear();

    /**
     * @brief Set number of bins (default: 10)
     */
    void setBinCount(int bins);

    /**
     * @brief Set colors for BLUFOR and OPFOR bars
     */
    void setColors(const QColor& blufor, const QColor& opfor);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct HistogramData {
        QVector<int> bins;      // Count per bin
        int maxCount = 0;       // Max count in any bin
        double minValue = 0;
        double maxValue = 100;
    };

    HistogramData computeBins(const QVector<double>& values) const;
    void drawAxis(QPainter& painter, const QRect& plotArea);
    void drawBars(QPainter& painter, const QRect& plotArea, 
                  const HistogramData& data, const QColor& color, int offset);
    void drawLegend(QPainter& painter, const QRect& area);

    QVector<double> m_bluforLosses;
    QVector<double> m_opforLosses;
    
    int m_binCount = 10;
    QColor m_bluforColor{70, 130, 180, 180};   // Steel blue with alpha
    QColor m_opforColor{205, 92, 92, 180};     // Indian red with alpha
    
    // Margins
    static constexpr int MARGIN_LEFT = 50;
    static constexpr int MARGIN_RIGHT = 20;
    static constexpr int MARGIN_TOP = 30;
    static constexpr int MARGIN_BOTTOM = 40;
};

} // namespace athena::ui

#endif // ATHENA_UI_LOSSHISTOGRAM_HPP
