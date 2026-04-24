/**
 * @file mainwindow.hpp
 * @brief Main application window for ATHENA
 * 
 * Central hub containing:
 * - Menu and toolbars
 * - Tab widget (Scenario Editor, Simulation Control, Results Dashboard)
 * - Dock widgets (Platform Browser, Force Tree, Properties)
 * - Status bar with simulation state
 */

#ifndef ATHENA_UI_MAINWINDOW_HPP
#define ATHENA_UI_MAINWINDOW_HPP

#include <QMainWindow>
#include <QString>
#include <memory>

// Forward declarations - Qt
class QTabWidget;
class QDockWidget;
class QStatusBar;
class QProgressBar;
class QLabel;
class QAction;
class QMenu;
class QToolBar;
class QUndoStack;

namespace athena::ui {

// Forward declarations - ATHENA widgets
class MapView;
class PlatformBrowser;
class ForceTree;
class SimulationPanel;
class ResultsPanel;
class SimulationRunner;
class PlatformDetailsPanel;
class RangeOverlay;
class MovementManager;
class MiniMap;
struct SimulationConfig;
struct SimulationResults;

/**
 * @brief Main application window
 * 
 * Layout:
 * ┌─────────────────────────────────────────────────────────────┐
 * │  File  Edit  View  Scenario  Simulation  Analysis  Help     │
 * ├─────────────────────────────────────────────────────────────┤
 * │  [New] [Open] [Save] │ [Run] [Pause] [Stop] │ [Zoom] [Pan]  │
 * ├───────────────────────┬─────────────────────────────────────┤
 * │                       │                                     │
 * │   Platform Browser    │   [Scenario] [Simulation] [Results] │
 * │   ─────────────────   │                                     │
 * │   Force Tree          │         Central Widget              │
 * │   ─────────────────   │     (Map / Controls / Charts)       │
 * │   Properties          │                                     │
 * │                       │                                     │
 * ├───────────────────────┴─────────────────────────────────────┤
 * │  Ready │ Platforms: 453 │ BLUFOR: 0 │ OPFOR: 0 │ Progress   │
 * └─────────────────────────────────────────────────────────────┘
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Disable copy
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /**
     * @brief Load scenario from file
     * @param filePath Path to .athena-scenario file
     * @return true if loaded successfully
     */
    bool loadScenario(const QString& filePath);

    /**
     * @brief Save current scenario to file
     * @param filePath Path to save to
     * @return true if saved successfully
     */
    bool saveScenario(const QString& filePath);

    /**
     * @brief Set path to platform database
     * @param path Directory containing platform JSON files
     */
    void setDataPath(const QString& path);

    /**
     * @brief Load platform database from path
     * @param path Directory containing platform JSON files
     */
    void loadPlatformDatabase(const QString& path);
    
    /**
     * @brief Get the undo stack for command registration
     */
    QUndoStack* undoStack() { return m_undoStack; }

signals:
    /**
     * @brief Emitted when scenario is loaded
     * @param name Scenario name
     */
    void scenarioLoaded(const QString& name);

    /**
     * @brief Emitted when simulation state changes
     * @param running true if simulation is running
     */
    void simulationStateChanged(bool running);

public slots:
    // File operations
    void newScenario();
    void openScenario();
    void saveScenarioAs();
    
    // Simulation control
    void runSimulation();
    void runSimulationWithConfig(const SimulationConfig& config);
    void pauseSimulation();
    void stopSimulation();
    void exportResults();
    void exportMapToPng();
    void exportMapToPdf();
    
    // View operations
    void resetView();
    void zoomIn();
    void zoomOut();
    void toggleFullscreen();
    
    // Formation operations
    void applyFormation(int formationType);
    void showFormationDialog();
    
private:
    void applyFormationWithParams(int formationType, double heading, double spacing);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    // Internal slots
    void onTabChanged(int index);
    void onSimulationProgress(int current, int total);
    void onSimulationComplete();
    void updateStatusBar();
    void updateWindowTitle();
    void openRecentFile();
    void clearRecentFiles();

private:
    // Setup methods
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void generateEngagementVisualization();
    
