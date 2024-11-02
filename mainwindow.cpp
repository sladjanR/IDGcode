#include "mainwindow.hpp"
#include "codeeditor.hpp"
#include "cppsyntaxhighlighter.hpp"
#include "repository.hpp"
#include "findme.hpp"
#include "preferencesdialog.hpp"
#include "compareworker.hpp"
#include "mergeworker.hpp"

// Qt
#include <QApplication>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QFile>
#include <QStyle>
#include <QTreeView>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QGraphicsTextItem>
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}
{
    this->setWindowTitle("IDGcode");
    this->resize(1024,768);
    createMenubar();
    preferencesDialog = new PreferencesDialog(this);
    createToolBar();
    createStatusBar();
    createDockWidget();
    applyDarkTheme();

    // Pocetne vrednosti
    savedFile = false;


    // Za centralni deo prozora postavljamo codeEditor
    codeEditor = new CodeEditor(this);
    setCentralWidget(codeEditor);

    // Dodavanje nase klase za c++ highlight teksta
    CPPSyntaxHighlighter *highlighter = new CPPSyntaxHighlighter(codeEditor->document());
    codeEditor->setSyntaxHighlighting(highlighter);

    findTextDialog = new FindMe(this);


    // Osnovni konektori
    connect(treeView, &QTreeView::clicked, this, &MainWindow::treeViewClicked);
    connect(findTextDialog, &FindMe::findRequested, this, &MainWindow::findText);


}

// Metoda zaduzena za promenu boja pozadine
void MainWindow::changeTheme(const QString &theme)
{
    if (theme == "dark")
    {
        applyDarkTheme();
    }
    else
    {
        applyLightTheme();
    }
}

// Cuvanje fajla
void MainWindow::saveFile()
{
    if (!currentFilePath.isEmpty())
    {
        QFile file(currentFilePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&file);
            out << codeEditor->toPlainText();
            savedFile = true;
        }
    }
}

// Otvaranje novog projekta i pravljenje direktorijuma kao pocetnim root-om
void MainWindow::newProject()
{
    // Potrebno je pre svega otvoriti dialog sa upitom gde kreirati novi projekat (repozitorijum)
    QString dirPath = QFileDialog::getExistingDirectory(this, "New Project", "");

    if (!dirPath.isEmpty())
    {
        QDir dir(dirPath);
        QString projectName = QInputDialog::getText(this, "Project Name", "Enter project name: ");

        if (!projectName.isEmpty())
        {
            dir.mkdir(projectName);
            treeView->setRootIndex(fileModel->index(dir.absoluteFilePath(projectName)));
            repo = new Repository(dir.absoluteFilePath(projectName));
        }
    }
}

// Otvaranje vec postojeceg projekta
void MainWindow::openProject()
{
    savedFile = false;
    QString dirPath = QFileDialog::getExistingDirectory(this,"Open Existing Project", "");

    if (!dirPath.isEmpty())
    {
        treeView->setRootIndex(fileModel->index(dirPath));

        repo = new Repository(dirPath);
        repo->restoreToCommit(repo->getLastCommitHash());
        currentFilePath.clear();
        codeEditor->clear();
        populateCommitHistory();
    }
}

// Postavljanje prozora koji ce predstavljati prikaz istorije commit-a
void MainWindow::setupCommitHistoryDockWidget() {

    commitHistoryDockWidget = new QDockWidget("Commit History", this);
    commitHistoryDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);


    commitHistoryView = new QListView(this);
    commitHistoryView->setStyleSheet("QListView {"
                                     "  border-radius: 10px;"
                                     "  border: 2px solid #cccccc;"
                                     "  background-color: #2E3440;"
                                     "  font-family: 'Segoe UI';"
                                     "  font-size: 12pt;"
                                     "}");

    commitModel = new QStandardItemModel(this);
    commitHistoryView->setModel(commitModel);

    commitDetailsWidget = new QWidget(this);
    commitDetailsWidget->setStyleSheet("QWidget {"
                                       "  background-color: #2E3440;"
                                       "  border-radius: 10px;"
                                       "  border: 2px solid #cccccc;"
                                       "  padding: 10px;"
                                       "}");

    commitDetailsLayout = new QVBoxLayout(commitDetailsWidget);
    commitDetailsLayout->setSpacing(10);

    // Postavljanje glavnog layout-a, tj. mainLayout
    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->addWidget(commitHistoryView);
    mainLayout->addWidget(commitDetailsWidget);

    QWidget* containerWidget = new QWidget(this);
    containerWidget->setLayout(mainLayout);

    commitHistoryDockWidget->setWidget(containerWidget);
    addDockWidget(Qt::RightDockWidgetArea, commitHistoryDockWidget);


    connect(commitHistoryView, &QListView::clicked, this, &MainWindow::onCommitClicked);
}

