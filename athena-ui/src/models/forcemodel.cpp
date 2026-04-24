/**
 * @file forcemodel.cpp
 * @brief Model for force composition tree implementation
 */

#include "forcemodel.hpp"

#include <QMimeData>
#include <QUuid>
#include <QDebug>

namespace athena::ui {

struct ForceModel::TreeItem {
    QString id;           // Unique ID for undo/redo
    QString name;
    QString platformId;
    int quantity = 0;
    std::vector<std::unique_ptr<TreeItem>> children;
    TreeItem* parent = nullptr;
    
    TreeItem(const QString& n = QString(), TreeItem* p = nullptr)
        : name(n), parent(p) {}
    
    int totalUnits() const {
        int total = quantity;
        for (const auto& child : children) {
            total += child->totalUnits();
        }
        return total;
    }
};

ForceModel::ForceModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_rootItem(std::make_unique<TreeItem>())
{
    setupInitialStructure();
}

ForceModel::~ForceModel() = default;

QModelIndex ForceModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    
    TreeItem* parentItem = parent.isValid()
        ? static_cast<TreeItem*>(parent.internalPointer())
        : m_rootItem.get();
    
    if (row < static_cast<int>(parentItem->children.size())) {
        return createIndex(row, column, parentItem->children[row].get());
    }
    
    return QModelIndex();
}

QModelIndex ForceModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }
    
    TreeItem* childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem* parentItem = childItem->parent;
    
    if (parentItem == m_rootItem.get()) {
        return QModelIndex();
    }
    
    TreeItem* grandparent = parentItem->parent;
    if (!grandparent) {
        return QModelIndex();
    }
    
    for (size_t i = 0; i < grandparent->children.size(); ++i) {
        if (grandparent->children[i].get() == parentItem) {
            return createIndex(static_cast<int>(i), 0, parentItem);
        }
    }
    
    return QModelIndex();
}

int ForceModel::rowCount(const QModelIndex& parent) const
{
    TreeItem* parentItem = parent.isValid()
        ? static_cast<TreeItem*>(parent.internalPointer())
        : m_rootItem.get();
    
    return static_cast<int>(parentItem->children.size());
}

int ForceModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 2;  // Name, Quantity
}

QVariant ForceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return item->name;
            case 1: return item->quantity > 0 ? QString::number(item->quantity) : QString();
        }
    }
    
    if (role == Qt::ForegroundRole && index.column() == 0) {
        // Color BLUFOR blue, OPFOR red
        if (item->name == "BLUFOR") {
            return QColor(0, 100, 200);
        } else if (item->name == "OPFOR") {
            return QColor(200, 50, 50);
        }
    }
    
    return QVariant();
}

QVariant ForceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "Unit";
            case 1: return "Qty";
        }
    }
    return QVariant();
}

Qt::ItemFlags ForceModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    
    if (index.isValid()) {
        TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
        // BLUFOR/OPFOR nodes accept drops
        if (item->name == "BLUFOR" || item->name == "OPFOR") {
            return defaultFlags | Qt::ItemIsDropEnabled;
        }
        // Unit items can be dragged (for reordering)
        if (!item->platformId.isEmpty()) {
            return defaultFlags | Qt::ItemIsDragEnabled;
        }
    }
    
    return defaultFlags;
}

bool ForceModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    Q_UNUSED(row)
    Q_UNUSED(column)
    
    if (action == Qt::IgnoreAction) {
        return true;
    }
    
    if (!data->hasFormat("application/x-athena-platform")) {
        return false;
    }
    
    // Get platform ID from mime data
    QString platformId = QString::fromUtf8(data->data("application/x-athena-platform"));
    
    // Determine if BLUFOR or OPFOR
    TreeItem* parentItem = parent.isValid()
        ? static_cast<TreeItem*>(parent.internalPointer())
        : m_rootItem.get();
    
    bool blufor = (parentItem->name == "BLUFOR");
    
    addUnit(blufor, platformId, 1);
    
    return true;
}

