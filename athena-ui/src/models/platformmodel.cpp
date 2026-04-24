/**
 * @file platformmodel.cpp
 * @brief Model for platform database implementation
 * 
 * Uses athena-core PlatformDatabase for loading platform data.
 */

#include "platformmodel.hpp"

#include <QMimeData>
#include <QIcon>
#include <QDebug>

namespace athena::ui {

// =============================================================================
// Tree Item Structure
// =============================================================================

struct PlatformModel::TreeItem {
    QString name;           // Display name
    QString id;             // Platform ID (empty for categories)
    QString country;        // Country code
    QString type;           // Platform type string
    athena::PlatformCategory category = athena::PlatformCategory::UNKNOWN;
    
    std::vector<std::unique_ptr<TreeItem>> children;
    TreeItem* parent = nullptr;
    
    TreeItem(const QString& n = QString(), const QString& i = QString(), TreeItem* p = nullptr)
        : name(n), id(i), parent(p) {}
    
    bool isCategory() const { return id.isEmpty() && !children.empty(); }
    bool isPlatform() const { return !id.isEmpty(); }
};

// =============================================================================
// Construction / Destruction
// =============================================================================

PlatformModel::PlatformModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_rootItem(std::make_unique<TreeItem>())
{
}

PlatformModel::~PlatformModel() = default;

// =============================================================================
// QAbstractItemModel Interface
// =============================================================================

QModelIndex PlatformModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    
    TreeItem* parentItem = parent.isValid() 
        ? static_cast<TreeItem*>(parent.internalPointer())
        : m_rootItem.get();
    
    if (row >= 0 && row < static_cast<int>(parentItem->children.size())) {
        return createIndex(row, column, parentItem->children[row].get());
    }
    
    return QModelIndex();
}

QModelIndex PlatformModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }
    
    TreeItem* childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem* parentItem = childItem->parent;
    
    if (parentItem == m_rootItem.get() || parentItem == nullptr) {
        return QModelIndex();
    }
    
    // Find row of parent in grandparent
    TreeItem* grandparent = parentItem->parent;
    if (!grandparent) {
        grandparent = m_rootItem.get();
    }
    
    for (size_t i = 0; i < grandparent->children.size(); ++i) {
        if (grandparent->children[i].get() == parentItem) {
            return createIndex(static_cast<int>(i), 0, parentItem);
        }
    }
    
    return QModelIndex();
}

int PlatformModel::rowCount(const QModelIndex& parent) const
{
    TreeItem* parentItem = parent.isValid()
        ? static_cast<TreeItem*>(parent.internalPointer())
        : m_rootItem.get();
    
    return static_cast<int>(parentItem->children.size());
}

int PlatformModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 3;  // Name, Country, Type
}

QVariant PlatformModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return item->name;
            case 1: return item->country;
            case 2: return item->type;
        }
    }
    
    if (role == Qt::FontRole && item->isCategory()) {
        QFont font;
        font.setBold(true);
        return font;
    }
    
    if (role == Qt::ToolTipRole && item->isPlatform()) {
        const auto* spec = m_database.get(item->id.toStdString());
        if (spec) {
            return QString("%1\nCountry: %2\nActive: %3")
                .arg(QString::fromStdString(spec->designation))
                .arg(QString::fromStdString(spec->country))
                .arg(spec->operators.empty() ? 0 : spec->operators[0].quantity);
        }
    }
    
    return QVariant();
}

QVariant PlatformModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "Platform";
            case 1: return "Country";
            case 2: return "Type";
        }
    }
    return QVariant();
}

Qt::ItemFlags PlatformModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    
    // Platform items (leaves) are draggable
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    if (item->isPlatform()) {
        flags |= Qt::ItemIsDragEnabled;
    }
    
    return flags;
}

QMimeData* PlatformModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.isEmpty()) {
        return nullptr;
    }
    
    // Get first item's platform ID
    TreeItem* item = static_cast<TreeItem*>(indexes.first().internalPointer());
    if (!item->isPlatform()) {
        return nullptr;
    }
    
    QMimeData* mimeData = new QMimeData();
    mimeData->setData("application/x-athena-platform", item->id.toUtf8());
    mimeData->setText(item->name);
    
    return mimeData;
}