// Slot koji definise ponasanje nakon klika na prilaganje commit-a
void MainWindow::onCommitClicked(const QModelIndex& index) {
    QString commitHash = index.data(Qt::UserRole + 1).toString();

    QString author = repo->getCommitAuthor(commitHash);
    QString message = repo->getCommitMessage(commitHash);
    qint64 timestamp = repo->loadCommit(commitHash).getTimestamp();
    // Obratiti paznju da je su u pitanju milisekunde, a ne sekunde
    QString localTime = QDateTime::fromMSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");
    // Brisemo prethodne postavke
    QLayoutItem* item;
    while ((item = commitDetailsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }


    QLabel* authorLabel = new QLabel("Author: " + author, commitDetailsWidget);
    QLabel* messageLabel = new QLabel("Message: " + message, commitDetailsWidget);
    QLabel* timeLabel = new QLabel("Date: " + localTime, commitDetailsWidget);

    authorLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14pt; color: #757555;}");
    messageLabel->setStyleSheet("QLabel { font-size: 12pt; color: #757555; }");
    timeLabel->setStyleSheet("QLabel { font-size: 12pt; color: #9e9e9e; }");
    commitDetailsLayout->addWidget(authorLabel);
    commitDetailsLayout->addWidget(messageLabel);
    commitDetailsLayout->addWidget(timeLabel);

    commitDetailsWidget->adjustSize();
}

// Punjeje liste elementima
void MainWindow::populateCommitHistory()
{
    if (repo == nullptr)
    {
        qDebug() << "Potrebno je prvo otvoriti ili kreirati repozitorijum kako bi update listu";
        return;
    }

    // Proveravamo da li je doslo do promene tagova
    repo->loadTags();

    // Cistimo prosle unose
    commitModel->clear();
    QString currentBranch = repo->getCurrentBranch();
    QVector<QString> commitHashes = repo->getCommitsForBranch(currentBranch);

    for (const QString& commitHash : commitHashes)
    {
        QString displayText = repo->getCommitTagForHash(commitHash);
        if (displayText.isEmpty())
        {
            displayText = commitHash;
        }

        QStandardItem* item = new QStandardItem(displayText);
        item->setData(commitHash);

        item->setFont(QFont("Segoe UI", 11));
        item->setBackground(QBrush(QColor("#e0f7fa")));
        item->setForeground(QBrush(QColor("#00695c")));

        commitModel->appendRow(item);
    }
    qDebug() << "Metoda history list je uspesno zavrsena";
}

// Dodavanje novog fajla, za sada ova metoda nema primenu TODO
void MainWindow::newFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, "New File", "", "Text Files (*.txt);; All Files (*))");
    if (!fileName.isEmpty())
    {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            file.close();
            QFileInfo fileInfo(fileName);
            treeView->setRootIndex(fileModel->index(fileInfo.absolutePath()));
        }
    }
}

// Pravljenje novog foldera
void MainWindow::newFolder()
{
    QString path = QFileDialog::getExistingDirectory(this, "Select Directory", "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!path.isEmpty())
    {
        QString folderName = QInputDialog::getText(this, "New Folder", "Enter folder name:");

        if (!folderName.isEmpty())
        {
            QDir dir(path);
            if (dir.mkpath(folderName))  // Ukoliko ne postoji kreiramo ga
            {
                // Mozemo i osveziti stablo prikaza
                treeView->setRootIndex(fileModel->index(path));
            }
            else
            {
                qDebug() << "Nije uspelo kreiranje foldera:" << dir.filePath(folderName);
            }
        }
    }
}

// Definisanje implementacije za klik na stablo prikaza
void MainWindow::treeViewClicked(const QModelIndex &index)
{
    // Ovde cemo primeniti auto-save pre nego sto promenimo input na sledeci fajl
    saveFile();

    QString filePath = fileModel->filePath(index);
    QFileInfo fileInfo(filePath);

    if (fileInfo.isFile())
    {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            codeEditor->setPlainText(in.readAll());
            currentFilePath = filePath;
        }
    }
}

// Osnovna klasa za kreiranje dock widget-a
void MainWindow::createDockWidget()
{
    fileExplorerDock = new QDockWidget("File Explorer", this);
    treeView = new QTreeView(fileExplorerDock);
    treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    treeView->setSelectionMode(QAbstractItemView::SingleSelection);

    // Nas event filter - da kada se klikne na prazno mesto, odselektuje se
    treeView->viewport()->installEventFilter(this);

    fileModel = new QFileSystemModel(this);
    fileModel->setRootPath(QDir::rootPath());
    treeView->setModel(fileModel);
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    fileExplorerDock->setWidget(treeView);

    // Dodavanje Dock-a
    addDockWidget(Qt::LeftDockWidgetArea,fileExplorerDock);
    setupCommitHistoryDockWidget();

    // Konektori
    connect(treeView, &QWidget::customContextMenuRequested, this, &MainWindow::onTreeViewContextMenu);
}