Qt::DropActions ForceModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList ForceModel::mimeTypes() const
{
    return {"application/x-athena-platform"};
}

void ForceModel::clear()
{
    beginResetModel();
    m_rootItem->children.clear();
    setupInitialStructure();
    endResetModel();
    
    emit forceChanged();
}

int ForceModel::bluforCount() const
{
    if (m_rootItem->children.size() > 0) {
        return m_rootItem->children[0]->totalUnits();
    }
    return 0;
}

int ForceModel::opforCount() const
{
    if (m_rootItem->children.size() > 1) {
        return m_rootItem->children[1]->totalUnits();
    }
    return 0;
}

void ForceModel::addUnit(bool blufor, const QString& platformId, int quantity)
{
    TreeItem* sideItem = blufor 
        ? m_rootItem->children[0].get() 
        : m_rootItem->children[1].get();
    
    QModelIndex parentIndex = index(blufor ? 0 : 1, 0, QModelIndex());
    
    beginInsertRows(parentIndex, static_cast<int>(sideItem->children.size()), 
                    static_cast<int>(sideItem->children.size()));
    
    auto unit = std::make_unique<TreeItem>(platformId, sideItem);
    unit->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    unit->platformId = platformId;
    unit->quantity = quantity;
    
    QString unitId = unit->id;  // Save before move
    sideItem->children.push_back(std::move(unit));
    
    endInsertRows();
    
    emit forceChanged();
    
    // Emit for Tree → Map sync
    ForceUnit forceUnit;
    forceUnit.id = unitId;
    forceUnit.platformId = platformId;
    forceUnit.displayName = platformId;
    forceUnit.quantity = quantity;
    forceUnit.isBlufor = blufor;
    // Default position based on side
    forceUnit.lat = blufor ? 50.5 : 49.5;
    forceUnit.lon = 10.0;
    emit unitAddedFromUI(forceUnit);
}

void ForceModel::addUnit(const ForceUnit& forceUnit)
{
    // Use display name if available, otherwise platformId
    const QString displayName = forceUnit.displayName.isEmpty() 
        ? forceUnit.platformId 
        : forceUnit.displayName;
    
    TreeItem* sideItem = forceUnit.isBlufor 
        ? m_rootItem->children[0].get() 
        : m_rootItem->children[1].get();
    
    QModelIndex parentIndex = index(forceUnit.isBlufor ? 0 : 1, 0, QModelIndex());
    
    beginInsertRows(parentIndex, static_cast<int>(sideItem->children.size()), 
                    static_cast<int>(sideItem->children.size()));
    
    auto unit = std::make_unique<TreeItem>(displayName, sideItem);
    // Use existing ID or generate new one
    unit->id = forceUnit.id.isEmpty() 
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : forceUnit.id;
    unit->platformId = forceUnit.platformId;
    unit->quantity = forceUnit.quantity;
    sideItem->children.push_back(std::move(unit));
    
    endInsertRows();
    
    emit forceChanged();
}

void ForceModel::removeUnit(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    TreeItem* parentItem = item->parent;
    
    if (!parentItem) {
        return;
    }
    
    QModelIndex parentIndex = this->parent(index);
    
    for (size_t i = 0; i < parentItem->children.size(); ++i) {
        if (parentItem->children[i].get() == item) {
            beginRemoveRows(parentIndex, static_cast<int>(i), static_cast<int>(i));
            parentItem->children.erase(parentItem->children.begin() + i);
            endRemoveRows();
            break;
        }
    }
    
    emit forceChanged();
}

