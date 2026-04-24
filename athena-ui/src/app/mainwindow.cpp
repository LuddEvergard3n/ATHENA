/**
 * @file mainwindow.cpp
 * @brief Main application window implementation
 */

#include "mainwindow.hpp"

// Qt includes
#include <QApplication>
#include <QCoreApplication>
#include <QTabWidget>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QProgressBar>
#include <QLabel>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QDateTime>
#include <QDebug>
#include <QUndoStack>
#include <QPainter>
#include <QPixmap>
#include <QPdfWriter>

// ATHENA widgets
#include "widgets/mapview.hpp"
#include "widgets/platformbrowser.hpp"
#include "widgets/forcetree.hpp"
#include "widgets/simulationpanel.hpp"
#include "widgets/resultspanel.hpp"
#include "widgets/platformdetails.hpp"
#include "widgets/minimap.hpp"
#include "widgets/formationdialog.hpp"
#include "simulation/simulationrunner.hpp"
#include "simulation/scenariobuilder.hpp"
#include "simulation/scenariofile.hpp"
#include "simulation/resultsexporter.hpp"
#include "graphics/mapscene.hpp"
#include "graphics/rangeoverlay.hpp"
#include "graphics/unititem.hpp"
#include "graphics/engagementlines.hpp"
#include "graphics/movementpath.hpp"
#include "graphics/unititem.hpp"
#include "models/platformmodel.hpp"
#include "commands/undocommands.hpp"
#include "utils/formationtemplates.hpp"

// ATHENA Core
#include <athena/scenario.hpp>

namespace athena::ui {

// =============================================================================
// Construction / Destruction
// =============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    loadSettings();
    
    // Auto-load platform database
    if (!m_dataPath.isEmpty()) {
        loadPlatformDatabase(m_dataPath);
    }
    
    updateStatusBar();
    
    qDebug() << "MainWindow initialized";
}

MainWindow::~MainWindow()
{
    saveSettings();
}

// =============================================================================
// Public Methods
// =============================================================================

bool MainWindow::loadScenario(const QString& filePath)
{
    ScenarioFile file;
    ScenarioFileData data;
    
    if (!file.load(filePath, data)) {
        QMessageBox::critical(this, "Load Error",
                             QString("Failed to load scenario:\n%1").arg(file.lastError()));
        return false;
    }
    
    // Load units into force tree
    if (m_forceTree) {
        m_forceTree->loadUnits(data.units);
    }
    
    // Load units into map view
    if (m_mapView) {
        m_mapView->setBounds(data.minLat, data.maxLat, data.minLon, data.maxLon);
        m_mapView->loadUnits(data.units);
        m_mapView->resetView();
    }
    
    // Update state
    m_currentScenarioPath = filePath;
    m_scenarioModified = false;
    
    // Add to recent files
    addToRecentFiles(filePath);
    
    // Update window title
    QFileInfo fileInfo(filePath);
    const QString displayName = data.metadata.name.isEmpty() 
        ? fileInfo.baseName() 
        : data.metadata.name;
    setWindowTitle(QString("ATHENA - %1").arg(displayName));
    
    emit scenarioLoaded(displayName);
    updateStatusBar();
    
    qDebug() << "Loaded scenario:" << filePath
             << "| BLUFOR:" << m_forceTree->bluforCount()
             << "| OPFOR:" << m_forceTree->opforCount();
    
    return true;
}

bool MainWindow::saveScenario(const QString& filePath)
{
    if (!m_forceTree) {
        return false;
    }
    
    // Build file data
    ScenarioFileData data;
    
    // Metadata
    QFileInfo fileInfo(filePath);
    data.metadata.name = fileInfo.baseName();
    data.metadata.author = qgetenv("USER");
    data.metadata.createdDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Units from force tree
    data.units = m_forceTree->getUnits();
    
    // Simulation defaults
    data.maxTicks = 3600;
    data.defaultIterations = 1000;
    
    // Save
    ScenarioFile file;
    if (!file.save(filePath, data)) {
        QMessageBox::critical(this, "Save Error",
                             QString("Failed to save scenario:\n%1").arg(file.lastError()));
        return false;
    }
    
    // Update state
    m_currentScenarioPath = filePath;
    m_scenarioModified = false;
    
    setWindowTitle(QString("ATHENA - %1").arg(data.metadata.name));
    updateStatusBar();
    
    qDebug() << "Saved scenario:" << filePath;
    
    return true;
}

void MainWindow::setDataPath(const QString& path)
{
    m_dataPath = path;
    loadPlatformDatabase(path);
    updateStatusBar();
}

void MainWindow::loadPlatformDatabase(const QString& path)
{
    if (m_platformBrowser) {
        const int count = m_platformBrowser->loadDatabase(path);
        qDebug() << "Loaded" << count << "platforms from" << path;
        
        // Update window title with platform count
        setWindowTitle(QString("ATHENA - %1 Platforms").arg(count));
    }
    
    // Configure details panel with same data path
    if (m_detailsPanel) {
        m_detailsPanel->setDataPath(path);
    }
}

// =============================================================================
// Public Slots - File Operations
// =============================================================================

void MainWindow::newScenario()
{
    // Check for unsaved changes
    if (m_scenarioModified) {
        const auto result = QMessageBox::question(
            this,
            "Unsaved Changes",
            "Current scenario has unsaved changes. Save before creating new?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
        );
        
        if (result == QMessageBox::Save) {
            saveScenarioAs();
        } else if (result == QMessageBox::Cancel) {
            return;
        }
    }
    
    // Reset state
    m_currentScenarioPath.clear();
    m_scenarioModified = false;
    setWindowTitle("ATHENA - New Scenario");
    
    // Clear editors
    if (m_forceTree) {
        m_forceTree->clear();
    }
    if (m_mapView) {
        m_mapView->clear();
    }
    
    updateStatusBar();
}

void MainWindow::openScenario()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Scenario",
        QString(),
        ScenarioFile::fileFilter()
    );
    
    if (!filePath.isEmpty()) {
        loadScenario(filePath);
    }
}

void MainWindow::saveScenarioAs()
{
    QString defaultPath = m_currentScenarioPath;
    if (defaultPath.isEmpty()) {
        defaultPath = "untitled" + ScenarioFile::defaultExtension();
    }
    
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save Scenario",
        defaultPath,
        ScenarioFile::fileFilter()
    );
    
    if (!filePath.isEmpty()) {
        saveScenario(filePath);
    }
}

// =============================================================================
// Public Slots - Simulation Control
// =============================================================================

void MainWindow::runSimulation()
{
    // Trigger via panel (which will emit runRequested)
    if (!m_simulationRunning && m_simulationPanel) {
        // Switch to Simulation tab
        m_tabWidget->setCurrentWidget(m_simulationPanel);
        // Panel's Run button handles the request
    }
}

