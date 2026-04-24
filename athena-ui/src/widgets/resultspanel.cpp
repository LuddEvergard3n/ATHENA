/**
 * @file resultspanel.cpp
 * @brief Results dashboard implementation
 * 
 * v0.6.5: Visual stat cards for simulation results
 * v0.7.1: Added loss distribution histogram
 */

#include "resultspanel.hpp"
#include "losshistogram.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QLabel>
#include <QFrame>
#include <QTableView>
#include <QHeaderView>
#include <QFont>
#include <QDebug>

namespace athena::ui {

ResultsPanel::ResultsPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

ResultsPanel::~ResultsPanel() = default;

void ResultsPanel::clear()
{
    m_hasResults = false;
    m_lastResults = SimulationResults{};
    
    // Show "no results" message
    m_noResultsLabel->setVisible(true);
    
    // Reset cards to default values
    updateStatCard(m_winRateCard, "--", "No simulation run");
    updateStatCard(m_iterationsCard, "--", "");
    updateStatCard(m_bluforCasCard, "--", "");
    updateStatCard(m_opforCasCard, "--", "");
    updateStatCard(m_runtimeCard, "--", "");
    
    // Clear histogram
    if (m_lossHistogram) {
        m_lossHistogram->clear();
    }
}

void ResultsPanel::loadResults(const QString& filePath)
{
    // TODO: Load results from JSON/binary file
    qDebug() << "ResultsPanel::loadResults" << filePath;
}

void ResultsPanel::showResults(const SimulationResults& results)
{
    m_lastResults = results;
    m_hasResults = true;
    m_noResultsLabel->setVisible(false);
    
    // Update win rate card
    if (results.completed && !results.cancelled) {
        const double winPct = results.bluforWinRate * 100;
        const double ciHalf = (results.bluforWinRateHigh - results.bluforWinRateLow) * 50;
        updateStatCard(m_winRateCard, 
                      QString("%1%").arg(winPct, 0, 'f', 1),
                      QString("± %1% (95% CI)").arg(ciHalf, 0, 'f', 1));
    } else if (results.cancelled) {
        updateStatCard(m_winRateCard, "N/A", "Simulation cancelled");
    } else {
        updateStatCard(m_winRateCard, "Error", results.errorMessage);
    }
    
    // Update iterations card
    updateStatCard(m_iterationsCard,
                  QString::number(results.completedRuns),
                  QString("of %1 planned").arg(results.totalRuns));
    
    // Update casualties cards
    if (results.completed && !results.cancelled) {
        updateStatCard(m_bluforCasCard,
                      QString("%1%").arg(results.avgBluforCasualties * 100, 0, 'f', 1),
                      "average losses");
        updateStatCard(m_opforCasCard,
                      QString("%1%").arg(results.avgOpforCasualties * 100, 0, 'f', 1),
                      "average losses");
    } else {
        updateStatCard(m_bluforCasCard, "--", "");
        updateStatCard(m_opforCasCard, "--", "");
    }
    
    // Update runtime card
    if (results.totalRuntimeSeconds >= 60) {
        const int min = static_cast<int>(results.totalRuntimeSeconds) / 60;
        const int sec = static_cast<int>(results.totalRuntimeSeconds) % 60;
        updateStatCard(m_runtimeCard,
                      QString("%1m %2s").arg(min).arg(sec),
                      QString("%1 it/s").arg(results.completedRuns / results.totalRuntimeSeconds, 0, 'f', 0));
    } else {
        updateStatCard(m_runtimeCard,
                      QString("%1s").arg(results.totalRuntimeSeconds, 0, 'f', 1),
                      QString("%1 it/s").arg(results.completedRuns / std::max(0.001, results.totalRuntimeSeconds), 0, 'f', 0));
    }
    
    // Update histogram
    if (m_lossHistogram && results.completed && !results.cancelled) {
        QVector<double> bluforLosses;
        QVector<double> opforLosses;
        
        for (const auto& iter : results.iterations) {
            double bluforPct = results.initialBlufor > 0 
                ? (iter.bluforLosses * 100.0 / results.initialBlufor) 
                : 0.0;
            double opforPct = results.initialOpfor > 0 
                ? (iter.opforLosses * 100.0 / results.initialOpfor) 
                : 0.0;
            
            bluforLosses.append(bluforPct);
            opforLosses.append(opforPct);
        }
        
        m_lossHistogram->setData(bluforLosses, opforLosses);
    }
    
    qDebug() << "ResultsPanel: Showing results for" << results.completedRuns << "iterations";
}

void ResultsPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);
    
    // Create tabs
    setupOverviewTab();
    setupDistributionTab();
    
    // Placeholder tabs for future features
    auto* sensitivityTab = new QWidget();
    m_tabWidget->addTab(sensitivityTab, "Sensitivity");
}

