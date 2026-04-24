/**
 * @file undocommands.hpp
 * @brief Undo/Redo commands for scenario editing
 * 
 * v0.7.0: Initial implementation
 */

#ifndef ATHENA_UI_UNDOCOMMANDS_HPP
#define ATHENA_UI_UNDOCOMMANDS_HPP

#include <QUndoCommand>
#include <QString>
#include <QPointF>
#include "models/forcemodel.hpp"

namespace athena::ui {

class MapScene;
class ForceModel;

/**
 * @brief Command for adding a unit
 */
class AddUnitCommand : public QUndoCommand
{
public:
    AddUnitCommand(MapScene* scene, ForceModel* model, const ForceUnit& unit,
                   QUndoCommand* parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    MapScene* m_scene;
    ForceModel* m_model;
    ForceUnit m_unit;
    bool m_firstRedo = true;
};

/**
 * @brief Command for removing a unit
 */
class RemoveUnitCommand : public QUndoCommand
{
public:
    RemoveUnitCommand(MapScene* scene, ForceModel* model, const QString& unitId,
                      QUndoCommand* parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    MapScene* m_scene;
    ForceModel* m_model;
    QString m_unitId;
    ForceUnit m_savedUnit;
    bool m_firstRedo = true;
};

/**
 * @brief Command for moving a unit
 */
class MoveUnitCommand : public QUndoCommand
{
public:
    MoveUnitCommand(MapScene* scene, const QString& unitId,
                    double oldLat, double oldLon,
                    double newLat, double newLon,
                    QUndoCommand* parent = nullptr);
    
    void undo() override;
    void redo() override;
    
    int id() const override { return 1001; }
    bool mergeWith(const QUndoCommand* other) override;
    
private:
    MapScene* m_scene;
    QString m_unitId;
    double m_oldLat, m_oldLon;
    double m_newLat, m_newLon;
};

/**
 * @brief Command for changing unit side
 */
class ChangeSideCommand : public QUndoCommand
{
public:
    ChangeSideCommand(MapScene* scene, ForceModel* model, const QString& unitId,
                      QUndoCommand* parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    MapScene* m_scene;
    ForceModel* m_model;
    QString m_unitId;
    bool m_wasBlufor;
};

/**
 * @brief Command for changing unit quantity
 */
class ChangeQuantityCommand : public QUndoCommand
{
public:
    ChangeQuantityCommand(MapScene* scene, ForceModel* model, const QString& unitId,
                          int oldQty, int newQty,
                          QUndoCommand* parent = nullptr);
    
    void undo() override;
    void redo() override;
    
    int id() const override { return 1002; }
    bool mergeWith(const QUndoCommand* other) override;
    
private:
    MapScene* m_scene;
    ForceModel* m_model;
    QString m_unitId;
    int m_oldQty;
    int m_newQty;
};

/**
 * @brief Command for duplicating units
 */
class DuplicateUnitsCommand : public QUndoCommand
{
public:
    DuplicateUnitsCommand(MapScene* scene, ForceModel* model, 
                          const QList<QString>& unitIds,
                          QUndoCommand* parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    MapScene* m_scene;
    ForceModel* m_model;
    QList<QString> m_sourceIds;
    QList<ForceUnit> m_createdUnits;
    bool m_firstRedo = true;
};

} // namespace athena::ui

#endif // ATHENA_UI_UNDOCOMMANDS_HPP
