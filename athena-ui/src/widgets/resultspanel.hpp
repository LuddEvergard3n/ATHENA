/**
 * @file resultspanel.hpp
 * @brief Results dashboard panel widget
 * 
 * v0.6.5: Display SimulationResults with summary cards
 * v0.7.1: Added loss distribution histogram tab
 */

#ifndef ATHENA_UI_RESULTSPANEL_HPP
#define ATHENA_UI_RESULTSPANEL_HPP

#include <QWidget>
#include "simulation/simulationrunner.hpp"

class QTabWidget;
class QTableView;
class QLabel;
class QProgressBar;
class QFrame;

namespace athena::ui {

class LossHistogram;

/**
 * @brief Panel for displaying simulation results and analysis
 */
class ResultsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ResultsPanel(QWidget* parent = nullptr);
    ~ResultsPanel() override;

    /**
     * @brief Clear all displayed results
     */
    void clear();

    /**
     * @brief Load results from file
     */
    void loadResults(const QString& filePath);

public slots:
    /**
     * @brief Display simulation results
     */
    void showResults(const SimulationResults& results);

private:
    void setupUi();
    void setupOverviewTab();
    void setupDistributionTab();
    QFrame* createStatCard(const QString& title, const QString& value, 
                           const QString& subtitle = QString());
    void updateStatCard(QFrame* card, const QString& value, const QString& subtitle = QString());

    QTabWidget* m_tabWidget = nullptr;
    
    // Overview tab widgets
    QWidget* m_overviewTab = nullptr;
    QFrame* m_winRateCard = nullptr;
    QFrame* m_iterationsCard = nullptr;
    QFrame* m_bluforCasCard = nullptr;
    QFrame* m_opforCasCard = nullptr;
    QFrame* m_runtimeCard = nullptr;
    
    // Distribution tab
    QWidget* m_distributionTab = nullptr;
    LossHistogram* m_lossHistogram = nullptr;
    
    QLabel* m_noResultsLabel = nullptr;
    
    // Store last results
    SimulationResults m_lastResults;
    bool m_hasResults = false;
};

} // namespace athena::ui

#endif // ATHENA_UI_RESULTSPANEL_HPP
