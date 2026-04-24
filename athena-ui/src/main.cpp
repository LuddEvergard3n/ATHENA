/**
 * @file main.cpp
 * @brief ATHENA UI Application Entry Point
 * 
 * ATHENA - Advanced Tactical & Heuristic Engagement & Network Analyzer
 * Military simulation and analysis platform.
 */

#include <QApplication>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QLocale>
#include <QTranslator>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

#include "app/mainwindow.hpp"

namespace {

/**
 * @brief Configure OpenGL surface format for graphics view
 */
void configureOpenGL()
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);  // 4x MSAA antialiasing
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);
}

/**
 * @brief Set application metadata
 */
void configureApplication(QApplication& app)
{
    app.setOrganizationName("ATHENA");
    app.setOrganizationDomain("athena-sim.mil");
    app.setApplicationName("ATHENA");
    app.setApplicationVersion("0.1.0");
    app.setApplicationDisplayName("ATHENA - Military Simulation Platform");
    
    // Use Fusion style for consistent cross-platform look
    app.setStyle(QStyleFactory::create("Fusion"));
}

/**
 * @brief Ensure required directories exist
 */
void ensureDirectories()
{
    const QStringList dirs = {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/scenarios",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/results",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/exports"
    };
    
    for (const QString& dir : dirs) {
        QDir().mkpath(dir);
    }
}

/**
 * @brief Parse command line arguments
 */
struct CommandLineOptions {
    QString scenarioFile;
    bool debugMode = false;
    QString dataPath;
};

CommandLineOptions parseCommandLine(QApplication& app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("ATHENA Military Simulation Platform");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // --scenario <file>
    QCommandLineOption scenarioOption(
        QStringList() << "s" << "scenario",
        "Load scenario file on startup",
        "file"
    );
    parser.addOption(scenarioOption);
    
    // --debug
    QCommandLineOption debugOption(
        QStringList() << "d" << "debug",
        "Enable debug mode"
    );
    parser.addOption(debugOption);
    
    // --data <path>
    QCommandLineOption dataOption(
        "data",
        "Path to platform database directory",
        "path"
    );
    parser.addOption(dataOption);
    
    parser.process(app);
    
    CommandLineOptions opts;
    opts.scenarioFile = parser.value(scenarioOption);
    opts.debugMode = parser.isSet(debugOption);
    opts.dataPath = parser.value(dataOption);
    
    return opts;
}

} // anonymous namespace

/**
 * @brief Application entry point
 */
int main(int argc, char* argv[])
{
    // Configure OpenGL before QApplication
    configureOpenGL();
    
    // Create application
    QApplication app(argc, argv);
    configureApplication(app);
    
    // Parse command line
    const auto options = parseCommandLine(app);
    
    if (options.debugMode) {
        qDebug() << "ATHENA starting in debug mode";
        qDebug() << "Data path:" << options.dataPath;
    }
    
    // Ensure directories exist
    ensureDirectories();
    
    // Create and show main window
    athena::ui::MainWindow mainWindow;
    
    // Load scenario if specified
    if (!options.scenarioFile.isEmpty()) {
        mainWindow.loadScenario(options.scenarioFile);
    }
    
    // Set data path if specified
    if (!options.dataPath.isEmpty()) {
        mainWindow.setDataPath(options.dataPath);
    }
    
    mainWindow.show();
    
    // Enter event loop
    return app.exec();
}