void ForceModel::removeUnitByPlatform(const QString& platformId, bool blufor)
{
    TreeItem* sideItem = blufor 
        ? m_rootItem->children[0].get() 
        : m_rootItem->children[1].get();
    
    QModelIndex parentIndex = index(blufor ? 0 : 1, 0, QModelIndex());
    
    for (size_t i = 0; i < sideItem->children.size(); ++i) {
        if (sideItem->children[i]->platformId == platformId) {
            beginRemoveRows(parentIndex, static_cast<int>(i), static_cast<int>(i));
            sideItem->children.erase(sideItem->children.begin() + i);
            endRemoveRows();
            emit forceChanged();
            return;
        }
    }
}

void ForceModel::updateUnitQuantity(const QString& platformId, bool blufor, int quantity)
{
    TreeItem* sideItem = blufor 
        ? m_rootItem->children[0].get() 
        : m_rootItem->children[1].get();
    
    for (auto& child : sideItem->children) {
        if (child->platformId == platformId) {
            child->quantity = quantity;
            emit forceChanged();
            
            // Emit data changed for the specific row
            QModelIndex parentIndex = index(blufor ? 0 : 1, 0, QModelIndex());
            for (size_t i = 0; i < sideItem->children.size(); ++i) {
                if (sideItem->children[i].get() == child.get()) {
                    QModelIndex idx = this->index(static_cast<int>(i), 0, parentIndex);
                    emit dataChanged(idx, idx);
                    break;
                }
            }
            return;
        }
    }
}

void ForceModel::setupInitialStructure()
{
    // Create BLUFOR and OPFOR root nodes
    auto blufor = std::make_unique<TreeItem>("BLUFOR", m_rootItem.get());
    auto opfor = std::make_unique<TreeItem>("OPFOR", m_rootItem.get());
    
    m_rootItem->children.push_back(std::move(blufor));
    m_rootItem->children.push_back(std::move(opfor));
}

std::vector<ForceUnit> ForceModel::getUnits() const
{
    std::vector<ForceUnit> units;
    
    // Collect BLUFOR units
    if (m_rootItem->children.size() > 0) {
        collectUnits(m_rootItem->children[0].get(), true, units);
    }
    
    // Collect OPFOR units
    if (m_rootItem->children.size() > 1) {
        collectUnits(m_rootItem->children[1].get(), false, units);
    }
    
    return units;
}

std::vector<ForceUnit> ForceModel::getUnits(bool blufor) const
{
    std::vector<ForceUnit> units;
    
    const size_t sideIndex = blufor ? 0 : 1;
    if (m_rootItem->children.size() > sideIndex) {
        collectUnits(m_rootItem->children[sideIndex].get(), blufor, units);
    }
    
    return units;
}

bool ForceModel::isValidForSimulation() const
{
    return bluforCount() > 0 && opforCount() > 0;
}

void ForceModel::loadUnits(const std::vector<ForceUnit>& units)
{
    beginResetModel();
    
    // Clear existing units but keep BLUFOR/OPFOR nodes
    if (m_rootItem->children.size() > 0) {
        m_rootItem->children[0]->children.clear();
    }
    if (m_rootItem->children.size() > 1) {
        m_rootItem->children[1]->children.clear();
    }
    
    // Add loaded units
    for (const auto& unit : units) {
        TreeItem* sideItem = unit.isBlufor 
            ? m_rootItem->children[0].get() 
            : m_rootItem->children[1].get();
        
        auto treeUnit = std::make_unique<TreeItem>(unit.displayName, sideItem);
        treeUnit->platformId = unit.platformId;
        treeUnit->quantity = unit.quantity;
        sideItem->children.push_back(std::move(treeUnit));
    }
    
    endResetModel();
    emit forceChanged();
}

void ForceModel::collectUnits(const TreeItem* item, bool isBlufor, std::vector<ForceUnit>& units) const
{
    // If this item has a platformId, it's a unit
    if (!item->platformId.isEmpty() && item->quantity > 0) {
        ForceUnit unit;
        unit.id = item->id;
        unit.platformId = item->platformId;
        unit.displayName = item->name;
        unit.quantity = item->quantity;
        unit.isBlufor = isBlufor;
        // Default positions (will be set by map placement later)
        unit.lat = isBlufor ? 50.0 : 49.5;  // Simple offset for now
        unit.lon = 10.0;
        units.push_back(unit);
    }
    
    // Recursively collect from children
    for (const auto& child : item->children) {
        collectUnits(child.get(), isBlufor, units);
    }
}