void MainWindow::runSimulationWithConfig(const SimulationConfig& config)
{
    if (m_simulationRunning) {
        qWarning() << "Simulation already running";
        return;
    }
    
    // Check force composition
    if (!m_forceTree->isValidForSimulation()) {
        QMessageBox::warning(this, "Invalid Force Composition",
                            "Please add at least one unit to both BLUFOR and OPFOR.\n\n"
                            "Drag platforms from the Platform Browser to the Force Tree.");
        return;
    }
    
    // Build scenario from force composition
    ScenarioBuilder builder;
    builder.setName("ATHENA Simulation");
    builder.setDescription("Generated from UI force composition");
    builder.setMaxTicks(3600);  // 1 hour
    
    // Set platform database for capability lookup
    if (m_platformBrowser && m_platformBrowser->model()) {
        builder.setPlatformDatabase(m_platformBrowser->model()->getDatabase());
    }
    
    // Add units from force tree
    builder.addUnits(m_forceTree->getUnits());
    
    // Build the scenario
    auto scenario = builder.build();
    if (!scenario) {
        QMessageBox::warning(this, "Scenario Build Error",
                            QString("Failed to build scenario:\n%1").arg(builder.lastError()));
        return;
    }
    
    qDebug() << "Built scenario with" 
             << builder.bluforCount() << "BLUFOR and"
             << builder.opforCount() << "OPFOR units";
    
    // Start simulation
    if (m_simulationRunner->start(std::move(scenario), config)) {
        m_simulationRunning = true;
        m_runAction->setEnabled(false);
        m_pauseAction->setEnabled(false);
        m_stopAction->setEnabled(true);
        m_progressBar->setVisible(true);
        m_statusLabel->setText("Running simulation...");
        
        emit simulationStateChanged(true);
        qDebug() << "Simulation started with" << config.iterations << "iterations";
    } else {
        QMessageBox::warning(this, "Simulation Error", 
                            "Failed to start simulation");
    }
}

void MainWindow::pauseSimulation()
{
    if (!m_simulationRunning) {
        return;
    }
    
    // Toggle pause
    const bool wasPaused = m_pauseAction->isChecked();
    m_statusLabel->setText(wasPaused ? "Running simulation..." : "Paused");
    
    // TODO: Actually pause simulation
    qDebug() << "Simulation" << (wasPaused ? "resumed" : "paused");
}

void MainWindow::stopSimulation()
{
    if (!m_simulationRunning) {
        return;
    }
    
    // Cancel the runner
    if (m_simulationRunner && m_simulationRunner->isRunning()) {
        m_simulationRunner->cancel();
        m_statusLabel->setText("Stopping simulation...");
        // UI will be updated when finished signal arrives
    } else {
        // No runner, just reset UI
        m_simulationRunning = false;
        m_runAction->setEnabled(true);
        m_pauseAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_progressBar->setVisible(false);
        m_statusLabel->setText("Simulation stopped");
        
        emit simulationStateChanged(false);
    }
    
    qDebug() << "Simulation stop requested";
}

void MainWindow::exportResults()
{
    // Get actual results from SimulationRunner
    if (!m_simulationRunner) {
        QMessageBox::warning(this, "Export Failed", "No simulation data available.");
        return;
    }
    
    SimulationResults results = m_simulationRunner->currentResults();
    
    if (!results.completed && results.iterations.isEmpty()) {
        QMessageBox::warning(this, "Export Failed", 
            "No simulation results to export.\nRun a simulation first.");
        return;
    }
    
    // Get scenario name for filename
    QString scenarioName = m_currentScenarioFile.isEmpty() ? 
        "Untitled_Scenario" : QFileInfo(m_currentScenarioFile).baseName();
    
    // Ask user for file location and format
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Results",
        QDir::homePath() + "/" + scenarioName + "_results",
        ResultsExporter::getFileFilter()
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    bool success = false;
    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        success = ResultsExporter::exportToCsv(results, filePath);
    } else if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
        success = ResultsExporter::exportToJson(results, filePath);
    } else if (filePath.endsWith(".html", Qt::CaseInsensitive)) {
        success = ResultsExporter::exportToHtml(results, filePath);
    } else if (filePath.endsWith(".txt", Qt::CaseInsensitive)) {
        success = ResultsExporter::exportToText(results, filePath);
    } else {
        // Default to CSV
        if (!filePath.endsWith(".csv")) {
            filePath += ".csv";
        }
        success = ResultsExporter::exportToCsv(results, filePath);
    }
    
    if (success) {
        m_statusLabel->setText(QString("Results exported to %1").arg(QFileInfo(filePath).fileName()));
        QMessageBox::information(this, "Export Complete",
            QString("Results exported successfully to:\n%1").arg(filePath));
    } else {
        QMessageBox::warning(this, "Export Failed",
            QString("Failed to export results to:\n%1").arg(filePath));
    }
}

void MainWindow::exportMapToPng()
{
    if (!m_mapView || !m_mapView->mapScene()) {
        QMessageBox::warning(this, "Export Failed", "No map to export.");
        return;
    }
    
    QString scenarioName = m_currentScenarioFile.isEmpty() ? 
        "map" : QFileInfo(m_currentScenarioFile).baseName();
    
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Map as PNG",
        QDir::homePath() + "/" + scenarioName + "_map.png",
        "PNG Images (*.png)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    if (!filePath.endsWith(".png", Qt::CaseInsensitive)) {
        filePath += ".png";
    }
    
    // Get scene rect
    MapScene* scene = m_mapView->mapScene();
    QRectF sceneRect = scene->sceneRect();
    
    // Create image (limit to reasonable size)
    const int maxDim = 4096;
    int width = qMin(static_cast<int>(sceneRect.width()), maxDim);
    int height = qMin(static_cast<int>(sceneRect.height()), maxDim);
    
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::white);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    scene->render(&painter, QRectF(), sceneRect);
    painter.end();
    
    if (pixmap.save(filePath)) {
        m_statusLabel->setText(QString("Map exported to %1").arg(QFileInfo(filePath).fileName()));
        QMessageBox::information(this, "Export Complete",
            QString("Map exported successfully to:\n%1\nSize: %2x%3 pixels")
            .arg(filePath).arg(width).arg(height));
    } else {
        QMessageBox::warning(this, "Export Failed",
            QString("Failed to export map to:\n%1").arg(filePath));
    }
}

