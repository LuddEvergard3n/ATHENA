/**
 * @file forcemodel.hpp
 * @brief Model for force composition tree
 * 
 * v0.6.6: Added ForceUnit struct and getUnits() for scenario building
 */

#ifndef ATHENA_UI_FORCEMODEL_HPP
#define ATHENA_UI_FORCEMODEL_HPP

#include <QAbstractItemModel>
#include <QString>
#include <memory>
#include <vector>

namespace athena::ui {

/**
 * @brief Represents a single unit in the force composition
 */
struct ForceUnit {
    QString id;              // Unique ID for undo/redo tracking
    QString platformId;      // Platform ID from database (e.g., "m1a2-sep-v3")
    QString displayName;     // Display name (e.g., "M1A2 SEP v3 Abrams")
    int quantity = 1;        // Number of units
    bool isBlufor = true;    // true = BLUFOR, false = OPFOR
    
    // Position for map placement
    double lat = 0.0;
    double lon = 0.0;
};

/**
 * @brief Tree model for force composition (OOB)
 * 
 * Organizes forces as: Side (BLUFOR/OPFOR) -> Echelon -> Units
 */
class ForceModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit ForceModel(QObject* parent = nullptr);
    ~ForceModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;

    // Custom methods
    void clear();
    int bluforCount() const;
    int opforCount() const;
    
    void addUnit(bool blufor, const QString& platformId, int quantity = 1);
    void addUnit(const ForceUnit& unit);
    void removeUnit(const QModelIndex& index);
    
    /**
     * @brief Remove unit by platform ID and side
     */
    void removeUnitByPlatform(const QString& platformId, bool blufor);
    
    /**
     * @brief Update quantity for a unit
     */
    void updateUnitQuantity(const QString& platformId, bool blufor, int quantity);
    
    /**
     * @brief Remove unit by unique ID (for undo/redo)
     */
    void removeUnit(const QString& unitId);
    
    /**
     * @brief Set unit side by unique ID (for undo/redo)
     */
    void setUnitSide(const QString& unitId, bool blufor);
    
    /**
     * @brief Set unit quantity by unique ID (for undo/redo)
     */
    void setUnitQuantity(const QString& unitId, int quantity);
    
    /**
     * @brief Get all units in the force composition
     * @return Vector of ForceUnit structs
     */
    std::vector<ForceUnit> getUnits() const;
    
    /**
     * @brief Get units for a specific side
     * @param blufor true for BLUFOR, false for OPFOR
     * @return Vector of ForceUnit structs
     */
    std::vector<ForceUnit> getUnits(bool blufor) const;
    
    /**
     * @brief Check if force composition is valid for simulation
     * @return true if at least one unit on each side
     */
    bool isValidForSimulation() const;
    
    /**
     * @brief Load units from file data (replaces current units)
     * @param units Vector of ForceUnit to load
     */
    void loadUnits(const std::vector<ForceUnit>& units);

signals:
    void forceChanged();
    
    /**
     * @brief Emitted when unit added from UI (not from map sync)
     * Used for Tree → Map synchronization
     */
    void unitAddedFromUI(const ForceUnit& unit);

private:
    struct TreeItem;
    std::unique_ptr<TreeItem> m_rootItem;
    
    void setupInitialStructure();
    void collectUnits(const TreeItem* item, bool isBlufor, std::vector<ForceUnit>& units) const;
};

} // namespace athena::ui

#endif // ATHENA_UI_FORCEMODEL_HPP
