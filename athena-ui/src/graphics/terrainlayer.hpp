/**
 * @file terrainlayer.hpp
 * @brief Terrain rendering layer for map
 * 
 * v0.6.9: Full terrain visualization with multiple terrain types
 */

#ifndef ATHENA_UI_TERRAINLAYER_HPP
#define ATHENA_UI_TERRAINLAYER_HPP

#include <QGraphicsItem>
#include <QImage>
#include <QColor>
#include <vector>

namespace athena::ui {

/**
 * @brief Terrain type enumeration
 */
enum class TerrainType : uint8_t {
    Open = 0,       // Open/clear terrain (grassland)
    Forest = 1,     // Forested area
    Urban = 2,      // Urban/built-up area
    Water = 3,      // Water (rivers, lakes)
    Mountain = 4,   // Mountainous terrain
    Desert = 5,     // Desert/arid
    Marsh = 6,      // Marsh/wetland
    Road = 7,       // Road network
    COUNT
};

/**
 * @brief Terrain cell data
 */
struct TerrainCell {
    TerrainType type = TerrainType::Open;
    float elevation = 0.0f;     // meters above sea level
    float movementCost = 1.0f;  // multiplier (1.0 = normal)
    float coverValue = 0.0f;    // 0-1 concealment
};

/**
 * @brief Graphics item for rendering terrain
 * 
 * Renders a grid-based terrain map with:
 * - Color-coded terrain types
 * - Optional elevation shading
 * - Procedural generation for demos
 */
class TerrainLayer : public QGraphicsItem
{
public:
    TerrainLayer(QGraphicsItem* parent = nullptr);
    ~TerrainLayer() override;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    /**
     * @brief Load terrain from JSON file
     * @param filePath Path to terrain file
     * @return true on success
     */
    bool loadTerrain(const QString& filePath);

    /**
     * @brief Generate procedural terrain for demo
     * @param width Grid width in cells
     * @param height Grid height in cells
     * @param seed Random seed (0 = random)
     */
    void generateTerrain(int width, int height, uint32_t seed = 0);

    /**
     * @brief Clear all terrain data
     */
    void clear();

    /**
     * @brief Set scene bounds for rendering
     */
    void setSceneBounds(const QRectF& bounds);

    /**
     * @brief Get terrain at cell position
     */
    TerrainCell getCell(int x, int y) const;

    /**
     * @brief Get terrain type at geo position
     */
    TerrainType getTerrainAt(double lat, double lon) const;

    // Grid dimensions
    int gridWidth() const { return m_gridWidth; }
    int gridHeight() const { return m_gridHeight; }

    // Visibility
    void setElevationVisible(bool visible);
    bool isElevationVisible() const { return m_showElevation; }

    // Colors
    static QColor colorForTerrain(TerrainType type);
    static QString nameForTerrain(TerrainType type);

private:
    void updateImage();
    void generatePerlinNoise(std::vector<float>& noise, int width, int height, uint32_t seed);

    QRectF m_sceneBounds;
    int m_gridWidth = 0;
    int m_gridHeight = 0;
    
    std::vector<TerrainCell> m_cells;
    QImage m_terrainImage;
    bool m_imageValid = false;
    bool m_showElevation = true;
    
    // Geo bounds (for coordinate conversion)
    double m_minLat = 49.0;
    double m_maxLat = 51.0;
    double m_minLon = 9.0;
    double m_maxLon = 11.0;
};

} // namespace athena::ui

#endif // ATHENA_UI_TERRAINLAYER_HPP
