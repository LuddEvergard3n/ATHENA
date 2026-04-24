/**
 * @file forcetree.cpp
 * @brief Force composition tree implementation
 */

#include "forcetree.hpp"
#include "models/forcemodel.hpp"

#include <QVBoxLayout>
#include <QTreeView>
#include <QHeaderView>
#include <QDebug>

namespace athena::ui {

ForceTree::ForceTree(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

ForceTree::~ForceTree() = default;

void ForceTree::clear()
{
    if (m_model) {
        m_model->clear();
    }
}

int ForceTree::bluforCount() const
{
    return m_model ? m_model->bluforCount() : 0;
}

int ForceTree::opforCount() const
{
    return m_model ? m_model->opforCount() : 0;
}

std::vector<ForceUnit> ForceTree::getUnits() const
{
    return m_model ? m_model->getUnits() : std::vector<ForceUnit>{};
}

bool ForceTree::isValidForSimulation() const
{
    return m_model ? m_model->isValidForSimulation() : false;
}

void ForceTree::loadUnits(const std::vector<ForceUnit>& units)
{
    if (m_model) {
        m_model->loadUnits(units);
        m_treeView->expandAll();
    }
}

void ForceTree::addUnit(const ForceUnit& unit)
{
    if (m_model) {
        m_model->addUnit(unit);
        m_treeView->expandAll();
    }
}

void ForceTree::removeUnit(const QString& platformId, bool blufor)
{
    if (m_model) {
        m_model->removeUnitByPlatform(platformId, blufor);
    }
}

void ForceTree::updateUnitQuantity(const QString& platformId, bool blufor, int quantity)
{
    if (m_model) {
        m_model->updateUnitQuantity(platformId, blufor, quantity);
    }
}

void ForceTree::onPlatformSelected(const QString& platformId)
{
    qDebug() << "Platform selected in browser:" << platformId;
    // TODO: Enable add to force buttons
}

void ForceTree::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    
    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(false);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragDrop);
    m_treeView->setAcceptDrops(true);
    m_treeView->header()->setStretchLastSection(true);
    layout->addWidget(m_treeView);
    
    m_model = new ForceModel(this);
    m_treeView->setModel(m_model);
    
    // Forward signal for Tree → Map sync
    connect(m_model, &ForceModel::unitAddedFromUI,
            this, &ForceTree::unitAddedFromUI);
}

} // namespace athena::ui