// Override metoda
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == treeView->viewport() && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        QModelIndex index = treeView->indexAt(mouseEvent->pos());

        if (!index.isValid()) // Ako nije kliknuto na validan index
        {
            treeView->clearSelection(); // Očistite selekciju
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onTreeViewContextMenu(const QPoint &point)
{
    if (repo == nullptr)
    {
        qDebug() << "Potrebno je napraviti repozitorijum";
        return;
    }
    QModelIndex index = treeView->indexAt(point);
    // if (!index.isValid())
    // {
    //     qDebug() << "Greska sa indeksom";
    //     return;
    // }

    QMenu contextMenu(this);

    QAction *addFolderAction = new QAction("Add New Folder", this);
    QAction *addFileAction = new QAction("Add New File", this);
    QAction *removeAction = new QAction("Remove", this);

    connect(addFolderAction, &QAction::triggered, this, &MainWindow::onAddNewFolder);
    connect(addFileAction, &QAction::triggered, this, &MainWindow::onAddNewFile);
    connect(removeAction, &QAction::triggered, this, &MainWindow::onRemoveFolderFile);

    contextMenu.addAction(addFolderAction);
    contextMenu.addAction(addFileAction);
    contextMenu.addAction(removeAction);

    if (index.isValid())
    {
        // Kliknuto je na folder ili fajl, dobija se path od tog direktorijuma
        QString dirPath = fileModel->filePath(index);
        QFileInfo fileInfo(dirPath);

        // if (fileInfo.isDir())
        // {
        //     // Postavlja root index na trenutni folder za novi fajl ili folder
        //     treeView->setRootIndex(fileModel->index(dirPath));
        // }
        // else
        // {
        //     // U slucaju da je to fajl koristi trenutni direktorijum
        //     treeView->setRootIndex(fileModel->index(fileInfo.absolutePath()));
        // }
    }
    else
    {
        // U slucaju da se klikne na prazno mesto to je root
        treeView->setRootIndex(fileModel->index(repo->getRepoPath()));
    }

    contextMenu.exec(treeView->mapToGlobal(point));
}

// Kreiranje novog foldera na desni klik u stablu prikaza
void MainWindow::onAddNewFolder()
{
    QModelIndex index = treeView->currentIndex();

    QString dirPath;
    if (!index.isValid() || !fileModel->isDir(index)) // Ako nije validan ili nije direktorijum
    {
        dirPath = repo->getRepoPath();
    }
    else
    {
        dirPath = fileModel->filePath(index);
    }

    if (dirPath == "/")
    {
        dirPath = repo->getRepoPath();
    }
    QDir dir(dirPath);

    if (!dir.exists())
    {
        QMessageBox::warning(this, "Error", "The specified directory does not exist.");
        qDebug() << "Direktorijum ne postoji:" << dirPath;
        return;
    }

    QString folderName = QInputDialog::getText(this, "New Folder", "Enter folder name: ");

    if (folderName.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Folder name cannot be empty.");
        return;
    }

    QString fullPath = dir.absoluteFilePath(folderName);

    if (dir.exists(folderName))
    {
        QMessageBox::warning(this, "Error", "A folder with this name already exists.");
        return;
    }

    if (dir.mkdir(folderName))
    {
        // Osvježite prikaz
        QModelIndex newFolderIndex = fileModel->index(fullPath);
        treeView->setCurrentIndex(newFolderIndex);
    }
    else
    {
        QMessageBox::warning(this, "Error", "Could not create the folder. Check the folder name and permissions.");
        qDebug() << "Nije uspelo pravljenje foldera:" << fullPath;
    }
}

// Brisanje foldera jedino ako nije pre toga commit!!!
void MainWindow::onRemoveFolderFile()
{
    QModelIndex index = treeView->currentIndex();

    if (!index.isValid())
    {
        QMessageBox::warning(this, "Error", "No item selected.");
        return;
    }

    QString itemPath = fileModel->filePath(index);

    if (!QFileInfo::exists(itemPath))
    {
        QMessageBox::warning(this, "Error", "The specified item does not exist.");
        qDebug() << "Stavka ne postoji:" << itemPath;
        return;
    }

    QString itemName = QFileInfo(itemPath).fileName();
    QString message = QString("Are you sure you want to delete '%1'?").arg(itemName);
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm Deletion", message,
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
    {
        return;
    }

    QDir dir(QFileInfo(itemPath).absolutePath());

    if (QFileInfo(itemPath).isDir())
    {
        if (dir.rmdir(QFileInfo(itemPath).fileName()))
        {
            qDebug() << "Folder je obrisan:" << itemPath;
        }
        else
        {
            QMessageBox::warning(this, "Error", "Could not delete the folder. Check permissions and if it is empty.");
            qDebug() << "Nije uspelo brisanje foldera:" << itemPath;
        }
    }
    else
    {
        if (QFile::remove(itemPath))
        {
            qDebug() << "Fajl je uspesno obrisan:" << itemPath;
        }
        else
        {
            QMessageBox::warning(this, "Error", "Could not delete the file. Check permissions.");
            qDebug() << "Nije uspelo brisanje fajla:" << itemPath;
        }
    }

    // Prikaz stabla cemo osveziti prvo
    QModelIndex parentIndex = index.parent();
    treeView->setCurrentIndex(parentIndex); // Nakon brisanja se vracamo na roditelja
    treeView->model()->removeRow(index.row(), index.parent()); // Tek onda ukloniti iz modela prikaza!
}



// Primena kod dodavanja novog fajla na desni klik
void MainWindow::onAddNewFile()
{
    QModelIndex index = treeView->currentIndex();

    QString dirPath;
    if (!index.isValid() || !fileModel->isDir(index)) // Proveravamo da li je selektovan validan direktorijum
    {
        dirPath = repo->getRepoPath(); // Ako nije, postavljamo korenski direktorijum
    }
    else
    {
        dirPath = fileModel->filePath(index);
    }

    if (dirPath == "/")
    {
        dirPath = repo->getRepoPath();
    }

    QDir dir(dirPath);

    if (!dir.exists())
    {
        QMessageBox::warning(this, "Error", "The specified directory does not exist.");
        qDebug() << "Direktorijum ne postoji:" << dirPath;
        return;
    }

    QString fileName = QInputDialog::getText(this, "New File", "Enter file name (with extension): ");

    if (fileName.isEmpty())
    {
        QMessageBox::warning(this, "Error", "File name cannot be empty.");
        return;
    }

    QString fullPath = dir.absoluteFilePath(fileName);

    if (QFile::exists(fullPath))
    {
        QMessageBox::warning(this, "Error", "A file with this name already exists.");
        return;
    }

    QFile file(fullPath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.close();
        QModelIndex newFileIndex = fileModel->index(fullPath);
        treeView->setCurrentIndex(newFileIndex);
    }
    else
    {
        QMessageBox::warning(this, "Error", "Could not create the file. Check the file name and permissions.");
        qDebug() << "Nije uspelo pravljenje fajla:" << fullPath;
    }
}



// Cuvanje promena commit-a, slot
void MainWindow::commitChanges()
{
    if (repo == nullptr)
    {
        qDebug() << "Morate prvo da otvorite repozitoijum pa tek onda da commit promene";
        return;
    }
    if (!savedFile)
    {
        if (askQuestion(this, "Save before Commit", "Do you want to save data before commit?"))
        {
            saveFile();
        }
    }
    QString author = QInputDialog::getText(this, "Author", "Enter author name: ");
    QString message = QInputDialog::getText(this, "Commit Message", "Enter commit message:");
    if (!author.isEmpty() && !message.isEmpty() && repo) {
        repo->createCommit(author, message);
        populateCommitHistory();
        QApplication::processEvents();
         savedFile = true;

    }
}

bool MainWindow::askQuestion(QWidget *parent, const QString &title, const QString &question) {
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(parent, title, question,
                                  QMessageBox::Yes | QMessageBox::No);
    return reply == QMessageBox::Yes;
}


// Metoda za kreiranje svih elemenata koji ce se nalaziti na menuBar-u
void MainWindow::createMenubar()
{
    QMenuBar *menuBar = new QMenuBar(this);
    this->setMenuBar(menuBar);

    // Meni
    QMenu *fileMenu = menuBar->addMenu("File");
    QMenu *editMenu = menuBar->addMenu("Edit");
    QMenu *viewMenu = menuBar->addMenu("View");
    QMenu *toolsMenu = menuBar->addMenu("Tools");
    QMenu *helpMenu = menuBar->addMenu("Help");

    // Akcije
    QAction *newProjectAction = new QAction("New Project..", this);
    QAction *openProjectAction = new QAction("Open Project...",this);
    QAction *saveFileAction = new QAction("Save File", this);
    QAction *saveStashChangesAction = new QAction("Save Stash Changes", this);
    QAction *loadStashAction = new QAction("Load Stash", this);
    QAction *deleteStashAction = new QAction("Delete Stash", this);
    QAction *exitAction = new QAction("Exit");

    QAction *undoAction = new QAction("Undo", this);
    QAction *redoAction = new QAction("Redo", this);
    QAction *cutAction = new QAction("Cut", this);
    QAction *copyAction = new QAction("Copy", this);
    QAction *pasteAction = new QAction("Paste", this);
    QAction *selectAction = new QAction("Select All", this);
    QAction *findAction = new QAction("Find...", this);
    QAction *preferencesAction = new QAction("Preferencess...", this);

    QAction *showLeftSidebarAction = new QAction("Show left Sidebar", this);
    showLeftSidebarAction->setCheckable(true);
    showLeftSidebarAction->setChecked(true);
    QAction *showRightSidebarAction = new QAction("Show right Sidebar", this);
    showRightSidebarAction->setCheckable(true);
    showRightSidebarAction->setChecked(true);
    QAction *showToolBarAction = new QAction("Show Bottom Toolbar", this);
    showToolBarAction->setCheckable(true);
    showToolBarAction->setChecked(true);

    QAction *commitAction = new QAction("Commit", this);
    QAction *pushAction = new QAction("Push data", this);
    QAction *pullAction = new QAction("Pull data", this);
    QAction *createBranchAction = new QAction("Create Branch",this);
    QAction *switchBranchAction = new QAction("Switch Branch",this);
    QAction *tagCommitAction = new QAction("Tag Commit",this);
    QAction *viewTagsAction = new QAction("View Tags",this);
    QAction *restoreAction = new QAction("Restore To Previous",this);
    QAction *mergeBranchAction = new QAction("Merge Branch",this);
    QAction *viewDifferencesAction = new QAction("View Differences",this);

    QAction *aboutProgramAction = new QAction("About IDGcode...");
    QAction *contactAction = new QAction("Contact...");


    // Postavljanje akcija
    fileMenu->addAction(newProjectAction);
    fileMenu->addAction(openProjectAction);
    fileMenu->addAction(saveFileAction);
    fileMenu->addAction(saveStashChangesAction);
    fileMenu->addAction(loadStashAction);
    fileMenu->addAction(deleteStashAction);
    fileMenu->addAction(exitAction);


    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addAction(cutAction);
    editMenu->addAction(copyAction);
    editMenu->addAction(pasteAction);
    editMenu->addAction(selectAction);
    editMenu->addAction(findAction);
    editMenu->addAction(preferencesAction);

    viewMenu->addAction(showLeftSidebarAction);
    viewMenu->addAction(showRightSidebarAction);
    viewMenu->addAction(showToolBarAction);

    toolsMenu->addAction(commitAction);
    toolsMenu->addAction(pushAction);
    toolsMenu->addAction(pullAction);
    toolsMenu->addAction(createBranchAction);
    toolsMenu->addAction(switchBranchAction);
    toolsMenu->addAction(tagCommitAction);
    toolsMenu->addAction(viewTagsAction);
    toolsMenu->addAction(restoreAction);
    toolsMenu->addAction(mergeBranchAction);
    toolsMenu->addAction(viewDifferencesAction);

    connect(commitAction, &QAction::triggered, this, &MainWindow::commitChanges);
    connect(pushAction, &QAction::triggered, this, &MainWindow::pushDataToServer);
    connect(pullAction, &QAction::triggered, this, &MainWindow::pullDataFromServer);
    connect(createBranchAction, &QAction::triggered, this, &MainWindow::createBranch);
    connect(switchBranchAction, &QAction::triggered, this, &MainWindow::switchBranch);
    connect(tagCommitAction, &QAction::triggered, this, &MainWindow::tagCommit);
    connect(viewTagsAction, &QAction::triggered, this, &MainWindow::viewTags);
    connect(restoreAction, &QAction::triggered, this, &MainWindow::restoreToPreviousCommit);
    connect(mergeBranchAction, &QAction::triggered, this, &MainWindow::mergeBranch);
    connect(viewDifferencesAction, &QAction::triggered, this, &MainWindow::compareCommits);



    helpMenu->addAction(aboutProgramAction);
    helpMenu->addAction(contactAction);

    // Konektori:
    connect(newProjectAction, &QAction::triggered, this, &MainWindow::newProject);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::openProject);
    connect(saveFileAction, &QAction::triggered, this, &MainWindow::saveFile);
    connect(saveStashChangesAction, &QAction::triggered, this, &MainWindow::stashChanges);
    connect(loadStashAction, &QAction::triggered, this, &MainWindow::applyStash);
    connect(deleteStashAction, &QAction::triggered, this, &MainWindow::discardStash);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExit);

    connect(undoAction, &QAction::triggered, [this]()
            {
                if (codeEditor)
                { codeEditor->undo(); }
            });

    connect(redoAction, &QAction::triggered, [this]()
            {
                if (codeEditor)
                { codeEditor->redo(); }
            });

    connect(cutAction, &QAction::triggered, [this]()
            {
                if (codeEditor)
                { codeEditor->cut(); }
            });

    connect(copyAction, &QAction::triggered, [this]()
            {
                if (codeEditor)
                { codeEditor->copy(); }
            });

    connect(pasteAction, &QAction::triggered, [this]()
            {
                if (codeEditor)
                { codeEditor->paste(); }
            });

    connect(selectAction, &QAction::triggered, [this]()
            {
                if (codeEditor)
                { codeEditor->selectAll(); }
            });
    connect(findAction, &QAction::triggered, this, &MainWindow::showFindMe);
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::showPreferencesDialog);



    connect(showLeftSidebarAction, &QAction::toggled, [this](bool checked) {
        if (checked) { fileExplorerDock->show(); }
        else {fileExplorerDock->hide(); }
    });

    connect(showRightSidebarAction, &QAction::toggled, [this](bool checked){
        if (checked) { commitHistoryDockWidget->show(); }
        else {commitHistoryDockWidget->hide(); }
    });

    connect(showToolBarAction, &QAction::toggled, [this](bool checked){
        if (checked) { toolBar->show(); }
        else {toolBar->hide(); }
    });


    connect(aboutProgramAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
    connect(contactAction, &QAction::triggered, this, &MainWindow::showContactDialog);
}

