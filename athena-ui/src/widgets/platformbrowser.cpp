/**
 * @file platformbrowser.cpp
 * @brief Platform database browser implementation
 */

#include "platformbrowser.hpp"
#include "models/platformmodel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeView>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QDebug>

namespace athena::ui {

PlatformBrowser::PlatformBrowser(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

PlatformBrowser::~PlatformBrowser() = default;

int PlatformBrowser::loadDatabase(const QString& path)
{
    qDebug() << "PlatformBrowser: Loading database from:" << path;
    
    const int count = m_model->loadFromDirectory(path);
    
    updateCountLabel();
    m_treeView->expandAll();
    
    emit databaseLoaded(count);
    return count;
}

int PlatformBrowser::platformCount() const
{
    return m_model ? m_model->totalPlatforms() : 0;
}

void PlatformBrowser::onSearchTextChanged(const QString& text)
{
    if (m_proxyModel) {
        m_proxyModel->setFilterFixedString(text);
        
        // Expand all when searching
        if (!text.isEmpty()) {
            m_treeView->expandAll();
        }
    }
}

void PlatformBrowser::onFilterChanged()
{
    // TODO: Apply country/category filters via custom filter proxy
    // For now, just update the view
    m_treeView->expandAll();
}

void PlatformBrowser::onSelectionChanged()
{
    const auto selection = m_treeView->selectionModel()->selectedIndexes();
    if (!selection.isEmpty()) {
        const QModelIndex proxyIndex = selection.first();
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        const QString platformId = m_model->platformId(sourceIndex);
        
        if (!platformId.isEmpty()) {
            emit platformSelected(platformId);
            qDebug() << "Platform selected:" << platformId;
        }
    }
}

void PlatformBrowser::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    
    // Search bar
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search platforms...");
    m_searchEdit->setClearButtonEnabled(true);
    layout->addWidget(m_searchEdit);
    
    // Filters row
    auto* filterLayout = new QHBoxLayout();
    
    m_countryFilter = new QComboBox(this);
    m_countryFilter->addItem("All Countries");
    m_countryFilter->addItem("US", "US");
    m_countryFilter->addItem("Russia", "RU");
    m_countryFilter->addItem("China", "CN");
    m_countryFilter->addItem("UK", "UK");
    m_countryFilter->addItem("Germany", "DE");
    m_countryFilter->addItem("France", "FR");
    m_countryFilter->addItem("Brazil", "BR");
    m_countryFilter->addItem("Japan", "JP");
    m_countryFilter->addItem("South Korea", "KR");
    m_countryFilter->addItem("Israel", "IL");
    m_countryFilter->addItem("Turkey", "TR");
    m_countryFilter->addItem("India", "IN");
    filterLayout->addWidget(m_countryFilter);
    
    m_categoryFilter = new QComboBox(this);
    m_categoryFilter->addItem("All Categories");
    m_categoryFilter->addItem("Tanks");
    m_categoryFilter->addItem("IFVs");
    m_categoryFilter->addItem("APCs");
    m_categoryFilter->addItem("Artillery");
    m_categoryFilter->addItem("MLRS");
    m_categoryFilter->addItem("SAM");
    m_categoryFilter->addItem("ATGM");
    m_categoryFilter->addItem("MANPADS");
    m_categoryFilter->addItem("Aircraft");
    m_categoryFilter->addItem("Helicopters");
    m_categoryFilter->addItem("Bombers");
    m_categoryFilter->addItem("Ships");
    m_categoryFilter->addItem("Submarines");
    m_categoryFilter->addItem("UAVs");
    filterLayout->addWidget(m_categoryFilter);
    
    layout->addLayout(filterLayout);
    
    // Tree view
    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(false);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setDragEnabled(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragOnly);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_treeView, 1);
    
    // Count label
    m_countLabel = new QLabel("Platforms: 0", this);
    m_countLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_countLabel);
    
    // Create model
    m_model = new PlatformModel(this);
    
    // Create proxy model for filtering
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setRecursiveFilteringEnabled(true);
    m_proxyModel->setFilterKeyColumn(0);  // Filter by name
    
    m_treeView->setModel(m_proxyModel);
    
    // Resize columns
    m_treeView->setColumnWidth(0, 200);
    m_treeView->setColumnWidth(1, 50);
    m_treeView->setColumnWidth(2, 100);
}

void PlatformBrowser::setupConnections()
{
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &PlatformBrowser::onSearchTextChanged);
    
    connect(m_countryFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlatformBrowser::onFilterChanged);
    
    connect(m_categoryFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlatformBrowser::onFilterChanged);
    
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &PlatformBrowser::onSelectionChanged);
}

void PlatformBrowser::updateCountLabel()
{
    const int count = platformCount();
    m_countLabel->setText(QString("Platforms: %1").arg(count));
}

} // namespace athena::ui
