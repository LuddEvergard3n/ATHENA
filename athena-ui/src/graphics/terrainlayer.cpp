/**
 * @file terrainlayer.cpp
 * @brief Terrain rendering layer implementation
 * 
 * v0.6.9: Full terrain visualization with procedural generation
 */

#include "terrainlayer.hpp"

#include <QPainter>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <cmath>
#include <random>

namespace athena::ui {

TerrainLayer::TerrainLayer(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
    setZValue(-100);  // Behind everything else
    m_sceneBounds = QRectF(-1000, -1000, 2000, 2000);
}

TerrainLayer::~TerrainLayer() = default;

QRectF TerrainLayer::boundingRect() const
{
    return m_sceneBounds;
}

void TerrainLayer::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    if (!m_imageValid || m_terrainImage.isNull()) {
        // Draw placeholder background
        painter->fillRect(m_sceneBounds, QColor(200, 210, 180));
        return;
    }
    
    // Draw terrain image scaled to scene bounds
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(m_sceneBounds, m_terrainImage);
}

bool TerrainLayer::loadTerrain(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "TerrainLayer: Cannot open" << filePath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning() << "TerrainLayer: Invalid JSON in" << filePath;
        return false;
    }
    
    QJsonObject root = doc.object();
    
    m_gridWidth = root["width"].toInt(100);
    m_gridHeight = root["height"].toInt(100);
    
    // Bounds
    QJsonObject bounds = root["bounds"].toObject();
    m_minLat = bounds["min_lat"].toDouble(49.0);
    m_maxLat = bounds["max_lat"].toDouble(51.0);
    m_minLon = bounds["min_lon"].toDouble(9.0);
    m_maxLon = bounds["max_lon"].toDouble(11.0);
    
    // Cells
    m_cells.resize(m_gridWidth * m_gridHeight);
    
    QJsonArray cells = root["cells"].toArray();
    for (int i = 0; i < cells.size() && i < static_cast<int>(m_cells.size()); ++i) {
        QJsonObject cell = cells[i].toObject();
        m_cells[i].type = static_cast<TerrainType>(cell["type"].toInt(0));
        m_cells[i].elevation = static_cast<float>(cell["elevation"].toDouble(0.0));
        m_cells[i].movementCost = static_cast<float>(cell["movement_cost"].toDouble(1.0));
        m_cells[i].coverValue = static_cast<float>(cell["cover"].toDouble(0.0));
    }
    
    updateImage();
    
    qDebug() << "TerrainLayer: Loaded" << m_gridWidth << "x" << m_gridHeight << "terrain";
    return true;
}

