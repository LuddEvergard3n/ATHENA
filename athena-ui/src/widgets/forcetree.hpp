/**
 * @file forcetree.hpp
 * @brief Force composition tree widget
 * 
 * v0.6.6: Added getUnits() for scenario building
 */

#ifndef ATHENA_UI_FORCETREE_HPP
#define ATHENA_UI_FORCETREE_HPP

#include <QWidget>
#include <QString>
#include <vector>

class QTreeView;

namespace athena::ui {

class ForceModel;
struct ForceUnit;

/**
 * @brief Widget for managing force composition (OOB)
 * 
 * Displays BLUFOR/OPFOR force structure as a tree.
 * Supports drag-drop from Platform Browser.
 */
class ForceTree : public QWidget
{
    Q_OBJECT

public:
    explicit ForceTree(QWidget* parent = nullptr);
    ~ForceTree() override;

    void clear();
    int bluforCount() const;
    int opforCount() const;
    
    /**
     * @brief Get all units for scenario building
     */
    std::vector<ForceUnit> getUnits() const;
    
    /**
     * @brief Check if force composition is valid for simulation
     */
    bool isValidForSimulation() const;
    
    /**
     * @brief Load units from file data
     */
    void loadUnits(const std::vector<ForceUnit>& units);
    
    /**
     * @brief Add a single unit (for map sync)
     */
    void addUnit(const ForceUnit& unit);
    
    /**
     * @brief Remove a unit by platform ID and side
     */
    void removeUnit(const QString& platformId, bool blufor);
    
    /**
     * @brief Update unit quantity
     */
    void updateUnitQuantity(const QString& platformId, bool blufor, int quantity);
    
    /**
     * @brief Get the underlying model
     */
    ForceModel* model() const { return m_model; }

signals:
    /**
     * @brief Emitted when unit added from UI (for Tree → Map sync)
     */
    void unitAddedFromUI(const ForceUnit& unit);

public slots:
    void onPlatformSelected(const QString& platformId);

signals:
    void forceChanged();

private:
    void setupUi();

    QTreeView* m_treeView = nullptr;
    ForceModel* m_model = nullptr;
};

} // namespace athena::ui

#endif // ATHENA_UI_FORCETREE_HPP