// Metoda za kreiranje svih elemenata koji ce se nalaziti na toolBar-u;
void MainWindow::createToolBar()
{
    toolBar = new QToolBar("Main Toolbar",this);
    this->addToolBar(Qt::BottomToolBarArea, toolBar);

    // Akcije
    QAction *commitAction = new QAction("Commit me",this);
    QAction *pushAction = new QAction("Push data", this);
    QAction *pullAction = new QAction("Pull data",this);
    QAction *createBranchAction = new QAction("Create Branch",this);
    QAction *switchBranchAction = new QAction("Switch Branch",this);
    QAction *tagCommitAction = new QAction("Tag Commit",this);
    QAction *viewTagsAction = new QAction("View Tags",this);
    QAction *restoreAction = new QAction("Restore To Previous",this);
    QAction *mergeBranchAction = new QAction("Merge Branch",this);
    QAction *viewDifferencesAction = new QAction("View Differences",this);
    QAction *viewGraphicallyAction = new QAction("View Graphics");

    // Postavljanje akcija
    toolBar->addAction(commitAction);
    toolBar->addAction(pushAction);
    toolBar->addAction(pullAction);
    toolBar->addAction(createBranchAction);
    toolBar->addAction(switchBranchAction);
    toolBar->addAction(tagCommitAction);
    toolBar->addAction(viewTagsAction);
    toolBar->addAction(restoreAction);
    toolBar->addAction(mergeBranchAction);
    toolBar->addAction(viewDifferencesAction);
    toolBar->addAction(viewGraphicallyAction);

    // Konektori
    connect(commitAction, &QAction::triggered, this, &MainWindow::commitChanges);
    connect(pushAction, &QAction::triggered, this, &MainWindow::pushDataToServer);
    connect(pullAction, &QAction::triggered, this, &MainWindow::pullDataFromServer);
    connect(createBranchAction, &QAction::triggered, this, &MainWindow::createBranch);
    connect(switchBranchAction, &QAction::triggered, this, &MainWindow::switchBranch);
    connect(tagCommitAction, &QAction::triggered, this, &MainWindow::tagCommit);
    connect(viewTagsAction, &QAction::triggered, this, &MainWindow::viewTags);
    connect(restoreAction, &QAction::triggered, this, &MainWindow::restoreToPreviousCommit);
    connect(mergeBranchAction, &QAction::triggered, this, &MainWindow::mergeBranch);
    connect(viewDifferencesAction, &QAction::triggered, this, &MainWindow::compareCommits);
    connect(viewGraphicallyAction, &QAction::triggered, this, [this]()
            {
        setupBranchGraphView();
        visualizeBranches();
    });

    connect(preferencesDialog, &PreferencesDialog::languageChanged, this, &MainWindow::updateLanguage);
    connect(preferencesDialog, &PreferencesDialog::fontChanged, this, &MainWindow::updateFont);
    connect(preferencesDialog, &PreferencesDialog::themeColorChanged, this, &MainWindow::updateThemeColor);
    connect(preferencesDialog, &PreferencesDialog::shortcutChanged, this, &MainWindow::updateShortcut);
}