void MainWindow::exportMapToPdf()
{
    if (!m_mapView || !m_mapView->mapScene()) {
        QMessageBox::warning(this, "Export Failed", "No map to export.");
        return;
    }
    
    QString scenarioName = m_currentScenarioFile.isEmpty() ? 
        "map" : QFileInfo(m_currentScenarioFile).baseName();
    
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Map as PDF",
        QDir::homePath() + "/" + scenarioName + "_map.pdf",
        "PDF Documents (*.pdf)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    if (!filePath.endsWith(".pdf", Qt::CaseInsensitive)) {
        filePath += ".pdf";
    }
    
    MapScene* scene = m_mapView->mapScene();
    QRectF sceneRect = scene->sceneRect();
    
    // Create PDF writer
    QPdfWriter writer(filePath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setResolution(150);
    writer.setTitle(scenarioName + " - Tactical Map");
    writer.setCreator("ATHENA Military Simulation");
    
    QPainter painter(&writer);
    
    // Calculate scaling to fit page
    QRectF pageRect = painter.viewport();
    double scaleX = pageRect.width() / sceneRect.width();
    double scaleY = pageRect.height() / sceneRect.height();
    double scale = qMin(scaleX, scaleY) * 0.95;  // 5% margin
    
    painter.scale(scale, scale);
    painter.translate(-sceneRect.topLeft());
    
    scene->render(&painter, QRectF(), sceneRect);
    painter.end();
    
    m_statusLabel->setText(QString("Map exported to %1").arg(QFileInfo(filePath).fileName()));
    QMessageBox::information(this, "Export Complete",
        QString("Map exported successfully to:\n%1").arg(filePath));
}

// =============================================================================
// Public Slots - View Operations
// =============================================================================

void MainWindow::resetView()
{
    if (m_mapView) {
        m_mapView->resetView();
    }
}

void MainWindow::zoomIn()
{
    if (m_mapView) {
        m_mapView->zoomIn();
    }
}

void MainWindow::zoomOut()
{
    if (m_mapView) {
        m_mapView->zoomOut();
    }
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::applyFormation(int formationType)
{
    applyFormationWithParams(formationType, 0.0, 0.005);
}

void MainWindow::showFormationDialog()
{
    if (!m_mapView || !m_mapView->mapScene()) {
        return;
    }
    
    QList<UnitItem*> selectedUnits = m_mapView->mapScene()->selectedUnits();
    if (selectedUnits.size() < 2) {
        m_statusLabel->setText("Select at least 2 units for formation.");
        return;
    }
    
    FormationDialog dialog(selectedUnits.size(), this);
    if (dialog.exec() == QDialog::Accepted) {
        double spacingKm = dialog.spacing();
        double spacingDeg = spacingKm / 111.0;  // Approx km to degrees
        applyFormationWithParams(
            static_cast<int>(dialog.formationType()),
            dialog.heading(),
            spacingDeg
        );
    }
}

void MainWindow::applyFormationWithParams(int formationType, double heading, double spacing)
{
    if (!m_mapView || !m_mapView->mapScene()) {
        return;
    }
    
    // Get selected units
    QList<UnitItem*> selectedUnits = m_mapView->mapScene()->selectedUnits();
    if (selectedUnits.isEmpty()) {
        m_statusLabel->setText("No units selected. Select units first.");
        return;
    }
    
    if (selectedUnits.size() < 2) {
        m_statusLabel->setText("Select at least 2 units for formation.");
        return;
    }
    
    // Calculate center of selected units
    double sumLat = 0, sumLon = 0;
    for (UnitItem* unit : selectedUnits) {
        sumLat += unit->geoLat();
        sumLon += unit->geoLon();
    }
    
    FormationParams params;
    params.center = QPointF(sumLon / selectedUnits.size(), sumLat / selectedUnits.size());
    params.unitCount = selectedUnits.size();
    params.spacing = spacing;
    params.heading = heading;
    
    // Generate positions
    QVector<QPointF> positions = FormationTemplates::generate(
        static_cast<FormationType>(formationType), params);
    
    if (positions.size() != selectedUnits.size()) {
        qWarning() << "Formation position count mismatch";
        return;
    }
    
    // Apply positions to units
    for (int i = 0; i < selectedUnits.size(); ++i) {
        UnitItem* unit = selectedUnits[i];
        double newLon = positions[i].x();
        double newLat = positions[i].y();
        
        // Create undo command for each move
        if (m_undoStack && m_forceTree && m_forceTree->model()) {
            auto* cmd = new MoveUnitCommand(
                m_mapView->mapScene(),
                unit->unitId(),
                unit->geoLat(), unit->geoLon(),
                newLat, newLon
            );
            m_undoStack->push(cmd);
        }
        
        // Apply position
        unit->setGeoPosition(newLat, newLon);
        QPointF scenePos = m_mapView->mapScene()->geoToScene(newLat, newLon);
        unit->setPos(scenePos);
    }
    
    QString formationName = FormationTemplates::typeName(static_cast<FormationType>(formationType));
    m_statusLabel->setText(QString("%1 units arranged in %2 formation (heading: %3°)")
        .arg(selectedUnits.size()).arg(formationName).arg(heading, 0, 'f', 0));
}

// =============================================================================
// Protected Methods
// =============================================================================

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_scenarioModified) {
        const auto result = QMessageBox::question(
            this,
            "Unsaved Changes",
            "Current scenario has unsaved changes. Save before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
        );
        
        if (result == QMessageBox::Save) {
            saveScenarioAs();
        } else if (result == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }
    
    // Stop simulation if running
    if (m_simulationRunning) {
        stopSimulation();
    }
    
    saveSettings();
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    // Global keyboard shortcuts
    switch (event->key()) {
        case Qt::Key_F11:
            toggleFullscreen();
            break;
        case Qt::Key_Escape:
            if (isFullScreen()) {
                showNormal();
            }
            break;
        default:
            QMainWindow::keyPressEvent(event);
            break;
    }
}

// =============================================================================
// Private Slots
// =============================================================================

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index)
    updateStatusBar();
}

void MainWindow::onSimulationProgress(int current, int total)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("Running simulation... %1/%2").arg(current).arg(total));
}

void MainWindow::onSimulationComplete()
{
    m_simulationRunning = false;
    m_runAction->setEnabled(true);
    m_pauseAction->setEnabled(false);
    m_stopAction->setEnabled(false);
    m_progressBar->setVisible(false);
    m_statusLabel->setText("Simulation complete");
    
    emit simulationStateChanged(false);
    
    // Generate engagement visualization based on results
    generateEngagementVisualization();
    
    // Switch to results tab
    m_tabWidget->setCurrentIndex(2);
}