// Helper to find item by unique ID
namespace {
ForceModel::TreeItem* findItemById(ForceModel::TreeItem* root, const QString& id)
{
    if (root->id == id) {
        return root;
    }
    for (auto& child : root->children) {
        if (auto* found = findItemById(child.get(), id)) {
            return found;
        }
    }
    return nullptr;
}
} // anonymous namespace

void ForceModel::removeUnit(const QString& unitId)
{
    // Search in both BLUFOR and OPFOR
    for (size_t sideIdx = 0; sideIdx < 2 && sideIdx < m_rootItem->children.size(); ++sideIdx) {
        TreeItem* sideItem = m_rootItem->children[sideIdx].get();
        
        for (size_t i = 0; i < sideItem->children.size(); ++i) {
            if (sideItem->children[i]->id == unitId) {
                QModelIndex parentIndex = index(static_cast<int>(sideIdx), 0, QModelIndex());
                beginRemoveRows(parentIndex, static_cast<int>(i), static_cast<int>(i));
                sideItem->children.erase(sideItem->children.begin() + i);
                endRemoveRows();
                emit forceChanged();
                return;
            }
        }
    }
}

void ForceModel::setUnitSide(const QString& unitId, bool blufor)
{
    // Find the unit and move it to the other side
    for (size_t sideIdx = 0; sideIdx < 2 && sideIdx < m_rootItem->children.size(); ++sideIdx) {
        TreeItem* sideItem = m_rootItem->children[sideIdx].get();
        bool currentlyBlufor = (sideIdx == 0);
        
        for (size_t i = 0; i < sideItem->children.size(); ++i) {
            if (sideItem->children[i]->id == unitId) {
                if (currentlyBlufor == blufor) {
                    // Already on correct side
                    return;
                }
                
                // Move to other side
                TreeItem* targetSide = blufor 
                    ? m_rootItem->children[0].get() 
                    : m_rootItem->children[1].get();
                
                // Remove from current side
                QModelIndex oldParentIndex = index(static_cast<int>(sideIdx), 0, QModelIndex());
                beginRemoveRows(oldParentIndex, static_cast<int>(i), static_cast<int>(i));
                auto unitPtr = std::move(sideItem->children[i]);
                sideItem->children.erase(sideItem->children.begin() + i);
                endRemoveRows();
                
                // Add to target side
                unitPtr->parent = targetSide;
                QModelIndex newParentIndex = index(blufor ? 0 : 1, 0, QModelIndex());
                beginInsertRows(newParentIndex, static_cast<int>(targetSide->children.size()), 
                               static_cast<int>(targetSide->children.size()));
                targetSide->children.push_back(std::move(unitPtr));
                endInsertRows();
                
                emit forceChanged();
                return;
            }
        }
    }
}

void ForceModel::setUnitQuantity(const QString& unitId, int quantity)
{
    for (size_t sideIdx = 0; sideIdx < 2 && sideIdx < m_rootItem->children.size(); ++sideIdx) {
        TreeItem* sideItem = m_rootItem->children[sideIdx].get();
        
        for (size_t i = 0; i < sideItem->children.size(); ++i) {
            if (sideItem->children[i]->id == unitId) {
                sideItem->children[i]->quantity = quantity;
                
                QModelIndex parentIndex = index(static_cast<int>(sideIdx), 0, QModelIndex());
                QModelIndex idx = this->index(static_cast<int>(i), 1, parentIndex);
                emit dataChanged(idx, idx);
                emit forceChanged();
                return;
            }
        }
    }
}

} // namespace athena::ui