void MainWindow::createStatusBar()
{
    QStatusBar *statusBar = new QStatusBar(this);
    this->setStatusBar(statusBar);
    statusBar->showMessage("Ready and on fire!");

}

void MainWindow::applyDarkTheme()
{
    // Promena palete za tamnu temu
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(45, 45, 48));
    darkPalette.setColor(QPalette::WindowText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Text, QColor(230, 230, 230));
    darkPalette.setColor(QPalette::Button, QColor(45, 45, 48));
    darkPalette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(38, 79, 120));
    darkPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));

    // Znaci da ce da se vidi na svim widgetima u aplikaciji!!!
    QApplication::setPalette(darkPalette);

    // Azuriranje za ostale widget-e (ovo mi je pravilo problem!!)
    // Kada postavljas theme, OBAVEZNO kastuj prvo u pokazivac, pa tek onda barataj sa njime
    // Inace ce doci do konflikta, na ovaj nacin ne moras da pazis gde pozivas applyTheme() da li pre inicijalizacije codeEditora ili posle
    CodeEditor *editor = qobject_cast<CodeEditor *>(centralWidget());
    if (editor) {
        editor->changeTheme("dark");
    }

    // Pronadji prvo toolbar pa onda postavi i stil
    QToolBar *toolBar = findChild<QToolBar *>("Main Toolbar");
    if (toolBar)
    {
        toolBar->setStyleSheet("QToolBar { background-color: #2D2D2D; color: #E0E2E4; }");
    }

    // Promena boje menija i status bara
    menuBar()->setStyleSheet("QMenuBar { background-color: #2D2D2D; color: #E0E2E4; }"
                             "QMenu { background-color: #2D2D2D; color: #E0E2E4; }"
                             "QMenu::item:selected { background-color: #3A3A3A; }");
    statusBar()->setStyleSheet("QStatusBar { background-color: #2D2D2D; color: #E0E2E4; }");
}


void MainWindow::onExit()
{
    QApplication::closeAllWindows();
    // QApplication::exit(0);
}

void MainWindow::showFindMe()
{
    findTextDialog->exec();
}