void MainWindow::generateEngagementVisualization()
{
    if (!m_mapView || !m_mapView->mapScene()) {
        return;
    }
    
    MapScene* scene = m_mapView->mapScene();
    QList<UnitItem*> allUnits = scene->allUnits();
    
    // Separate by side
    QList<UnitItem*> blufor, opfor;
    for (UnitItem* unit : allUnits) {
        if (unit->isBlufor()) {
            blufor.append(unit);
        } else {
            opfor.append(unit);
        }
    }
    
    // Generate engagements based on simulation results
    QVector<Engagement> engagements;
    
    if (m_simulationRunner && !blufor.isEmpty() && !opfor.isEmpty()) {
        SimulationResults results = m_simulationRunner->currentResults();
        
        // Create engagements proportional to casualties
        double bluforCasRate = results.avgBluforCasualties / 
            qMax(1, results.initialBlufor);
        double opforCasRate = results.avgOpforCasualties / 
            qMax(1, results.initialOpfor);
        
        // BLUFOR attacking OPFOR
        int bluforEngagements = qMin(blufor.size(), opfor.size());
        for (int i = 0; i < bluforEngagements; ++i) {
            Engagement eng;
            eng.attackerId = blufor[i % blufor.size()]->unitId();
            eng.targetId = opfor[i % opfor.size()]->unitId();
            eng.damage = opforCasRate * (0.5 + 0.5 * (i == 0 ? 1.0 : 0.5));
            eng.isActive = false;
            engagements.append(eng);
        }
        
        // OPFOR attacking BLUFOR
        int opforEngagements = qMin(opfor.size(), blufor.size());
        for (int i = 0; i < opforEngagements; ++i) {
            Engagement eng;
            eng.attackerId = opfor[i % opfor.size()]->unitId();
            eng.targetId = blufor[i % blufor.size()]->unitId();
            eng.damage = bluforCasRate * (0.5 + 0.5 * (i == 0 ? 1.0 : 0.5));
            eng.isActive = false;
            engagements.append(eng);
        }
    }
    
    scene->setEngagements(engagements);
}

void MainWindow::updateStatusBar()
{
    // Update platform count
    const int platformCount = m_platformBrowser ? m_platformBrowser->platformCount() : 0;
    m_platformCountLabel->setText(QString("Platforms: %1").arg(platformCount));
    
    // Update force counts
    int bluforCount = 0;
    int opforCount = 0;
    if (m_forceTree) {
        bluforCount = m_forceTree->bluforCount();
        opforCount = m_forceTree->opforCount();
    }
    m_bluforCountLabel->setText(QString("BLUFOR: %1").arg(bluforCount));
    m_opforCountLabel->setText(QString("OPFOR: %1").arg(opforCount));
}

void MainWindow::updateWindowTitle()
{
    QString title = "ATHENA";
    
    if (!m_currentScenarioPath.isEmpty()) {
        title += " - " + QFileInfo(m_currentScenarioPath).fileName();
    }
    
    if (m_scenarioModified) {
        title += " *";
    }
    
    setWindowTitle(title);
}

// =============================================================================
// Private Methods - Setup
// =============================================================================

void MainWindow::setupUi()
{
    // Window properties
    setWindowTitle("ATHENA - Military Simulation Platform");
    setMinimumSize(1024, 768);
    resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    // Create undo stack
    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(100);
    
    // Set window icon (TODO: add icon to resources)
    // setWindowIcon(QIcon(":/icons/athena.png"));
    
    // Setup components
    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupDockWidgets();
    setupStatusBar();
    setupConnections();
}