void ResultsPanel::setupOverviewTab()
{
    m_overviewTab = new QWidget();
    auto* layout = new QVBoxLayout(m_overviewTab);
    layout->setSpacing(16);
    layout->setContentsMargins(16, 16, 16, 16);
    
    // Title
    auto* titleLabel = new QLabel("Simulation Results", m_overviewTab);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    
    // No results message (initially visible)
    m_noResultsLabel = new QLabel("No simulation results to display.\n\n"
                                   "Run a simulation from the Simulation tab to see results here.",
                                   m_overviewTab);
    m_noResultsLabel->setAlignment(Qt::AlignCenter);
    m_noResultsLabel->setStyleSheet("QLabel { color: #888; font-size: 14px; padding: 40px; }");
    layout->addWidget(m_noResultsLabel);
    
    // Stats grid
    auto* statsGrid = new QGridLayout();
    statsGrid->setSpacing(12);
    
    // Create stat cards
    m_winRateCard = createStatCard("BLUFOR Win Rate", "--", "No simulation run");
    statsGrid->addWidget(m_winRateCard, 0, 0);
    
    m_iterationsCard = createStatCard("Iterations", "--", "");
    statsGrid->addWidget(m_iterationsCard, 0, 1);
    
    m_bluforCasCard = createStatCard("BLUFOR Casualties", "--", "");
    statsGrid->addWidget(m_bluforCasCard, 1, 0);
    
    m_opforCasCard = createStatCard("OPFOR Casualties", "--", "");
    statsGrid->addWidget(m_opforCasCard, 1, 1);
    
    m_runtimeCard = createStatCard("Runtime", "--", "");
    statsGrid->addWidget(m_runtimeCard, 2, 0);
    
    layout->addLayout(statsGrid);
    layout->addStretch();
    
    m_tabWidget->addTab(m_overviewTab, "Overview");
}

void ResultsPanel::setupDistributionTab()
{
    m_distributionTab = new QWidget();
    auto* layout = new QVBoxLayout(m_distributionTab);
    layout->setContentsMargins(16, 16, 16, 16);
    
    m_lossHistogram = new LossHistogram(m_distributionTab);
    layout->addWidget(m_lossHistogram);
    
    m_tabWidget->addTab(m_distributionTab, "Distribution");
}

QFrame* ResultsPanel::createStatCard(const QString& title, const QString& value, 
                                      const QString& subtitle)
{
    auto* card = new QFrame();
    card->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    card->setStyleSheet(
        "QFrame {"
        "  background-color: #f5f5f5;"
        "  border: 1px solid #ddd;"
        "  border-radius: 8px;"
        "  padding: 12px;"
        "}"
    );
    card->setMinimumSize(180, 100);
    
    auto* layout = new QVBoxLayout(card);
    layout->setSpacing(4);
    
    // Title
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; font-weight: bold; }");
    titleLabel->setObjectName("cardTitle");
    layout->addWidget(titleLabel);
    
    // Value
    auto* valueLabel = new QLabel(value, card);
    QFont valueFont = valueLabel->font();
    valueFont.setPointSize(24);
    valueFont.setBold(true);
    valueLabel->setFont(valueFont);
    valueLabel->setStyleSheet("QLabel { color: #333; }");
    valueLabel->setObjectName("cardValue");
    layout->addWidget(valueLabel);
    
    // Subtitle
    auto* subtitleLabel = new QLabel(subtitle, card);
    subtitleLabel->setStyleSheet("QLabel { color: #888; font-size: 10px; }");
    subtitleLabel->setObjectName("cardSubtitle");
    layout->addWidget(subtitleLabel);
    
    layout->addStretch();
    
    return card;
}

void ResultsPanel::updateStatCard(QFrame* card, const QString& value, const QString& subtitle)
{
    if (!card) return;
    
    auto* valueLabel = card->findChild<QLabel*>("cardValue");
    if (valueLabel) {
        valueLabel->setText(value);
    }
    
    auto* subtitleLabel = card->findChild<QLabel*>("cardSubtitle");
    if (subtitleLabel) {
        subtitleLabel->setText(subtitle);
    }
}

} // namespace athena::ui