void MainWindow::findText(const QString &text)
{
    if (text.isEmpty())
    {
        return;
    }

    QTextCursor cursor = codeEditor->textCursor();
    QTextDocument *document = codeEditor->document();

    // Pocinje od vrha fajla
    cursor.setPosition(0);
    QTextCursor highlightCursor = document->find(text, cursor);

    // Brise sve prethodno oznacene (selektovane)
    QTextCursor highlightCursorReset = codeEditor->textCursor();
    highlightCursorReset.movePosition(QTextCursor::Start);
    codeEditor->setTextCursor(highlightCursorReset);
    codeEditor->setTextCursor(highlightCursorReset);

    // Proverava sada da li je doslo do podudaranja
    if (highlightCursor.isNull())
    {
        QMessageBox::information(this, "Find", "Text not found");
    }
    else
    {
        QTextCursor highlightCursor = document->find(text, cursor);
        QTextCursor cursorStart = codeEditor->textCursor();
        cursorStart.movePosition(QTextCursor::Start);
        codeEditor->setTextCursor(cursorStart);

        // Trebalo bi sada da selektuje sav nadjen tekst
        QTextCursor cursorFind = document->find(text, cursorStart);
        while (!cursorFind.isNull())
        {
            cursorFind.movePosition(QTextCursor::WordRight, QTextCursor::KeepAnchor);
            cursorFind = document->find(text, cursorFind);
        }

        // Vrati cursor na prvi pronadjeni
        codeEditor->setTextCursor(highlightCursor);
        codeEditor->ensureCursorVisible();
    }
}

// Konekcija sa serverom i slanje posledjeg commit-a na server
void MainWindow::pushDataToServer()
{
    if (repo == nullptr)
    {
        qDebug() << "Potrebno je otvoriti repozitorijum koji zelite da posaljete na server";
        return;
    }
    if (repo->sendCommitToServer(repo->getLastCommit()))
    {
        QMessageBox::information(this,"Push Data To Server", "Successfully pushed latest commits");
    }
    else
    {
        QMessageBox::critical(this, "Push Data To Server", "Error when sending data");
    }
}

void MainWindow::pullDataFromServer()
{
    QStringList repositories = repo->requestRepositoryList();
    QString selectedRepo = QInputDialog::getItem(this, "List of Repositories", "Select Repository:",repositories, false);
    if (repo->pullRepositoryFromServer(selectedRepo))
    {
        QMessageBox::information(this, "Pull Data From Server", "Successfully stored data from server");
    }
    else
    {
        QMessageBox::critical(this, "Push Data To Server", "Error when sending data");
    }
}

// Kreiranje nove grane
void MainWindow::createBranch()
{
    QString branchName = QInputDialog::getText(this, "Create Branch", "Enter branch name:");
    if (!branchName.isEmpty() && repo)
    {
        repo->createBranch(branchName);
    }
}

// Promena grane
void MainWindow::switchBranch()
{
    if (!repo) {
        qDebug() << "Nije ni jedan repozitorijum ucitan kako bi promenili granu";
        return;
    }

    QStringList branches = repo->getBranches();
    bool ok;
    QString branchName = QInputDialog::getItem(this, "Switch Branch", "Select branch:", branches, 0, false, &ok);

    if (ok && !branchName.isEmpty()) {
        repo->switchBranch(branchName); // Menjamo sada trenutnu granu na selektovanu granu

        QString lastCommitHash = repo->getLastCommitHash();
        if (lastCommitHash.isEmpty()) {
            qDebug() << "Nisu pronadjeni commit-ovi za ovu granu.";
            return;
        }

        // Prvo ocistimo trenutni sadrzaj
        repo->clearCurrentDirectory();
        populateCommitHistory();

        // Vracamo stanje poslednjeg commita za ovu granu
        QApplication::setOverrideCursor(Qt::WaitCursor);
        RestoreWorker* worker = new RestoreWorker(repo, repo->getLastCommitHash());
        QThread* thread = new QThread;
        worker->moveToThread(thread);

        connect(thread, &QThread::started, worker, &RestoreWorker::process);
        connect(worker, &RestoreWorker::finished, this, [=]() {
            QApplication::restoreOverrideCursor();
            thread->quit();
            worker->deleteLater();
            thread->deleteLater();

            treeView->setRootIndex(fileModel->index(repo->getRepoPath()));
            codeEditor->clear();
            currentFilePath.clear();
        });

        // Start the thread
        thread->start();

        // Osvezavamo file explorer sa trenutnim sadrzajem

    }
}

// Kreiranje taga
void MainWindow::tagCommit()
{
    if (repo == nullptr)
    {
        qDebug() << "Nije ni jedan repozitorijum ucitan kako bi prineli tag";
        return;
    }

    QString commitHash = QInputDialog::getText(this, "Tag Commit", "Enter commit hash:");
    QString tagName = QInputDialog::getText(this, "Tag Name", "Enter tag name:");

    if (!commitHash.isEmpty() && !tagName.isEmpty())
    {
        bool success = repo->tagCommit(commitHash, tagName);
        if (success)
        {
            qDebug() << "Tag" << tagName << "je uspesno kreiran za commit: " << commitHash;

            commitModel->clear();
            populateCommitHistory();
            commitHistoryView->reset();
            commitHistoryView->update();

        }
        else
        {
            qDebug() << "Nije uspelo da se taguje commit.";
        }
    }
}


// Izlistavanje liste tagova
void MainWindow::viewTags()
{
    if (repo == nullptr)
    {
        qDebug() << "Nije ni jedan repozitorijum ucitan kako bi pogledali tag";
        return;
    }

    QStringList tags = repo->getTags();
    if (tags.isEmpty())
    {
        QMessageBox::information(this, "Tags", "No tags found.");
        return;
    }

    bool ok;
    QString selectedTag = QInputDialog::getItem(this, "View Tags", "Select a tag:", tags, 0, false, &ok);

    if (ok && !selectedTag.isEmpty())
    {
        QStringList splity = selectedTag.split(":");
        QString commitHash = repo->getCommitHashForTag(splity[0]);
        if (!commitHash.isEmpty())
        {

            // Prikaz svih sada informacija za selektovani tag
            QMap<QString, QString> commitContent = repo->loadCommitContent(commitHash);
            QString commitMessage = repo->getCommitMessage(commitHash);
            QString commitAuthor = repo->getCommitAuthor(commitHash);

            QString tagDetails = QString("Tag: %1\nCommit Hash: %2\nAuthor: %3\nMessage: %4\n\nFiles:\n")
                                     .arg(selectedTag)
                                     .arg(commitHash)
                                     .arg(commitAuthor)
                                     .arg(commitMessage);

            for (const QString &fileName : commitContent.keys())
            {
                tagDetails += QString("File: %1\n").arg(fileName);
            }

            QMessageBox::information(this, "Tag Details", tagDetails);
        }
    }
}