void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();
    
    // -------------------------------------------------------------------------
    // File Menu
    // -------------------------------------------------------------------------
    m_fileMenu = menuBar->addMenu("&File");
    
    m_newAction = m_fileMenu->addAction("&New Scenario");
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setIcon(QIcon::fromTheme("document-new"));
    
    m_openAction = m_fileMenu->addAction("&Open Scenario...");
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setIcon(QIcon::fromTheme("document-open"));
    
    m_fileMenu->addSeparator();
    
    m_saveAction = m_fileMenu->addAction("&Save");
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setIcon(QIcon::fromTheme("document-save"));
    
    m_saveAsAction = m_fileMenu->addAction("Save &As...");
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    
    m_fileMenu->addSeparator();
    
    m_exportAction = m_fileMenu->addAction("&Export Results...");
    m_exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    
    m_exportMapPngAction = m_fileMenu->addAction("Export Map as &PNG...");
    m_exportMapPngAction->setToolTip("Save current map view as PNG image");
    
    m_exportMapPdfAction = m_fileMenu->addAction("Export Map as P&DF...");
    m_exportMapPdfAction->setToolTip("Save current map view as PDF document");
    
    m_fileMenu->addSeparator();
    
    // Recent Files submenu
    m_recentFilesMenu = m_fileMenu->addMenu("&Recent Files");
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        m_recentFileActions[i] = new QAction(this);
        m_recentFileActions[i]->setVisible(false);
        connect(m_recentFileActions[i], &QAction::triggered, this, &MainWindow::openRecentFile);
        m_recentFilesMenu->addAction(m_recentFileActions[i]);
    }
    m_recentFilesMenu->addSeparator();
    m_clearRecentAction = m_recentFilesMenu->addAction("Clear Recent Files");
    connect(m_clearRecentAction, &QAction::triggered, this, &MainWindow::clearRecentFiles);
    updateRecentFilesMenu();
    
    m_fileMenu->addSeparator();
    
    m_exitAction = m_fileMenu->addAction("E&xit");
    m_exitAction->setShortcut(QKeySequence::Quit);
    
    // -------------------------------------------------------------------------
    // Edit Menu
    // -------------------------------------------------------------------------
    m_editMenu = menuBar->addMenu("&Edit");
    
    // Use QUndoStack's built-in actions
    m_undoAction = m_undoStack->createUndoAction(this, "&Undo");
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_editMenu->addAction(m_undoAction);
    
    m_redoAction = m_undoStack->createRedoAction(this, "&Redo");
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_editMenu->addAction(m_redoAction);
    
    m_editMenu->addSeparator();
    
    m_cutAction = m_editMenu->addAction("Cu&t");
    m_cutAction->setShortcut(QKeySequence::Cut);
    
    m_copyAction = m_editMenu->addAction("&Copy");
    m_copyAction->setShortcut(QKeySequence::Copy);
    
    m_pasteAction = m_editMenu->addAction("&Paste");
    m_pasteAction->setShortcut(QKeySequence::Paste);
    
    m_deleteAction = m_editMenu->addAction("&Delete");
    m_deleteAction->setShortcut(QKeySequence::Delete);
    
    // -------------------------------------------------------------------------
    // View Menu
    // -------------------------------------------------------------------------
    m_viewMenu = menuBar->addMenu("&View");
    
    m_zoomInAction = m_viewMenu->addAction("Zoom &In");
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    
    m_zoomOutAction = m_viewMenu->addAction("Zoom &Out");
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    
    m_zoomFitAction = m_viewMenu->addAction("Zoom to &Fit");
    m_zoomFitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    
    m_viewMenu->addSeparator();
    
    m_fullscreenAction = m_viewMenu->addAction("&Fullscreen");
    m_fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    m_fullscreenAction->setCheckable(true);
    
    m_viewMenu->addSeparator();
    
    m_showGridAction = m_viewMenu->addAction("Show &Grid");
    m_showGridAction->setCheckable(true);
    m_showGridAction->setChecked(true);
    
    m_showTerrainAction = m_viewMenu->addAction("Show &Terrain");
    m_showTerrainAction->setCheckable(true);
    m_showTerrainAction->setChecked(true);
    
    m_generateTerrainAction = m_viewMenu->addAction("&Generate Terrain");
    m_generateTerrainAction->setShortcut(QKeySequence("Ctrl+G"));
    
    m_viewMenu->addSeparator();
    
    m_measureAction = m_viewMenu->addAction("&Measure Distance");
    m_measureAction->setCheckable(true);
    m_measureAction->setShortcut(QKeySequence("R"));
    m_measureAction->setToolTip("Click two points on map to measure distance");
    
    m_showRangesAction = m_viewMenu->addAction("Show Weapon &Ranges");
    m_showRangesAction->setCheckable(true);
    m_showRangesAction->setShortcut(QKeySequence("W"));
    m_showRangesAction->setToolTip("Display weapon range circles around units");
    
    m_movementOrdersAction = m_viewMenu->addAction("Movement &Orders Mode");
    m_movementOrdersAction->setCheckable(true);
    m_movementOrdersAction->setShortcut(QKeySequence("O"));
    m_movementOrdersAction->setToolTip("Click to add waypoints for selected unit");
    
    m_viewMenu->addSeparator();
    
    m_showMiniMapAction = m_viewMenu->addAction("Show &Mini Map");
    m_showMiniMapAction->setCheckable(true);
    m_showMiniMapAction->setChecked(true);
    m_showMiniMapAction->setShortcut(QKeySequence("M"));
    m_showMiniMapAction->setToolTip("Toggle mini map overlay");
    
    m_showEngagementsAction = m_viewMenu->addAction("Show &Engagements");
    m_showEngagementsAction->setCheckable(true);
    m_showEngagementsAction->setChecked(true);
    m_showEngagementsAction->setShortcut(QKeySequence("E"));
    m_showEngagementsAction->setToolTip("Toggle engagement lines on map");
    
    // Formation submenu
    m_formationMenu = m_viewMenu->addMenu("&Formations");
    m_formationMenu->setToolTip("Arrange selected units in formation");
    
    m_formationLineAction = m_formationMenu->addAction("&Line");
    m_formationLineAction->setShortcut(QKeySequence("Ctrl+1"));
    m_formationLineAction->setToolTip("Arrange units in a horizontal line");
    
    m_formationColumnAction = m_formationMenu->addAction("&Column");
    m_formationColumnAction->setShortcut(QKeySequence("Ctrl+2"));
    m_formationColumnAction->setToolTip("Arrange units in a vertical column");
    
    m_formationWedgeAction = m_formationMenu->addAction("&Wedge");
    m_formationWedgeAction->setShortcut(QKeySequence("Ctrl+3"));
    m_formationWedgeAction->setToolTip("V-shape with point forward");
    
    m_formationVeeAction = m_formationMenu->addAction("&Vee");
    m_formationVeeAction->setShortcut(QKeySequence("Ctrl+4"));
    m_formationVeeAction->setToolTip("V-shape with opening forward");
    
    m_formationEchelonAction = m_formationMenu->addAction("&Echelon");
    m_formationEchelonAction->setShortcut(QKeySequence("Ctrl+5"));
    m_formationEchelonAction->setToolTip("Diagonal line formation");
    
    m_formationBoxAction = m_formationMenu->addAction("&Box");
    m_formationBoxAction->setShortcut(QKeySequence("Ctrl+6"));
    m_formationBoxAction->setToolTip("Rectangular grid arrangement");
    
    m_formationCircleAction = m_formationMenu->addAction("Ci&rcle");
    m_formationCircleAction->setShortcut(QKeySequence("Ctrl+7"));
    m_formationCircleAction->setToolTip("Circular arrangement");
    
    m_formationMenu->addSeparator();
    
    m_formationCustomAction = m_formationMenu->addAction("&Custom...");
    m_formationCustomAction->setShortcut(QKeySequence("Ctrl+Shift+F"));
    m_formationCustomAction->setToolTip("Open formation dialog with custom settings");
    
    // -------------------------------------------------------------------------
    // Scenario Menu
    // -------------------------------------------------------------------------
    m_scenarioMenu = menuBar->addMenu("&Scenario");
    m_scenarioMenu->addAction("Edit &Forces...");
    m_scenarioMenu->addAction("Edit &Terrain...");
    m_scenarioMenu->addAction("Edit &Objectives...");
    m_scenarioMenu->addSeparator();
    m_scenarioMenu->addAction("&Validate Scenario");
    
    // -------------------------------------------------------------------------
    // Simulation Menu
    // -------------------------------------------------------------------------
    m_simulationMenu = menuBar->addMenu("Si&mulation");
    
    m_runAction = m_simulationMenu->addAction("&Run");
    m_runAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_runAction->setIcon(QIcon::fromTheme("media-playback-start"));
    
    m_pauseAction = m_simulationMenu->addAction("&Pause");
    m_pauseAction->setShortcut(QKeySequence(Qt::Key_F6));
    m_pauseAction->setIcon(QIcon::fromTheme("media-playback-pause"));
    m_pauseAction->setCheckable(true);
    m_pauseAction->setEnabled(false);
    
    m_stopAction = m_simulationMenu->addAction("&Stop");
    m_stopAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
    m_stopAction->setIcon(QIcon::fromTheme("media-playback-stop"));
    m_stopAction->setEnabled(false);
    
    m_simulationMenu->addSeparator();
    
    m_settingsAction = m_simulationMenu->addAction("S&ettings...");
    
    // -------------------------------------------------------------------------
    // Analysis Menu
    // -------------------------------------------------------------------------
    m_analysisMenu = menuBar->addMenu("&Analysis");
    
    m_exportResultsAction = m_analysisMenu->addAction("&Export Results...");
    m_exportResultsAction->setShortcut(QKeySequence("Ctrl+E"));
    m_exportResultsAction->setToolTip("Export simulation results to CSV, JSON, or HTML");
    
    m_analysisMenu->addSeparator();
    m_analysisMenu->addAction("&Sobol Sensitivity...");
    m_analysisMenu->addAction("&Compare Scenarios...");
    m_analysisMenu->addAction("&What-If Analysis...");
    
    // -------------------------------------------------------------------------
    // Help Menu
    // -------------------------------------------------------------------------
    m_helpMenu = menuBar->addMenu("&Help");
    m_helpMenu->addAction("&Documentation");
    m_helpMenu->addAction("&Keyboard Shortcuts");
    m_helpMenu->addSeparator();
    m_helpMenu->addAction("&About ATHENA");
}

