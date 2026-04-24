/**
 * @file platformmodel.hpp
 * @brief Model for platform database tree view
 * 
 * Integrates with athena-core PlatformDatabase to display
 * all 453+ platforms in a hierarchical tree view.
 */

#ifndef ATHENA_UI_PLATFORMMODEL_HPP
#define ATHENA_UI_PLATFORMMODEL_HPP

#include <QAbstractItemModel>
#include <QString>
#include <vector>
#include <memory>

// ATHENA Core integration
#include <athena/platform_loader.hpp>

namespace athena::ui {

/**
 * @brief Tree model for platform database
 * 
 * Organizes platforms hierarchically by category.
 * Uses athena::PlatformDatabase for data loading.
 */
class PlatformModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit PlatformModel(QObject* parent = nullptr);
    ~PlatformModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    
    // Drag support
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    QStringList mimeTypes() const override;

    /**
     * @brief Load platform database from directory
     * @param path Directory containing platform JSON files (e.g., "data/platforms")
     * @return Number of platforms loaded
     */
    int loadFromDirectory(const QString& path);

    /**
     * @brief Get platform ID from model index
     * @param index Model index
     * @return Platform ID string, or empty if not a platform item
     */
    QString platformId(const QModelIndex& index) const;

    /**
     * @brief Get platform specification by ID
     * @param id Platform ID
     * @return Pointer to PlatformSpec, or nullptr if not found
     */
    const athena::PlatformSpec* getPlatform(const QString& id) const;
    
    /**
     * @brief Get the platform database
     * @return Pointer to the database (for ScenarioBuilder)
     */
    const athena::PlatformDatabase* getDatabase() const { return &m_database; }

    /**
     * @brief Get total number of platforms loaded
     */
    int totalPlatforms() const;

private:
    struct TreeItem;
    std::unique_ptr<TreeItem> m_rootItem;
    
    // ATHENA Core database
    athena::PlatformDatabase m_database;
    
    void buildTreeFromDatabase();
    TreeItem* findOrCreateCategory(const QString& name);
    static QString categoryDisplayName(athena::PlatformCategory cat);
};

} // namespace athena::ui

#endif // ATHENA_UI_PLATFORMMODEL_HPP
