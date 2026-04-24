/**
 * @file minimap.hpp
 * @brief Mini-map overview widget for tactical map navigation
 * 
 * v0.7.1: Initial implementation
 */

#ifndef ATHENA_UI_MINIMAP_HPP
#define ATHENA_UI_MINIMAP_HPP

#include <QWidget>
#include <QRectF>

class QGraphicsScene;

namespace athena::ui {

/**
 * @brief Miniature overview of the tactical map
 * 
 * Shows the entire map with a viewport indicator showing current view.
 * Clicking on the minimap navigates to that location.
 */
class MiniMap : public QWidget
{
    Q_OBJECT

public:
    explicit MiniMap(QWidget* parent = nullptr);
    ~MiniMap() override = default;

    /**
     * @brief Set the scene to display
     */
    void setScene(QGraphicsScene* scene);

    /**
     * @brief Update the viewport indicator rectangle
     * @param viewportRect The visible area in scene coordinates
     */
    void setViewportRect(const QRectF& viewportRect);

    /**
     * @brief Set terrain visibility
     */
    void setTerrainVisible(bool visible);
    
    /**
     * @brief Force redraw (e.g., when selection changes)
     */
    void refresh();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    /**
     * @brief Emitted when user clicks on minimap
     * @param scenePos Center position in scene coordinates
     */
    void navigationRequested(const QPointF& scenePos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QPointF widgetToScene(const QPoint& widgetPos) const;
    QPointF sceneToWidget(const QPointF& scenePos) const;
    QRectF sceneRectToWidget(const QRectF& sceneRect) const;
    void updateSceneRect();

    QGraphicsScene* m_scene = nullptr;
    QRectF m_sceneRect;
    QRectF m_viewportRect;
    bool m_showTerrain = true;
    bool m_dragging = false;
    
    // Cached rendering
    QImage m_cachedBackground;
    bool m_backgroundDirty = true;
};

} // namespace athena::ui

#endif // ATHENA_UI_MINIMAP_HPP