void MainWindow::setupToolBar()
{
    // File toolbar
    m_fileToolBar = addToolBar("File");
    m_fileToolBar->setObjectName("FileToolBar");
    m_fileToolBar->addAction(m_newAction);
    m_fileToolBar->addAction(m_openAction);
    m_fileToolBar->addAction(m_saveAction);
    
    // Simulation toolbar
    m_simulationToolBar = addToolBar("Simulation");
    m_simulationToolBar->setObjectName("SimulationToolBar");
    m_simulationToolBar->addAction(m_runAction);
    m_simulationToolBar->addAction(m_pauseAction);
    m_simulationToolBar->addAction(m_stopAction);
    
    // View toolbar
    m_viewToolBar = addToolBar("View");
    m_viewToolBar->setObjectName("ViewToolBar");
    m_viewToolBar->addAction(m_zoomInAction);
    m_viewToolBar->addAction(m_zoomOutAction);
    m_viewToolBar->addAction(m_zoomFitAction);
}

void MainWindow::setupCentralWidget()
{
    // Tab widget as central widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabPosition(QTabWidget::North);
    
    // Tab 0: Scenario Editor (with Map View)
    // Create container for map + minimap overlay
    auto* scenarioContainer = new QWidget(this);
    auto* scenarioLayout = new QVBoxLayout(scenarioContainer);
    scenarioLayout->setContentsMargins(0, 0, 0, 0);
    scenarioLayout->setSpacing(0);
    
    m_mapView = new MapView(scenarioContainer);
    scenarioLayout->addWidget(m_mapView);
    
    // MiniMap as floating overlay (positioned in corner)
    m_miniMap = new MiniMap(m_mapView);
    m_miniMap->setVisible(true);
    m_miniMap->move(10, 10);
    m_miniMap->resize(180, 150);
    
    m_tabWidget->addTab(scenarioContainer, "Scenario");
    
    // Initialize RangeOverlay and MovementManager with MapScene
    if (m_mapView && m_mapView->mapScene()) {
        m_rangeOverlay = new RangeOverlay(m_mapView->mapScene());
        m_rangeOverlay->setVisible(false);  // Hidden by default
        m_mapView->mapScene()->addItem(m_rangeOverlay);
        
        m_movementManager = new MovementManager(m_mapView->mapScene());
        
        // Connect MiniMap to scene
        m_miniMap->setScene(m_mapView->mapScene());
        
        // Refresh MiniMap when selection changes
        connect(m_mapView->mapScene(), &QGraphicsScene::selectionChanged,
                m_miniMap, &MiniMap::refresh);
    }
    
    // Tab 1: Simulation Control
    m_simulationPanel = new SimulationPanel(this);
    m_tabWidget->addTab(m_simulationPanel, "Simulation");
    
    // Tab 2: Results Dashboard
    m_resultsPanel = new ResultsPanel(this);
    m_tabWidget->addTab(m_resultsPanel, "Results");
    
    setCentralWidget(m_tabWidget);
}

void MainWindow::setupDockWidgets()
{
    // -------------------------------------------------------------------------
    // Platform Browser (left)
    // -------------------------------------------------------------------------
    m_platformDock = new QDockWidget("Platform Database", this);
    m_platformDock->setObjectName("PlatformDock");
    m_platformDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    m_platformBrowser = new PlatformBrowser(this);
    m_platformDock->setWidget(m_platformBrowser);
    
    addDockWidget(Qt::LeftDockWidgetArea, m_platformDock);
    
    // -------------------------------------------------------------------------
    // Force Tree (left, below Platform Browser)
    // -------------------------------------------------------------------------
    m_forceDock = new QDockWidget("Force Composition", this);
    m_forceDock->setObjectName("ForceDock");
    m_forceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    m_forceTree = new ForceTree(this);
    m_forceDock->setWidget(m_forceTree);
    
    addDockWidget(Qt::LeftDockWidgetArea, m_forceDock);
    
    // Stack force dock below platform dock
    tabifyDockWidget(m_platformDock, m_forceDock);
    m_platformDock->raise();  // Show platform dock by default
    
    // -------------------------------------------------------------------------
    // Properties (right) - placeholder for now
    // -------------------------------------------------------------------------
    m_propertiesDock = new QDockWidget("Platform Details", this);
    m_propertiesDock->setObjectName("PropertiesDock");
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    m_detailsPanel = new PlatformDetailsPanel(this);
    m_detailsPanel->setMinimumWidth(280);
    m_propertiesDock->setWidget(m_detailsPanel);
    
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    
    // Add dock toggles to View menu
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_platformDock->toggleViewAction());
    m_viewMenu->addAction(m_forceDock->toggleViewAction());
    m_viewMenu->addAction(m_propertiesDock->toggleViewAction());
}

void MainWindow::setupStatusBar()
{
    QStatusBar* statusBar = this->statusBar();
    
    // Status label (left)
    m_statusLabel = new QLabel("Ready");
    statusBar->addWidget(m_statusLabel, 1);
    
    // Platform count
    m_platformCountLabel = new QLabel("Platforms: 0");
    statusBar->addPermanentWidget(m_platformCountLabel);
    
    // Force counts
    m_bluforCountLabel = new QLabel("BLUFOR: 0");
    m_bluforCountLabel->setStyleSheet("QLabel { color: #0066CC; font-weight: bold; }");
    statusBar->addPermanentWidget(m_bluforCountLabel);
    
    m_opforCountLabel = new QLabel("OPFOR: 0");
    m_opforCountLabel->setStyleSheet("QLabel { color: #CC0000; font-weight: bold; }");
    statusBar->addPermanentWidget(m_opforCountLabel);
    
    // Coordinate display
    m_coordLabel = new QLabel("---, ---");
    m_coordLabel->setStyleSheet("QLabel { color: #666; font-family: monospace; }");
    m_coordLabel->setMinimumWidth(130);
    statusBar->addPermanentWidget(m_coordLabel);
    
    // Progress bar (hidden by default)
    m_progressBar = new QProgressBar();
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    statusBar->addPermanentWidget(m_progressBar);
}