// Vracanje na prethodnu jednu verziju
void MainWindow::restoreToPreviousCommit()
{
    if (repo == nullptr)
    {
        qDebug() << "Kreiraj prvo repozitorijum ili otvori!";
        return;
    }
    repo->clearCurrentDirectory();
    repo->restoreToCommit(repo->getSecondLastCommitHash());
    treeView->setRootIndex(fileModel->index(repo->getRepoPath()));
    codeEditor->clear();
    currentFilePath.clear();
}

// Spajanje grana
void MainWindow::mergeBranch()
{
    if (repo == nullptr)
    {
        qDebug() << "Potrebno je otvoriti repozitorijum da bi mogli da spajate grane";
        return;
    }

    QStringList branches = repo->getBranches();
    bool ok;
    QString targetBranch = QInputDialog::getItem(this, "Merge Branch", "Select target branch:", branches, 0, false, &ok);

    if (ok && !targetBranch.isEmpty())
    {
        QString currentBranch = repo->getCurrentBranch();
        if (currentBranch == targetBranch)
        {
            QMessageBox::warning(this, "Merge Branch", "You cannot merge a branch into itself.");
            return;
        }

        QThread* mergeThread = new QThread;
        MergeWorker* worker = new MergeWorker(repo, currentBranch, targetBranch);
        worker->moveToThread(mergeThread);

        connect(mergeThread, &QThread::started, worker, &MergeWorker::process);
        connect(worker, &MergeWorker::finished, this, [this, mergeThread](bool success)
                {
                    if (success)
                    {

                        QMessageBox::information(this, "Merge Successful", "Successfully merged branches.");

                        // repo->restoreToCommit(repo->getLastCommitHash());
                        // currentFilePath.clear();
                        codeEditor->clear();
                        populateCommitHistory();
                        setupBranchGraphView();
                        visualizeBranches();
                    }
                    else
                    {
                        QMessageBox::warning(this, "Merge Failed", "Merge failed due to conflicts or other issues.");
                    }
                    mergeThread->quit();
                });
        connect(mergeThread, &QThread::finished, worker, &MergeWorker::deleteLater);
        connect(mergeThread, &QThread::finished, mergeThread, &QThread::deleteLater);

        mergeThread->start();
    }
    else
    {
        qDebug() << "Prazan je target branch ili korisnik nije uneo ispravno branch!";
        return;
    }
}

void MainWindow::setupBranchGraphView() {
    // Create a new QMainWindow for the branch graph view
    QMainWindow* branchGraphWindow = new QMainWindow(this);

    branchGraphView = new QGraphicsView(this);
    branchGraphScene = new QGraphicsScene(this);

    branchGraphWindow->setWindowTitle("Branch Graph");

    // Set up the QGraphicsView and QGraphicsScene
    branchGraphView->setScene(branchGraphScene);
    branchGraphView->setRenderHint(QPainter::Antialiasing);

    // Set the QGraphicsView as the central widget of the new window
    branchGraphWindow->setCentralWidget(branchGraphView);

    // Show the new window
    branchGraphWindow->resize(800, 600); // Adjust the size as needed
    branchGraphWindow->show();
}


void MainWindow::visualizeBranches() {
    if (!repo) {
        qDebug() << "Nije otvoren repozitorijum da bi videli prikaz grana";
        return;
    }

    branchGraphScene->clear(); // Clear previous graph if any

    QMap<QString, QVector<QString>> branchCommits = repo->getBranchCommits();

    int x = 50; // X-coordinate for drawing
    int y = 50; // Y-coordinate for drawing

    QPen branchPen(Qt::yellow, 2);
    QPen commitPen(Qt::white, 2);
    QBrush commitBrush(QColor::fromRgb(46,52,64));

    // Iteracija preko mape branchCommit
    for (auto branchIt = branchCommits.begin(); branchIt != branchCommits.end(); ++branchIt) {
        QString branchName = branchIt.key();
        QVector<QString> commits = branchIt.value();

        // Promenimo font za commit
        QGraphicsTextItem *branchNameItem = branchGraphScene->addText(branchName);
        QFont branchFont = branchNameItem->font();
        branchFont.setBold(true);
        branchFont.setPointSize(12);
        branchNameItem->setFont(branchFont);
        branchNameItem->setPos(x, y);

        y += 40; // Dodajemo prosor izmedju

        // Crtanje kruga oko svakog commit-a
        for (const QString &commit : commits) {
            QGraphicsEllipseItem *commitNode = branchGraphScene->addEllipse(x, y, 30, 30, commitPen, commitBrush);

            QString commitTagText = repo->getCommitTagForHash(commit);
            if (commitTagText.isEmpty())
            {
                // Skracena verzija hash-a se prikazuje
                commitTagText = commit.mid(0,8);
            }
            QGraphicsTextItem *commitText = branchGraphScene->addText(commitTagText);
            commitText->setPos(x + 15, y + 35);

            // U slucaju da je ista grana onda se povezuje linijom
            if (y > 90) {
                branchGraphScene->addLine(x + 15, y - 30, x + 15, y, branchPen);
            }

            y += 60; // Prosor kod svakog commit-a
        }

        x += 200;
        y = 50;
    }
}

// Poredjenje commit-a
void MainWindow::compareCommits()
{
    if (repo == nullptr)
    {
        qDebug() << "Potrebno je prvo kreirati ili otvoriti repozitorijum";
        return;
    }

    QString lastCommitHash = repo->getLastCommitHash();
    QString secondLastCommitHash = repo->getSecondLastCommitHash();

    // Poredjenje cemo obaviti na posebnom thread-u kako bi thread od UI-a ostavili da nesmetano radi
    QThread* comparisonThread = new QThread;
    CompareWorker* worker = new CompareWorker(repo, lastCommitHash, secondLastCommitHash);
    worker->moveToThread(comparisonThread);

    connect(comparisonThread, &QThread::started, worker, &CompareWorker::process);
    connect(worker, &CompareWorker::displayDiff, this, &MainWindow::showDiff);
    connect(worker, &CompareWorker::finished, comparisonThread, &QThread::quit);
    connect(worker, &CompareWorker::finished, worker, &CompareWorker::deleteLater);
    connect(comparisonThread, &QThread::finished, comparisonThread, &QThread::deleteLater);



    comparisonThread->start();
}