void TerrainLayer::generateTerrain(int width, int height, uint32_t seed)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    
    m_gridWidth = width;
    m_gridHeight = height;
    m_cells.resize(width * height);
    
    // Use seed or random
    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }
    
    std::mt19937 rng(seed);
    
    // Generate elevation noise
    std::vector<float> elevationNoise(width * height);
    generatePerlinNoise(elevationNoise, width, height, seed);
    
    // Generate secondary noise for terrain type variation
    std::vector<float> typeNoise(width * height);
    generatePerlinNoise(typeNoise, width, height, seed + 12345);
    
    // Assign terrain types based on noise
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = y * width + x;
            TerrainCell& cell = m_cells[idx];
            
            const float elev = elevationNoise[idx];
            const float type = typeNoise[idx];
            
            // Elevation in meters (0-500m range)
            cell.elevation = elev * 500.0f;
            
            // Assign terrain type based on elevation and noise
            if (elev < 0.15f) {
                // Low areas = water or marsh
                cell.type = (type < 0.5f) ? TerrainType::Water : TerrainType::Marsh;
                cell.movementCost = (cell.type == TerrainType::Water) ? 999.0f : 2.5f;
                cell.coverValue = (cell.type == TerrainType::Water) ? 0.0f : 0.3f;
            } else if (elev < 0.35f) {
                // Low-mid = open or forest
                if (type < 0.4f) {
                    cell.type = TerrainType::Forest;
                    cell.movementCost = 1.8f;
                    cell.coverValue = 0.7f;
                } else if (type < 0.55f) {
                    cell.type = TerrainType::Urban;
                    cell.movementCost = 1.2f;
                    cell.coverValue = 0.8f;
                } else {
                    cell.type = TerrainType::Open;
                    cell.movementCost = 1.0f;
                    cell.coverValue = 0.1f;
                }
            } else if (elev < 0.65f) {
                // Mid elevation = mostly open, some forest
                if (type < 0.3f) {
                    cell.type = TerrainType::Forest;
                    cell.movementCost = 1.8f;
                    cell.coverValue = 0.7f;
                } else {
                    cell.type = TerrainType::Open;
                    cell.movementCost = 1.0f;
                    cell.coverValue = 0.1f;
                }
            } else {
                // High elevation = mountains
                cell.type = TerrainType::Mountain;
                cell.movementCost = 3.0f;
                cell.coverValue = 0.4f;
            }
        }
    }
    
    // Add some roads (horizontal and vertical through center)
    const int roadY = height / 2;
    const int roadX = width / 2;
    for (int x = 0; x < width; ++x) {
        TerrainCell& cell = m_cells[roadY * width + x];
        if (cell.type != TerrainType::Water) {
            cell.type = TerrainType::Road;
            cell.movementCost = 0.5f;
            cell.coverValue = 0.0f;
        }
    }
    for (int y = 0; y < height; ++y) {
        TerrainCell& cell = m_cells[y * width + roadX];
        if (cell.type != TerrainType::Water) {
            cell.type = TerrainType::Road;
            cell.movementCost = 0.5f;
            cell.coverValue = 0.0f;
        }
    }
    
    updateImage();
    
    qDebug() << "TerrainLayer: Generated" << width << "x" << height << "terrain (seed:" << seed << ")";
}

void TerrainLayer::clear()
{
    m_cells.clear();
    m_gridWidth = 0;
    m_gridHeight = 0;
    m_terrainImage = QImage();
    m_imageValid = false;
    update();
}

void TerrainLayer::setSceneBounds(const QRectF& bounds)
{
    prepareGeometryChange();
    m_sceneBounds = bounds;
    update();
}

TerrainCell TerrainLayer::getCell(int x, int y) const
{
    if (x < 0 || x >= m_gridWidth || y < 0 || y >= m_gridHeight) {
        return TerrainCell();
    }
    return m_cells[y * m_gridWidth + x];
}

TerrainType TerrainLayer::getTerrainAt(double lat, double lon) const
{
    if (m_cells.empty()) {
        return TerrainType::Open;
    }
    
    // Convert geo to grid coordinates
    const double latNorm = (lat - m_minLat) / (m_maxLat - m_minLat);
    const double lonNorm = (lon - m_minLon) / (m_maxLon - m_minLon);
    
    const int x = static_cast<int>(lonNorm * m_gridWidth);
    const int y = static_cast<int>((1.0 - latNorm) * m_gridHeight);  // Flip Y
    
    return getCell(x, y).type;
}

void TerrainLayer::setElevationVisible(bool visible)
{
    m_showElevation = visible;
    updateImage();
}

QColor TerrainLayer::colorForTerrain(TerrainType type)
{
    switch (type) {
        case TerrainType::Open:     return QColor(180, 200, 140);  // Light green
        case TerrainType::Forest:   return QColor(60, 120, 60);    // Dark green
        case TerrainType::Urban:    return QColor(160, 160, 160);  // Gray
        case TerrainType::Water:    return QColor(100, 150, 200);  // Blue
        case TerrainType::Mountain: return QColor(140, 120, 100);  // Brown
        case TerrainType::Desert:   return QColor(220, 200, 160);  // Tan
        case TerrainType::Marsh:    return QColor(120, 160, 140);  // Teal
        case TerrainType::Road:     return QColor(100, 80, 60);    // Dark brown
        default:                    return QColor(200, 200, 200);
    }
}