QStringList PlatformModel::mimeTypes() const
{
    return {"application/x-athena-platform", "text/plain"};
}

// =============================================================================
// Public Methods
// =============================================================================

int PlatformModel::loadFromDirectory(const QString& path)
{
    qDebug() << "Loading platform database from:" << path;
    
    beginResetModel();
    
    // Clear existing data
    m_rootItem->children.clear();
    
    // Load from athena-core
    auto result = m_database.load_all(path.toStdString());
    
    if (!result) {
        qWarning() << "Failed to load platform database:" << QString::fromStdString(result.error());
        endResetModel();
        return 0;
    }
    
    const int count = static_cast<int>(result.value());
    qDebug() << "Loaded" << count << "platforms";
    
    // Build tree structure
    buildTreeFromDatabase();
    
    endResetModel();
    
    return count;
}

QString PlatformModel::platformId(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QString();
    }
    
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    return item->id;
}

const athena::PlatformSpec* PlatformModel::getPlatform(const QString& id) const
{
    return m_database.get(id.toStdString());
}

int PlatformModel::totalPlatforms() const
{
    return static_cast<int>(m_database.all().size());
}

// =============================================================================
// Private Methods
// =============================================================================

void PlatformModel::buildTreeFromDatabase()
{
    // Create category nodes and populate with platforms
    const auto& platforms = m_database.all();
    
    for (const auto& [id, spec] : platforms) {
        // Find or create category
        TreeItem* categoryItem = findOrCreateCategory(categoryDisplayName(spec.category));
        
        // Create platform item
        auto platformItem = std::make_unique<TreeItem>(
            QString::fromStdString(spec.designation),
            QString::fromStdString(id),
            categoryItem
        );
        platformItem->country = QString::fromStdString(spec.country).toUpper();
        platformItem->type = QString::fromStdString(spec.classification);
        platformItem->category = spec.category;
        
        categoryItem->children.push_back(std::move(platformItem));
    }
    
    // Sort categories alphabetically
    std::sort(m_rootItem->children.begin(), m_rootItem->children.end(),
        [](const auto& a, const auto& b) {
            return a->name < b->name;
        });
    
    // Sort platforms within each category
    for (auto& category : m_rootItem->children) {
        std::sort(category->children.begin(), category->children.end(),
            [](const auto& a, const auto& b) {
                // Sort by country first, then by name
                if (a->country != b->country) {
                    return a->country < b->country;
                }
                return a->name < b->name;
            });
    }
}

PlatformModel::TreeItem* PlatformModel::findOrCreateCategory(const QString& name)
{
    // Search for existing category
    for (auto& child : m_rootItem->children) {
        if (child->name == name) {
            return child.get();
        }
    }
    
    // Create new category
    auto category = std::make_unique<TreeItem>(name, QString(), m_rootItem.get());
    TreeItem* ptr = category.get();
    m_rootItem->children.push_back(std::move(category));
    return ptr;
}

QString PlatformModel::categoryDisplayName(athena::PlatformCategory cat)
{
    switch (cat) {
        case athena::PlatformCategory::TANK:       return "Tanks";
        case athena::PlatformCategory::IFV:        return "Infantry Fighting Vehicles";
        case athena::PlatformCategory::APC:        return "Armored Personnel Carriers";
        case athena::PlatformCategory::ARTILLERY:  return "Artillery";
        case athena::PlatformCategory::MLRS:       return "Multiple Launch Rocket Systems";
        case athena::PlatformCategory::SAM:        return "Surface-to-Air Missiles";
        case athena::PlatformCategory::ATGM:       return "Anti-Tank Guided Missiles";
        case athena::PlatformCategory::MANPADS:    return "MANPADS";
        case athena::PlatformCategory::AIRCRAFT:   return "Aircraft";
        case athena::PlatformCategory::HELICOPTER: return "Helicopters";
        case athena::PlatformCategory::BOMBER:     return "Bombers";
        case athena::PlatformCategory::SHIP:       return "Ships";
        case athena::PlatformCategory::SUBMARINE:  return "Submarines";
        case athena::PlatformCategory::UAV:        return "Unmanned Aerial Vehicles";
        default:                                   return "Unknown";
    }
}

} // namespace athena::ui
