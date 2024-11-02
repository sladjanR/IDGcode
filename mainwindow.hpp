#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

// Qt
#include <QGraphicsScene>
#include <QListView>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QVBoxLayout>

// Forward deklaracija
class CodeEditor;
class QTreeView;
class QFileSystemModel;
class Repository;
class FindMe;
class CompareWorker;
class PreferencesDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    CodeEditor *codeEditor;
    QDockWidget *fileExplorerDock;
    QTreeView *treeView;
    QDockWidget *commitHistoryDock;
    QFileSystemModel *fileModel;
    QString currentFilePath;
    bool savedFile;
    Repository *repo;
    QStandardItemModel* commitModel;

    QDockWidget* commitHistoryDockWidget;
    QListView* commitHistoryView;
    QWidget* commitDetailsWidget;
    QVBoxLayout* commitDetailsLayout;
    FindMe* findTextDialog;
    QToolBar* toolBar;

    QGraphicsView *branchGraphView;
    QGraphicsScene *branchGraphScene;


public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event);
private slots:
    void changeTheme(const QString &theme);
    void saveFile();
    void newProject();
    void openProject();
    // void openFile();
    void newFile();
    void treeViewClicked(const QModelIndex &index);

    void commitChanges();

    void onTreeViewContextMenu(const QPoint &point);
    void onAddNewFolder();
    void onCommitClicked(const QModelIndex &index);
    void newFolder();
    void stashChanges();
    void applyStash();
    void discardStash();
    void onExit();
    void showFindMe();
    void findText(const QString &text);

    void pushDataToServer();
    void pullDataFromServer();

    void createBranch();
    void switchBranch();
    void tagCommit();
    void viewTags();
    void restoreToPreviousCommit();
    void mergeBranch();
    void compareCommits();
    void showAboutDialog();
    void showContactDialog();
    void showPreferencesDialog();
    void onRemoveFolderFile();

    void updateLanguage(const QString &language);
    void updateFont(const QFont &font);
    void updateThemeColor(const QColor &color);
    void updateShortcut(const QString &action, const QKeySequence &shortcut);
private:
    void createDockWidget();
    void createMenubar();
    void createToolBar();
    void createStatusBar();
    void applyDarkTheme();
    void applyLightTheme();


    void onAddNewFile();
    void populateCommitHistory();
    bool askQuestion(QWidget *parent, const QString &title, const QString &question);

    void setupCommitHistoryDockWidget();
    PreferencesDialog *preferencesDialog;

    void setupBranchGraphView();
    void visualizeBranches();
    void showDiff(const QString &fileName, const QString &oldContent, const QString &newContent);
    bool eventFilter(QObject *obj, QEvent *event);
    void compareBlobsInTree(const QMap<QString, QString> &lastBlobs, const QMap<QString, QString> &previousBlobs, const QString &basePath);
    void compareTrees(const QMap<QString, QString> &lastTrees, const QMap<QString, QString> &previousTrees, const QString &basePath);
};


#endif // MAINWINDOW_HPP