QString TerrainLayer::nameForTerrain(TerrainType type)
{
    switch (type) {
        case TerrainType::Open:     return "Open";
        case TerrainType::Forest:   return "Forest";
        case TerrainType::Urban:    return "Urban";
        case TerrainType::Water:    return "Water";
        case TerrainType::Mountain: return "Mountain";
        case TerrainType::Desert:   return "Desert";
        case TerrainType::Marsh:    return "Marsh";
        case TerrainType::Road:     return "Road";
        default:                    return "Unknown";
    }
}

void TerrainLayer::updateImage()
{
    if (m_cells.empty() || m_gridWidth <= 0 || m_gridHeight <= 0) {
        m_imageValid = false;
        update();
        return;
    }
    
    m_terrainImage = QImage(m_gridWidth, m_gridHeight, QImage::Format_RGB32);
    
    // Find elevation range for shading
    float minElev = 999999.0f;
    float maxElev = -999999.0f;
    if (m_showElevation) {
        for (const auto& cell : m_cells) {
            minElev = std::min(minElev, cell.elevation);
            maxElev = std::max(maxElev, cell.elevation);
        }
    }
    const float elevRange = (maxElev - minElev > 0.1f) ? (maxElev - minElev) : 1.0f;
    
    for (int y = 0; y < m_gridHeight; ++y) {
        for (int x = 0; x < m_gridWidth; ++x) {
            const TerrainCell& cell = m_cells[y * m_gridWidth + x];
            QColor color = colorForTerrain(cell.type);
            
            // Apply elevation shading
            if (m_showElevation && cell.type != TerrainType::Water) {
                const float elevNorm = (cell.elevation - minElev) / elevRange;
                // Darken low areas, brighten high areas
                const int shade = static_cast<int>((elevNorm - 0.5f) * 40);
                color = QColor(
                    qBound(0, color.red() + shade, 255),
                    qBound(0, color.green() + shade, 255),
                    qBound(0, color.blue() + shade, 255)
                );
            }
            
            m_terrainImage.setPixelColor(x, y, color);
        }
    }
    
    m_imageValid = true;
    update();
}

void TerrainLayer::generatePerlinNoise(std::vector<float>& noise, int width, int height, uint32_t seed)
{
    // Simple multi-octave value noise (simplified Perlin-like)
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    // Generate random gradients at each grid point for multiple octaves
    const int numOctaves = 4;
    const int baseFreq = 8;
    
    noise.assign(width * height, 0.0f);
    
    float amplitude = 1.0f;
    float totalAmplitude = 0.0f;
    
    for (int octave = 0; octave < numOctaves; ++octave) {
        const int freq = baseFreq * (1 << octave);
        const int gridW = freq + 1;
        const int gridH = freq + 1;
        
        // Random values at grid points
        std::vector<float> grid(gridW * gridH);
        for (auto& v : grid) {
            v = dist(rng);
        }
        
        // Interpolate
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float fx = static_cast<float>(x) / width * freq;
                const float fy = static_cast<float>(y) / height * freq;
                
                const int x0 = static_cast<int>(fx);
                const int y0 = static_cast<int>(fy);
                const int x1 = std::min(x0 + 1, gridW - 1);
                const int y1 = std::min(y0 + 1, gridH - 1);
                
                const float tx = fx - x0;
                const float ty = fy - y0;
                
                // Smoothstep interpolation
                const float sx = tx * tx * (3.0f - 2.0f * tx);
                const float sy = ty * ty * (3.0f - 2.0f * ty);
                
                // Bilinear interpolation
                const float v00 = grid[y0 * gridW + x0];
                const float v10 = grid[y0 * gridW + x1];
                const float v01 = grid[y1 * gridW + x0];
                const float v11 = grid[y1 * gridW + x1];
                
                const float v0 = v00 + sx * (v10 - v00);
                const float v1 = v01 + sx * (v11 - v01);
                const float value = v0 + sy * (v1 - v0);
                
                noise[y * width + x] += value * amplitude;
            }
        }
        
        totalAmplitude += amplitude;
        amplitude *= 0.5f;
    }
    
    // Normalize
    for (auto& v : noise) {
        v /= totalAmplitude;
    }
}

} // namespace athena::ui
