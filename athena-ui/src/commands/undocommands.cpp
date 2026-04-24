/**
 * @file undocommands.cpp
 * @brief Undo/Redo command implementations
 * 
 * v0.7.0: Initial implementation
 */

#include "undocommands.hpp"
#include "graphics/mapscene.hpp"
#include "graphics/unititem.hpp"

namespace athena::ui {

// ============================================================================
// AddUnitCommand
// ============================================================================

AddUnitCommand::AddUnitCommand(MapScene* scene, ForceModel* model, const ForceUnit& unit,
                               QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_model(model)
    , m_unit(unit)
{
    setText(QString("Add %1").arg(unit.displayName));
}

void AddUnitCommand::undo()
{
    if (m_scene) {
        m_scene->removeUnit(m_unit.id);
    }
    if (m_model) {
        m_model->removeUnit(m_unit.id);
    }
}

void AddUnitCommand::redo()
{
    if (m_firstRedo) {
        // First redo is the initial action, already done
        m_firstRedo = false;
        return;
    }
    
    if (m_model) {
        m_model->addUnit(m_unit);
    }
    if (m_scene) {
        m_scene->addUnitFromTree(m_unit);
    }
}

// ============================================================================
// RemoveUnitCommand
// ============================================================================

RemoveUnitCommand::RemoveUnitCommand(MapScene* scene, ForceModel* model, const QString& unitId,
                                     QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_model(model)
    , m_unitId(unitId)
{
    // Save unit data before removal
    if (m_model) {
        auto units = m_model->getUnits();
        for (const auto& u : units) {
            if (u.id == unitId) {
                m_savedUnit = u;
                break;
            }
        }
    }
    setText(QString("Remove %1").arg(m_savedUnit.displayName));
}

void RemoveUnitCommand::undo()
{
    if (m_model) {
        m_model->addUnit(m_savedUnit);
    }
    if (m_scene) {
        m_scene->addUnitFromTree(m_savedUnit);
    }
}

void RemoveUnitCommand::redo()
{
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    
    if (m_scene) {
        m_scene->removeUnit(m_unitId);
    }
    if (m_model) {
        m_model->removeUnit(m_unitId);
    }
}

// ============================================================================
// MoveUnitCommand
// ============================================================================

MoveUnitCommand::MoveUnitCommand(MapScene* scene, const QString& unitId,
                                 double oldLat, double oldLon,
                                 double newLat, double newLon,
                                 QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_unitId(unitId)
    , m_oldLat(oldLat)
    , m_oldLon(oldLon)
    , m_newLat(newLat)
    , m_newLon(newLon)
{
    setText("Move Unit");
}

void MoveUnitCommand::undo()
{
    if (!m_scene) return;
    
    UnitItem* item = m_scene->findUnit(m_unitId);
    if (item) {
        item->setGeoPosition(m_oldLat, m_oldLon);
        QPointF scenePos = m_scene->geoToScene(m_oldLat, m_oldLon);
        item->setPos(scenePos);
    }
}

void MoveUnitCommand::redo()
{
    if (!m_scene) return;
    
    UnitItem* item = m_scene->findUnit(m_unitId);
    if (item) {
        item->setGeoPosition(m_newLat, m_newLon);
        QPointF scenePos = m_scene->geoToScene(m_newLat, m_newLon);
        item->setPos(scenePos);
    }
}

bool MoveUnitCommand::mergeWith(const QUndoCommand* other)
{
    if (other->id() != id()) return false;
    
    const MoveUnitCommand* cmd = static_cast<const MoveUnitCommand*>(other);
    if (cmd->m_unitId != m_unitId) return false;
    
    // Merge: keep original start, update end
    m_newLat = cmd->m_newLat;
    m_newLon = cmd->m_newLon;
    return true;
}

// ============================================================================
// ChangeSideCommand
// ============================================================================

ChangeSideCommand::ChangeSideCommand(MapScene* scene, ForceModel* model, const QString& unitId,
                                     QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_model(model)
    , m_unitId(unitId)
    , m_wasBlufor(true)
{
    // Get current side
    if (m_scene) {
        UnitItem* item = m_scene->findUnit(unitId);
        if (item) {
            m_wasBlufor = (item->side() == UnitItem::Side::Blue);
        }
    }
    setText("Change Side");
}

void ChangeSideCommand::undo()
{
    // Restore original side
    if (m_scene) {
        UnitItem* item = m_scene->findUnit(m_unitId);
        if (item) {
            item->setSide(m_wasBlufor ? UnitItem::Side::Blue : UnitItem::Side::Red);
        }
    }
    if (m_model) {
        m_model->setUnitSide(m_unitId, m_wasBlufor);
    }
}

void ChangeSideCommand::redo()
{
    // Toggle side
    if (m_scene) {
        UnitItem* item = m_scene->findUnit(m_unitId);
        if (item) {
            item->setSide(m_wasBlufor ? UnitItem::Side::Red : UnitItem::Side::Blue);
        }
    }
    if (m_model) {
        m_model->setUnitSide(m_unitId, !m_wasBlufor);
    }
}

// ============================================================================
// ChangeQuantityCommand
// ============================================================================

ChangeQuantityCommand::ChangeQuantityCommand(MapScene* scene, ForceModel* model, 
                                             const QString& unitId,
                                             int oldQty, int newQty,
                                             QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_model(model)
    , m_unitId(unitId)
    , m_oldQty(oldQty)
    , m_newQty(newQty)
{
    setText(QString("Change Quantity (%1 -> %2)").arg(oldQty).arg(newQty));
}

void ChangeQuantityCommand::undo()
{
    if (m_scene) {
        UnitItem* item = m_scene->findUnit(m_unitId);
        if (item) {
            item->setQuantity(m_oldQty);
        }
    }
    if (m_model) {
        m_model->setUnitQuantity(m_unitId, m_oldQty);
    }
}

void ChangeQuantityCommand::redo()
{
    if (m_scene) {
        UnitItem* item = m_scene->findUnit(m_unitId);
        if (item) {
            item->setQuantity(m_newQty);
        }
    }
    if (m_model) {
        m_model->setUnitQuantity(m_unitId, m_newQty);
    }
}

bool ChangeQuantityCommand::mergeWith(const QUndoCommand* other)
{
    if (other->id() != id()) return false;
    
    const ChangeQuantityCommand* cmd = static_cast<const ChangeQuantityCommand*>(other);
    if (cmd->m_unitId != m_unitId) return false;
    
    m_newQty = cmd->m_newQty;
    setText(QString("Change Quantity (%1 -> %2)").arg(m_oldQty).arg(m_newQty));
    return true;
}

// ============================================================================
// DuplicateUnitsCommand
// ============================================================================

DuplicateUnitsCommand::DuplicateUnitsCommand(MapScene* scene, ForceModel* model,
                                             const QList<QString>& unitIds,
                                             QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_model(model)
    , m_sourceIds(unitIds)
{
    setText(QString("Duplicate %1 unit(s)").arg(unitIds.size()));
}

void DuplicateUnitsCommand::undo()
{
    // Remove duplicated units
    for (const auto& unit : m_createdUnits) {
        if (m_scene) {
            m_scene->removeUnit(unit.id);
        }
        if (m_model) {
            m_model->removeUnit(unit.id);
        }
    }
}

void DuplicateUnitsCommand::redo()
{
    if (m_firstRedo) {
        m_firstRedo = false;
        // On first redo, the scene already performed the duplication
        // We need to capture what was created
        // This is handled by the caller setting m_createdUnits after creation
        return;
    }
    
    // Re-add the duplicated units
    for (const auto& unit : m_createdUnits) {
        if (m_model) {
            m_model->addUnit(unit);
        }
        if (m_scene) {
            m_scene->addUnitFromTree(unit);
        }
    }
}

} // namespace athena::ui
