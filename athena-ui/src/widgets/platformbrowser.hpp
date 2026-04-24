/**
 * @file platformbrowser.hpp
 * @brief Platform database browser widget
 * 
 * Uses athena-core PlatformDatabase to display all platforms.
 */

#ifndef ATHENA_UI_PLATFORMBROWSER_HPP
#define ATHENA_UI_PLATFORMBROWSER_HPP

#include <QWidget>
#include <QString>

class QLineEdit;
class QComboBox;
class QTreeView;
class QSortFilterProxyModel;
class QLabel;

namespace athena::ui {

class PlatformModel;

/**
 * @brief Widget for browsing and filtering the platform database
 * 
 * Features:
 * - Search by name
 * - Filter by country, type, category
 * - Tree view with hierarchical organization
 * - Drag-and-drop to Force Tree
 * - Details panel on selection
 */
class PlatformBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit PlatformBrowser(QWidget* parent = nullptr);
    ~PlatformBrowser() override;

    /**
     * @brief Load platform database from directory
     * @param path Directory containing platform JSON files
     * @return Number of platforms loaded
     */
    int loadDatabase(const QString& path);

    /**
     * @brief Get total platform count
     */
    int platformCount() const;

    /**
     * @brief Get underlying platform model
     */
    PlatformModel* model() const { return m_model; }

signals:
    /**
     * @brief Emitted when a platform is selected
     * @param platformId Platform ID
     */
    void platformSelected(const QString& platformId);

    /**
     * @brief Emitted when database is loaded
     * @param count Number of platforms loaded
     */
    void databaseLoaded(int count);

private slots:
    void onSearchTextChanged(const QString& text);
    void onFilterChanged();
    void onSelectionChanged();

private:
    void setupUi();
    void setupConnections();
    void updateCountLabel();

    // UI components
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_countryFilter = nullptr;
    QComboBox* m_categoryFilter = nullptr;
    QTreeView* m_treeView = nullptr;
    QLabel* m_countLabel = nullptr;

    // Model
    PlatformModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxyModel = nullptr;
};

} // namespace athena::ui

#endif // ATHENA_UI_PLATFORMBROWSER_HPP