    // Recent Files
    void updateRecentFilesMenu();
    void addToRecentFiles(const QString& filePath);

    // Central widget components
    QTabWidget* m_tabWidget = nullptr;
    MapView* m_mapView = nullptr;
    SimulationPanel* m_simulationPanel = nullptr;
    ResultsPanel* m_resultsPanel = nullptr;

    // Dock widgets
    QDockWidget* m_platformDock = nullptr;
    QDockWidget* m_forceDock = nullptr;
    QDockWidget* m_propertiesDock = nullptr;
    
    PlatformBrowser* m_platformBrowser = nullptr;
    ForceTree* m_forceTree = nullptr;

    // Status bar components
    QLabel* m_statusLabel = nullptr;
    QLabel* m_platformCountLabel = nullptr;
    QLabel* m_bluforCountLabel = nullptr;
    QLabel* m_opforCountLabel = nullptr;
    QLabel* m_coordLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // Menus
    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_scenarioMenu = nullptr;
    QMenu* m_simulationMenu = nullptr;
    QMenu* m_analysisMenu = nullptr;
    QMenu* m_helpMenu = nullptr;

    // Toolbars
    QToolBar* m_fileToolBar = nullptr;
    QToolBar* m_simulationToolBar = nullptr;
    QToolBar* m_viewToolBar = nullptr;

    // Actions - File
    QAction* m_newAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_exportMapPngAction = nullptr;
    QAction* m_exportMapPdfAction = nullptr;
    QAction* m_exitAction = nullptr;
    
    // Recent Files
    static constexpr int MAX_RECENT_FILES = 5;
    QMenu* m_recentFilesMenu = nullptr;
    QAction* m_recentFileActions[MAX_RECENT_FILES] = {};
    QAction* m_clearRecentAction = nullptr;

    // Actions - Edit
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_cutAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_pasteAction = nullptr;
    QAction* m_deleteAction = nullptr;

    // Actions - View
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_zoomFitAction = nullptr;
    QAction* m_fullscreenAction = nullptr;
    QAction* m_showGridAction = nullptr;
    QAction* m_showTerrainAction = nullptr;
    QAction* m_generateTerrainAction = nullptr;
    QAction* m_measureAction = nullptr;
    QAction* m_showRangesAction = nullptr;
    QAction* m_movementOrdersAction = nullptr;
    QAction* m_exportResultsAction = nullptr;
    QAction* m_showMiniMapAction = nullptr;
    QAction* m_showEngagementsAction = nullptr;

    // Actions - Formation
    QMenu* m_formationMenu = nullptr;
    QAction* m_formationLineAction = nullptr;
    QAction* m_formationColumnAction = nullptr;
    QAction* m_formationWedgeAction = nullptr;
    QAction* m_formationVeeAction = nullptr;
    QAction* m_formationEchelonAction = nullptr;
    QAction* m_formationBoxAction = nullptr;
    QAction* m_formationCircleAction = nullptr;
    QAction* m_formationCustomAction = nullptr;

    // Actions - Simulation
    QAction* m_runAction = nullptr;
    QAction* m_pauseAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_settingsAction = nullptr;

    // Simulation
    SimulationRunner* m_simulationRunner = nullptr;
    PlatformDetailsPanel* m_detailsPanel = nullptr;
    RangeOverlay* m_rangeOverlay = nullptr;
    MovementManager* m_movementManager = nullptr;
    MiniMap* m_miniMap = nullptr;
    
    // Undo/Redo
    QUndoStack* m_undoStack = nullptr;

    // State
    QString m_currentScenarioPath;
    QString m_dataPath;
    bool m_simulationRunning = false;
    bool m_scenarioModified = false;

    // Constants
    static constexpr int DEFAULT_WIDTH = 1400;
    static constexpr int DEFAULT_HEIGHT = 900;
};

} // namespace athena::ui

#endif // ATHENA_UI_MAINWINDOW_HPP