void MainWindow::setupConnections()
{
    // File menu
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newScenario);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openScenario);
    connect(m_saveAction, &QAction::triggered, this, [this]() {
        if (m_currentScenarioPath.isEmpty()) {
            saveScenarioAs();
        } else {
            saveScenario(m_currentScenarioPath);
        }
    });
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveScenarioAs);
    connect(m_exportMapPngAction, &QAction::triggered, this, &MainWindow::exportMapToPng);
    connect(m_exportMapPdfAction, &QAction::triggered, this, &MainWindow::exportMapToPdf);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::close);
    
    // Undo stack - mark scenario modified when commands are pushed
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this]() {
        if (!m_scenarioModified) {
            m_scenarioModified = true;
            updateWindowTitle();
        }
    });
    connect(m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
        m_scenarioModified = !clean;
        updateWindowTitle();
    });
    
    // Simulation menu
    connect(m_runAction, &QAction::triggered, this, &MainWindow::runSimulation);
    connect(m_pauseAction, &QAction::triggered, this, &MainWindow::pauseSimulation);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopSimulation);
    
    // View menu
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);
    connect(m_zoomFitAction, &QAction::triggered, this, &MainWindow::resetView);
    connect(m_fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    
    connect(m_showGridAction, &QAction::toggled, this, [this](bool checked) {
        if (m_mapView) m_mapView->setGridVisible(checked);
    });
    connect(m_showTerrainAction, &QAction::toggled, this, [this](bool checked) {
        if (m_mapView) m_mapView->setTerrainVisible(checked);
    });
    connect(m_generateTerrainAction, &QAction::triggered, this, [this]() {
        if (m_mapView) {
            m_mapView->generateTerrain(100, 100, 0);  // Random seed
            m_statusLabel->setText("Terrain generated");
        }
    });
    
    connect(m_measureAction, &QAction::toggled, this, [this](bool checked) {
        if (m_mapView) {
            m_mapView->setMeasureMode(checked);
            if (checked) {
                m_statusLabel->setText("Measure mode: Click two points to measure distance");
            } else {
                m_statusLabel->setText("Measure mode off");
            }
        }
    });
    
    connect(m_showRangesAction, &QAction::toggled, this, [this](bool checked) {
        if (m_rangeOverlay) {
            m_rangeOverlay->setVisible(checked);
            if (checked) {
                m_rangeOverlay->updateRanges();
                m_statusLabel->setText("Weapon ranges visible");
            } else {
                m_statusLabel->setText("Weapon ranges hidden");
            }
        }
    });
    
    connect(m_movementOrdersAction, &QAction::toggled, this, [this](bool checked) {
        if (m_movementManager) {
            m_movementManager->setOrderMode(checked);
            if (checked) {
                m_statusLabel->setText("Movement orders: Select a unit, then click to add waypoints");
            } else {
                m_statusLabel->setText("Movement orders mode off");
            }
        }
    });
    
    connect(m_showMiniMapAction, &QAction::toggled, this, [this](bool checked) {
        if (m_miniMap) {
            m_miniMap->setVisible(checked);
        }
    });
    
    connect(m_showEngagementsAction, &QAction::toggled, this, [this](bool checked) {
        if (m_mapView && m_mapView->mapScene()) {
            m_mapView->mapScene()->setEngagementsVisible(checked);
        }
    });
    
    // Formation actions
    connect(m_formationLineAction, &QAction::triggered, this, [this]() {
        applyFormation(static_cast<int>(FormationType::Line));
    });
    connect(m_formationColumnAction, &QAction::triggered, this, [this]() {
        applyFormation(static_cast<int>(FormationType::Column));
    });
    connect(m_formationWedgeAction, &QAction::triggered, this, [this]() {
        applyFormation(static_cast<int>(FormationType::Wedge));
    });
    connect(m_formationVeeAction, &QAction::triggered, this, [this]() {
        applyFormation(static_cast<int>(FormationType::Vee));
    });
    connect(m_formationEchelonAction, &QAction::triggered, this, [this]() {
        applyFormation(static_cast<int>(FormationType::Echelon));
    });
    connect(m_formationBoxAction, &QAction::triggered, this, [this]() {
        applyFormation(static_cast<int>(FormationType::Box));
    });
    connect(m_formationCircleAction, &QAction::triggered, this, [this]() {
        applyFormation(static_cast<int>(FormationType::Circle));
    });
    connect(m_formationCustomAction, &QAction::triggered, this, &MainWindow::showFormationDialog);
    
    // MiniMap navigation
    if (m_miniMap && m_mapView) {
        connect(m_miniMap, &MiniMap::navigationRequested, this, [this](const QPointF& scenePos) {
            if (m_mapView) {
                m_mapView->centerOn(scenePos);
            }
        });
        
        // Sync viewport rect from MapView to MiniMap
        connect(m_mapView, &MapView::viewportChanged, m_miniMap, &MiniMap::setViewportRect);
    }
    
    connect(m_exportResultsAction, &QAction::triggered, this, [this]() {
        exportResults();
    });
    
    // Tab widget
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    
    // Platform browser -> Force tree (drag-drop)
    connect(m_platformBrowser, &PlatformBrowser::platformSelected, 
            m_forceTree, &ForceTree::onPlatformSelected);
    
    // Platform browser -> Details panel (show specs)
    connect(m_platformBrowser, &PlatformBrowser::platformSelected,
            m_detailsPanel, &PlatformDetailsPanel::showPlatform);
    
    // ==========================================================================
    // Simulation Runner
    // ==========================================================================
    m_simulationRunner = new SimulationRunner(this);
    
    // SimulationPanel -> MainWindow (run/stop requests)
    connect(m_simulationPanel, &SimulationPanel::runRequested,
            this, [this](const SimulationConfig& config) {
                runSimulationWithConfig(config);
            });
    connect(m_simulationPanel, &SimulationPanel::stopRequested,
            this, &MainWindow::stopSimulation);
    
    // SimulationRunner -> SimulationPanel (progress updates)
    connect(m_simulationRunner, &SimulationRunner::progressChanged,
            m_simulationPanel, &SimulationPanel::setProgress);
    connect(m_simulationRunner, &SimulationRunner::finished,
            m_simulationPanel, &SimulationPanel::onSimulationFinished);
    
    // SimulationRunner -> ResultsPanel (show results)
    connect(m_simulationRunner, &SimulationRunner::finished,
            m_resultsPanel, &ResultsPanel::showResults);
    
    // SimulationRunner -> MainWindow (status updates)
    connect(m_simulationRunner, &SimulationRunner::progressChanged,
            this, &MainWindow::onSimulationProgress);
    connect(m_simulationRunner, &SimulationRunner::finished,
            this, [this](const SimulationResults& results) {
                onSimulationComplete();
                if (results.completed && !results.cancelled) {
                    // Switch to Results tab
                    m_tabWidget->setCurrentWidget(m_resultsPanel);
                }
            });
    connect(m_simulationRunner, &SimulationRunner::error,
            this, [this](const QString& message) {
                QMessageBox::warning(this, "Simulation Error", message);
            });
    
    // MapView connections
    if (m_mapView) {
        connect(m_mapView, &MapView::geoPositionChanged,
                this, [this](double lat, double lon) {
                    m_coordLabel->setText(QString("%1, %2")
                        .arg(lat, 0, 'f', 4)
                        .arg(lon, 0, 'f', 4));
                });
        
        connect(m_mapView, &MapView::unitSelected,
                this, [this](const QString& unitId) {
                    qDebug() << "Unit selected:" << unitId;
                });
        
        // Measurement
        connect(m_mapView, &MapView::measurementChanged,
                this, [this](double distanceKm) {
                    QString msg;
                    if (distanceKm < 1.0) {
                        msg = QString("Distance: %1 m").arg(distanceKm * 1000, 0, 'f', 0);
                    } else {
                        msg = QString("Distance: %1 km").arg(distanceKm, 0, 'f', 2);
                    }
                    m_statusLabel->setText(msg);
                });
        
        // Sync Map → ForceTree: when unit dropped on map, add to tree and create undo command
        connect(m_mapView, &MapView::unitAdded,
                this, [this](const ForceUnit& unit) {
                    // Add to ForceTree
                    m_forceTree->addUnit(unit);
                    
                    // Create undo command (action already done, so firstRedo will skip)
                    if (m_undoStack && m_mapView && m_mapView->mapScene() && m_forceTree->model()) {
                        auto* cmd = new AddUnitCommand(
                            m_mapView->mapScene(),
                            m_forceTree->model(),
                            unit
                        );
                        m_undoStack->push(cmd);
                    }
                });
    }
    
    // Sync ForceTree → Map: when unit added via UI, add to map
    if (m_forceTree && m_mapView) {
        connect(m_forceTree, &ForceTree::unitAddedFromUI,
                m_mapView, &MapView::addUnitFromTree);
    }
    
    // Context menu sync (Map → ForceTree)
    if (m_mapView && m_forceTree) {
        // Delete from map → delete from tree and create undo command
        connect(m_mapView, &MapView::unitDeleted,
                this, [this](const QString& unitId, const QString& platformId, bool wasBlufor) {
                    // Create undo command before modifying data
                    if (m_undoStack && m_mapView && m_mapView->mapScene() && m_forceTree->model()) {
                        auto* cmd = new RemoveUnitCommand(
                            m_mapView->mapScene(),
                            m_forceTree->model(),
                            unitId
                        );
                        m_undoStack->push(cmd);
                    }
                    
                    m_forceTree->removeUnit(platformId, wasBlufor);
                    m_statusLabel->setText("Unit deleted");
                });
        
        // Duplicate on map → add to tree
        connect(m_mapView, &MapView::unitDuplicated,
                this, [this](const ForceUnit& unit) {
                    m_forceTree->addUnit(unit);
                    
                    // Create undo command for the duplicate
                    if (m_undoStack && m_mapView && m_mapView->mapScene() && m_forceTree->model()) {
                        auto* cmd = new AddUnitCommand(
                            m_mapView->mapScene(),
                            m_forceTree->model(),
                            unit
                        );
                        m_undoStack->push(cmd);
                    }
                });
        
        // Change side on map → update and create undo command
        connect(m_mapView, &MapView::unitSideChanged,
                this, [this](const QString& unitId, bool nowBlufor) {
                    // Create undo command (toggles side, so stores the *previous* state)
                    if (m_undoStack && m_mapView && m_mapView->mapScene() && m_forceTree->model()) {
                        auto* cmd = new ChangeSideCommand(
                            m_mapView->mapScene(),
                            m_forceTree->model(),
                            unitId
                        );
                        m_undoStack->push(cmd);
                    }
                    m_statusLabel->setText(QString("Unit moved to %1").arg(nowBlufor ? "BLUFOR" : "OPFOR"));
                });
        
        // Quantity change on map → update and create undo command
        connect(m_mapView, &MapView::unitQuantityChanged,
                this, [this](const QString& unitId, int oldQuantity, int newQuantity) {
                    if (m_undoStack && m_mapView && m_mapView->mapScene() && m_forceTree->model()) {
                        auto* cmd = new ChangeQuantityCommand(
                            m_mapView->mapScene(),
                            m_forceTree->model(),
                            unitId,
                            oldQuantity,
                            newQuantity
                        );
                        m_undoStack->push(cmd);
                    }
                    m_statusLabel->setText(QString("Quantity updated: %1 → %2").arg(oldQuantity).arg(newQuantity));
                });
    }
}