// Prozor za prikaz razlika
void MainWindow::showDiff(const QString &fileName, const QString &oldContent, const QString &newContent) {
    QTextEdit *diffViewer = new QTextEdit;
    diffViewer->setWindowTitle("Differences in " + fileName);
    diffViewer->setReadOnly(true);

    diffViewer->setStyleSheet("QTextEdit {"
                              "  background-color: #f4f4f4;"
                              "  color: #757555;"
                              "  font-family: 'Consolas', 'Courier New', monospace;"
                              "  font-size: 11pt;"
                              "  border-radius: 8px;"
                              "  padding: 10px;"
                              "  border: 1px solid #cccccc;"
                              "  selection-background-color: #d0d0d0;"
                              "}");

    QString diffText;
    QStringList oldLines = oldContent.split('\n');
    QStringList newLines = newContent.split('\n');
    int maxLines = qMax(oldLines.size(), newLines.size());

    diffText += "<div style='display: flex; justify-content: center; align-items: center; height: 100%;'>";
    diffText += "<table style='width: 80%; height: auto; border-spacing: 0; border-collapse: collapse;'>";

    for (int i = 0; i < maxLines; ++i) {
        QString oldLine = i < oldLines.size() ? oldLines[i] : "";
        QString newLine = i < newLines.size() ? newLines[i] : "";

        QString lineNumber = QString::number(i + 1);
        QString lineStyle = "padding: 10px 10px;";

        if (oldLine != newLine) {
            diffText += "<tr>";
            diffText += "<td style='" + lineStyle + " background-color: #ffcccc; color: #a94442;'>" + lineNumber + "</td>";
            diffText += "<td style='" + lineStyle + " background-color: #ffcccc;'><b>- " + oldLine.toHtmlEscaped() + "</b></td>";
            diffText += "</tr>";
            diffText += "<tr>";
            diffText += "<td style='" + lineStyle + " background-color: #ccffcc; color: #3c763d;'>" + lineNumber + "</td>";
            diffText += "<td style='" + lineStyle + " background-color: #ccffcc;'><b>+ " + newLine.toHtmlEscaped() + "</b></td>";
            diffText += "</tr>";
        } else {
            diffText += "<tr>";
            diffText += "<td style='" + lineStyle + " background-color: #e0e0e0; color: #555555;'>" + lineNumber + "</td>";
            diffText += "<td style='" + lineStyle + " background-color: #ffffff;'>" + oldLine.toHtmlEscaped() + "</td>";
            diffText += "</tr>";
        }
    }

    diffText += "</table>";
    diffText += "</div>";

    qDebug() << "Diff viewer created";
    diffViewer->setHtml(diffText);
    diffViewer->resize(1000, 800);
    diffViewer->show();
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!currentFilePath.isEmpty())
    {
        if (!savedFile)
        {
            if (askQuestion(this,"Save current file", "Save the current file before exiting?"))
            {
                saveFile();
                event->ignore();
            }
            else
            {
                event->accept();
            }
        }

    }
}

void MainWindow::applyLightTheme()
{
     QApplication::setPalette(QApplication::style()->standardPalette());

}

// Kreiranje stash podataka, cuvanje podataka bez commit-a
void MainWindow::stashChanges() {
    if (repo) {
        repo->stashChanges();
        qDebug() << "Sacuvani su stash podaci.";
         savedFile = true;
    } else {
        qDebug() << "Repozitorijum nije inicijalizovan.";
    }
}


void MainWindow::applyStash() {
    if (repo)
    {
        repo->applyStash();
        qDebug() << "Prikazani su stash podaci.";
        // Osvezavanje prikaza
        treeView->setRootIndex(fileModel->index(repo->getRepoPath()));
    }
    else
    {
        qDebug() << "Repozitorijum nije inicijalizovan.";
    }
}

void MainWindow::discardStash()
{
    if (repo)
    {
        if (repo->hasStash())
        {
            repo->discardStash();
        }
        else
        {
            QMessageBox::information(this, "Discard Stash", "No stash found to discard.");
        }
    }
    else
    {
        qDebug() << "Nije otvoren repozitorijum";
        return;
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, "About IDGcode",
                       "IDGcode is a powerful version control system built using Qt 6, "
                       "designed to manage and track changes in your codebase effectively. "
                       "With features inspired by Git, IDGcode allows you to create repositories, "
                       "manage branches, track versions, and much, much more.");
}

void MainWindow::showContactDialog()
{
    QMessageBox::information(this, "Contact",
                             "For any inquiries or support, please contact us at:\n\n"
                             "Email: support@idgNocode.com\n"
                             "Phone: +381 000 911\n"
                             "Website: www.idgNocode.com");
}

void MainWindow::showPreferencesDialog()
{
    preferencesDialog->exec();
}

void MainWindow::updateLanguage(const QString &language)
{
    // TODO namesti i tr() za svaki element
    if (language == "Serbian")
    {
        // TODO fajl sa prevdom dodati u sledecoj verziji i za svaki element mora ispred tr()
    }
    else if (language == "English")
    {
        // TODO
    }
}

void MainWindow::updateFont(const QFont &font)
{
    codeEditor->setFont(font);
}

void MainWindow::updateThemeColor(const QColor &color)
{
    if (QColor(0,0,0) == color)
    {
        // Znaci da je dark mod
        // Potrebno je i invertovati slova
        codeEditor->changeTheme("dark");
    }
    if (QColor(255,255,255) == color)
    {
        // U pitanju je light mod i onda slova treba da budu crna
        codeEditor->changeTheme("light");
    }
    else
    {
        codeEditor->changeBackground(color);
    }
}

// Kao argument metoda prima akciju i tip akcije, tj. tacna kombinacija tastera
void MainWindow::updateShortcut(const QString &action, const QKeySequence &shortcut)
{
    if (action == "Undo")
    {
        codeEditor->setUndoShortcut(shortcut);
    }
    else if (action == "Copy")
    {
        codeEditor->setCopyShortcut(shortcut);
    }
}
