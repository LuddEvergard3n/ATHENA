/**
 * @file minimap.cpp
 * @brief Mini-map implementation
 * 
 * v0.7.1: Initial implementation
 */

#include "minimap.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsItem>

namespace athena::ui {

MiniMap::MiniMap(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(120, 100);
    setMaximumSize(250, 200);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setCursor(Qt::CrossCursor);
    
    // Style
    setStyleSheet("background-color: #2a2a2a; border: 1px solid #555;");
}

void MiniMap::setScene(QGraphicsScene* scene)
{
    if (m_scene) {
        disconnect(m_scene, nullptr, this, nullptr);
    }
    
    m_scene = scene;
    
    if (m_scene) {
        connect(m_scene, &QGraphicsScene::changed, this, [this]() {
            m_backgroundDirty = true;
            update();
        });
        connect(m_scene, &QGraphicsScene::sceneRectChanged, this, &MiniMap::updateSceneRect);
        updateSceneRect();
    }
    
    m_backgroundDirty = true;
    update();
}

void MiniMap::setViewportRect(const QRectF& viewportRect)
{
    m_viewportRect = viewportRect;
    update();
}

void MiniMap::setTerrainVisible(bool visible)
{
    m_showTerrain = visible;
    m_backgroundDirty = true;
    update();
}

void MiniMap::refresh()
{
    update();
}

QSize MiniMap::minimumSizeHint() const
{
    return QSize(120, 100);
}

QSize MiniMap::sizeHint() const
{
    return QSize(180, 150);
}

void MiniMap::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor(42, 42, 42));

    if (!m_scene || m_sceneRect.isEmpty()) {
        painter.setPen(QColor(100, 100, 100));
        painter.drawText(rect(), Qt::AlignCenter, "No map");
        return;
    }

    // Draw cached background or regenerate
    if (m_backgroundDirty || m_cachedBackground.size() != size()) {
        m_cachedBackground = QImage(size(), QImage::Format_RGB32);
        m_cachedBackground.fill(QColor(42, 42, 42));
        
        QPainter bgPainter(&m_cachedBackground);
        bgPainter.setRenderHint(QPainter::Antialiasing);
        
        // Draw terrain background (simplified)
        if (m_showTerrain) {
            // Simple green background representing terrain
            QRectF mapArea = sceneRectToWidget(m_sceneRect);
            bgPainter.fillRect(mapArea, QColor(34, 45, 34));
            
            // Grid lines
            bgPainter.setPen(QPen(QColor(50, 60, 50), 0.5));
            const int gridLines = 5;
            for (int i = 1; i < gridLines; ++i) {
                double x = mapArea.left() + mapArea.width() * i / gridLines;
                double y = mapArea.top() + mapArea.height() * i / gridLines;
                bgPainter.drawLine(QPointF(x, mapArea.top()), QPointF(x, mapArea.bottom()));
                bgPainter.drawLine(QPointF(mapArea.left(), y), QPointF(mapArea.right(), y));
            }
        }
        
        m_backgroundDirty = false;
    }
    
    painter.drawImage(0, 0, m_cachedBackground);

    // Draw units from scene
    if (m_scene) {
        const auto items = m_scene->items();
        for (QGraphicsItem* item : items) {
            // Check if it's a unit (has "UnitItem" type or specific data)
            if (item->type() == QGraphicsItem::UserType + 1) {  // UnitItem type
                QPointF scenePos = item->scenePos();
                QPointF widgetPos = sceneToWidget(scenePos);
                
                // Get color from item data (or detect side from position/color)
                QVariant sideData = item->data(0);  // Assuming side stored in data(0)
                bool isBlufor = sideData.isValid() ? sideData.toBool() : (scenePos.y() < m_sceneRect.center().y());
                bool isSelected = item->isSelected();
                
                // Draw selection ring first (behind unit)
                if (isSelected) {
                    painter.setPen(QPen(QColor(255, 255, 0), 2));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(widgetPos, 5, 5);
                }
                
                // Draw unit marker
                painter.setPen(Qt::NoPen);
                painter.setBrush(isBlufor ? QColor(70, 130, 180) : QColor(180, 70, 70));
                painter.drawEllipse(widgetPos, 3, 3);
            }
        }
    }

    // Draw viewport rectangle
    if (!m_viewportRect.isEmpty()) {
        QRectF vpWidget = sceneRectToWidget(m_viewportRect);
        
        // Clamp to widget bounds
        vpWidget = vpWidget.intersected(QRectF(rect()));
        
        if (!vpWidget.isEmpty()) {
            // Semi-transparent fill
            painter.fillRect(vpWidget, QColor(255, 255, 255, 30));
            
            // Border
            painter.setPen(QPen(QColor(255, 200, 100), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(vpWidget);
        }
    }

    // Border
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void MiniMap::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        m_dragging = true;
        QPointF scenePos = widgetToScene(event->pos());
        emit navigationRequested(scenePos);
    }
}

void MiniMap::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && m_scene) {
        QPointF scenePos = widgetToScene(event->pos());
        emit navigationRequested(scenePos);
    }
}

void MiniMap::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    m_backgroundDirty = true;
}

QPointF MiniMap::widgetToScene(const QPoint& widgetPos) const
{
    if (m_sceneRect.isEmpty() || width() == 0 || height() == 0) {
        return QPointF();
    }

    // Calculate scale maintaining aspect ratio
    const double scaleX = static_cast<double>(width()) / m_sceneRect.width();
    const double scaleY = static_cast<double>(height()) / m_sceneRect.height();
    const double scale = qMin(scaleX, scaleY);

    // Calculate offset for centering
    const double mapWidth = m_sceneRect.width() * scale;
    const double mapHeight = m_sceneRect.height() * scale;
    const double offsetX = (width() - mapWidth) / 2.0;
    const double offsetY = (height() - mapHeight) / 2.0;

    // Convert
    double sceneX = m_sceneRect.left() + (widgetPos.x() - offsetX) / scale;
    double sceneY = m_sceneRect.top() + (widgetPos.y() - offsetY) / scale;

    return QPointF(sceneX, sceneY);
}

QPointF MiniMap::sceneToWidget(const QPointF& scenePos) const
{
    if (m_sceneRect.isEmpty() || width() == 0 || height() == 0) {
        return QPointF();
    }

    // Calculate scale maintaining aspect ratio
    const double scaleX = static_cast<double>(width()) / m_sceneRect.width();
    const double scaleY = static_cast<double>(height()) / m_sceneRect.height();
    const double scale = qMin(scaleX, scaleY);

    // Calculate offset for centering
    const double mapWidth = m_sceneRect.width() * scale;
    const double mapHeight = m_sceneRect.height() * scale;
    const double offsetX = (width() - mapWidth) / 2.0;
    const double offsetY = (height() - mapHeight) / 2.0;

    // Convert
    double widgetX = offsetX + (scenePos.x() - m_sceneRect.left()) * scale;
    double widgetY = offsetY + (scenePos.y() - m_sceneRect.top()) * scale;

    return QPointF(widgetX, widgetY);
}

QRectF MiniMap::sceneRectToWidget(const QRectF& sceneRect) const
{
    QPointF topLeft = sceneToWidget(sceneRect.topLeft());
    QPointF bottomRight = sceneToWidget(sceneRect.bottomRight());
    return QRectF(topLeft, bottomRight);
}

void MiniMap::updateSceneRect()
{
    if (m_scene) {
        m_sceneRect = m_scene->sceneRect();
        m_backgroundDirty = true;
        update();
    }
}

} // namespace athena::ui