void MainWindow::loadSettings()
{
    QSettings settings;
    
    // Window geometry
    settings.beginGroup("MainWindow");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    settings.endGroup();
    
    // Data path - try multiple locations
    QString defaultPath = settings.value("dataPath").toString();
    if (defaultPath.isEmpty()) {
        // Try relative to executable
        defaultPath = QCoreApplication::applicationDirPath() + "/data/platforms";
        if (!QDir(defaultPath).exists()) {
            // Try development path
            defaultPath = "../athena-core/data/platforms";
        }
    }
    m_dataPath = defaultPath;
}

void MainWindow::saveSettings()
{
    QSettings settings;
    
    // Window geometry
    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.endGroup();
    
    // Data path
    settings.setValue("dataPath", m_dataPath);
}

void MainWindow::updateRecentFilesMenu()
{
    QSettings settings;
    QStringList files = settings.value("recentFiles").toStringList();
    
    const int numRecentFiles = qMin(files.size(), MAX_RECENT_FILES);
    
    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = QString("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());
        m_recentFileActions[i]->setText(text);
        m_recentFileActions[i]->setData(files[i]);
        m_recentFileActions[i]->setVisible(true);
        m_recentFileActions[i]->setToolTip(files[i]);
    }
    
    for (int i = numRecentFiles; i < MAX_RECENT_FILES; ++i) {
        m_recentFileActions[i]->setVisible(false);
    }
    
    m_recentFilesMenu->setEnabled(numRecentFiles > 0);
}

void MainWindow::addToRecentFiles(const QString& filePath)
{
    QSettings settings;
    QStringList files = settings.value("recentFiles").toStringList();
    
    // Remove if already exists
    files.removeAll(filePath);
    
    // Add to front
    files.prepend(filePath);
    
    // Trim to max
    while (files.size() > MAX_RECENT_FILES) {
        files.removeLast();
    }
    
    settings.setValue("recentFiles", files);
    updateRecentFilesMenu();
}

void MainWindow::openRecentFile()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        QString filePath = action->data().toString();
        if (QFile::exists(filePath)) {
            loadScenario(filePath);
        } else {
            QMessageBox::warning(this, "File Not Found",
                QString("The file '%1' no longer exists.").arg(filePath));
            
            // Remove from recent files
            QSettings settings;
            QStringList files = settings.value("recentFiles").toStringList();
            files.removeAll(filePath);
            settings.setValue("recentFiles", files);
            updateRecentFilesMenu();
        }
    }
}

void MainWindow::clearRecentFiles()
{
    QSettings settings;
    settings.setValue("recentFiles", QStringList());
    updateRecentFilesMenu();
    m_statusLabel->setText("Recent files cleared");
}

} // namespace athena::ui
