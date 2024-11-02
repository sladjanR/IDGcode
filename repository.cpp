#include "repository.hpp"
#include "tag.hpp"

// Qt
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QTcpSocket>
#include <QFileInfoList>
#include <QFileDialog>
#include <QProcess>
#include <QQueue>


// Konstruktor naseg repozitorijuma, kao argumente prosledjujemo putanju direktorijuma, tag-ova i stash-a
Repository::Repository(const QString &rootPath) :
    rootPath{rootPath},
    tagsFilePath{rootPath + "/.tags"},
    stashFilePath{rootPath + "/.stash"}
{
    loadBranchInfo();
    loadTags();
}

// Dodavanje fajlova postojecem stablu
void Repository::addFilesToTree(Tree &tree, const QDir &dir)
{

    QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Dirs);

    for (const QFileInfo &fileInfo : fileList)
    {
        if (fileInfo.isFile())
        {
            Blob blob(fileInfo.filePath(), rootPath);
            tree.addBlob(fileInfo.fileName(), blob);
        }
        else if (fileInfo.isDir())
        {
            // OVDE SE ZNACI KREIRA JOS JEDAN FAJL (u .trees)
            Tree subTree;
            addFilesToTree(subTree, QDir(fileInfo.filePath()));

            // Ali potrebno je sacuvati i poddirektorijume
            subTree.saveToDisk(rootPath);

            // Dodavanje subdirektorijumskog (podstablo) hash-a na main stablo
            tree.addTree(fileInfo.fileName(), subTree);
        }
    }
}


void Repository::saveCommitLocally(const Commit &commit)
{
    if (!QDir(rootPath + "/.commits").exists())
    {
        QDir().mkdir(rootPath + "/.commits");
    }

    QDir commitDir(rootPath + "/.commits");
    QString commitFileName = commitDir.filePath("commit_" + commit.getHash() + ".json");
    QFile commitFile(commitFileName);

    if (commitFile.open(QIODevice::WriteOnly))
    {
        // QJsonObject commitData;
        // commitData["commitHash"] = commit.getHash();
        // commitData["treeHash"] = commit.getTreeHash();
        // commitData["author"] = commit.getAuthor();
        // commitData["message"] = commit.getMessage();
        // commitData["parentHash"] = commit.getParentHash();
        // commitData["timestamp"] = QString::number(commit.getTimestamp());
        // commitData["branch"] = currentBranch;

        QJsonDocument doc(commit.toJson());
        commitFile.write(doc.toJson());
        commitFile.close();

        // Save the root tree to disk
        commit.getTree().saveToDisk(rootPath);
    }
    else
    {
        qDebug() << "Neuspelo cuvanje commit-a lokalno";
    }
}



QString Repository::getLastCommitHash() const
{
    return getNthLastCommitForBranch(currentBranch, 1);
}

// TODO: ovde je greska sigurno 1
QMap<QString, QString> Repository::loadCommitContent(const QString &commitHash) const
{
    QMap<QString, QString> contentMap;

    // Učitavanje commit fajla
    QString commitFilePath = rootPath + "/.commits/commit_" + commitHash + ".json";
    QFile commitFile(commitFilePath);

    if (commitFile.open(QIODevice::ReadOnly))
    {
        QByteArray commitData = commitFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(commitData);
        QJsonObject jsonObj = doc.object();

        // Uzimamo hash glavnog stabla iz commita
        QString treeHash = jsonObj["treeHash"].toString();

        // Rekurzivno ucitavamo sadrzaj stabla
        loadTreeContent(treeHash, "", contentMap);
    }

    qDebug() << "Ovako izgleda:" << contentMap.keys() << "\n\n";
    return contentMap;
}

void Repository::loadTreeContent(const QString &treeHash, const QString &currentPath, QMap<QString, QString> &contentMap) const
{
    // Ucitavanje tree fajla
    QString treeFilePath = rootPath + "/.trees/tree_" + treeHash + ".json";
    QFile treeFile(treeFilePath);

    if (treeFile.open(QIODevice::ReadOnly))
    {
        QByteArray treeData = treeFile.readAll();
        QJsonDocument treeDoc = QJsonDocument::fromJson(treeData);
        QJsonObject treeObj = treeDoc.object();

        // Obrada blobs (fajlova) u trenutnom stablu
        QJsonObject blobs = treeObj["blobs"].toObject();
        for (const QString &fileName : blobs.keys())
        {
            QString blobHash = blobs[fileName].toString();
            QString fullPath = currentPath.isEmpty() ? fileName : currentPath + "/" + fileName;

            // Ucitavanje sadržaja iz blob-a i dodavanje u mapu
            contentMap[fullPath] = loadBlobContent(blobHash);
        }

        // Obrada podstabala (direktorijuma) u tekucem stablu
        QJsonObject trees = treeObj["trees"].toObject();
        for (const QString &dirName : trees.keys())
        {
            QString subtreeHash = trees[dirName].toString();
            QString newPath = currentPath.isEmpty() ? dirName : currentPath + "/" + dirName;

            // Rekurzivni poziv za ucitavanje  podstabla
            loadTreeContent(subtreeHash, newPath, contentMap);
        }
    }
    else
    {
        qWarning() << "Neuspelo otvaranje tree fajla:" << treeFilePath;
    }
}


QString Repository::loadBlobContent(const QString &blobHash) const
{
    Blob blob = Blob::fromDisk(blobHash, rootPath);

    return QString::fromUtf8(blob.getContent());
}

// Kreiranje commit-a sa autorom i porukom koji se prosledjuju kao argument
Commit Repository::createCommit(const QString &author, const QString &message)
{
    Tree projectTree;
    QDir rootDir(rootPath);

    addFilesToTree(projectTree, rootPath);

    QString lastCommitHash = getLastCommitHash();
    qDebug() << "Ovako izgleda projectTree: ";
    qDebug() << projectTree.toJson();
    Commit commit(message, projectTree, author, lastCommitHash, currentBranch);

    lastCommit = commit;
    saveCommitLocally(commit);

    return commit;
}

// Slanje commit-a posledjeg na server
bool Repository::sendCommitToServer(const Commit &commit)
{
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", 9000);

    if (socket.waitForConnected())
    {
        // Pripremanje commit podataka
        QJsonObject commitData;
        commitData["commitHash"] = commit.getHash();
        commitData["treeHash"] = commit.getTreeHash();
        commitData["author"] = commit.getAuthor();
        commitData["message"] = commit.getMessage();
        commitData["parentHash"] = commit.getParentHash();
        commitData["timestamp"] = QString::number(commit.getTimestamp());
        commitData["branch"] = currentBranch;

        QJsonObject commitObj;
        commitObj["action"] = "commit";
        commitObj["commit"] = commitData;
        commitObj["repositoryName"] = QFileInfo(rootPath).fileName();

        QJsonDocument commitDoc(commitObj);
        QByteArray commitDataBytes = commitDoc.toJson();
        socket.write(commitDataBytes);

        if (!socket.waitForBytesWritten())
        {
            qDebug() << "Nije uspelo upisivanje podataka na server";
            return false;
        }

        if (!socket.waitForReadyRead())
        {
            qDebug() << "Nema odgovora od servera nakon slanja podataka";
            return false;
        }

        QByteArray commitResponse = socket.readAll();
        qDebug() << "Odgovor servera na commit: " << commitResponse;

        socket.disconnectFromHost();
    }
    else
    {
        qDebug() << "Nije uspela konekcija sa serverom...";
        return false;
    }

    // Saljemo bas sva stabla koja imaju veze sa poslednjim commitom
    QStringList treeHashes = collectTreeHashes(commit.getTreeHash());
    for (const QString& treeHash : treeHashes)
    {
        QTcpSocket treeSocket;
        treeSocket.connectToHost("127.0.0.1", 9000);

        if (treeSocket.waitForConnected())
        {
            Tree tree = Tree::fromDisk(treeHash, rootPath);

            QJsonObject treeObj;
            treeObj["action"] = "storeTree";
            treeObj["repositoryName"] = QFileInfo(rootPath).fileName();
            treeObj["treeHash"] = treeHash;
            treeObj["treeData"] = tree.toJson();

            QJsonDocument treeDoc(treeObj);
            QByteArray treeDataBytes = treeDoc.toJson();
            treeSocket.write(treeDataBytes);

            if (!treeSocket.waitForBytesWritten())
            {
                qDebug() << "Neuspelo pisanje informacija o stablu na server";
                return false;
            }

            if (!treeSocket.waitForReadyRead())
            {
                qDebug() << "Nema odgovora od servera nakon slanja podataka";
                return false;
            }

            QByteArray treeResponse = treeSocket.readAll();
            qDebug() << "Odgovor servera na poslato stablo: " << treeResponse;

            treeSocket.disconnectFromHost();
        }
        else
        {
            qDebug() << "Neuspela konekcija sa serverom radi razmene stabla.";
            return false;
        }
    }

    // Saljemo bas sve blobove koji imaju veze sa commit-om
    for (const QString& treeHash : treeHashes)
    {
        Tree tree = Tree::fromDisk(treeHash, rootPath);

        for (const auto& blobEntry : tree.getBlobs())
        {
            QTcpSocket blobSocket;
            blobSocket.connectToHost("127.0.0.1", 9000);

            if (blobSocket.waitForConnected())
            {
                Blob blob = Blob::fromDisk(blobEntry, rootPath);

                QJsonObject blobObj;
                blobObj["action"] = "storeBlob";
                blobObj["repositoryName"] = QFileInfo(rootPath).fileName();
                blobObj["blobHash"] = blob.getHash();
                blobObj["content"] = QString(blob.getContent().toBase64());

                QJsonDocument blobDoc(blobObj);
                QByteArray blobData = blobDoc.toJson();
                blobSocket.write(blobData);

                if (!blobSocket.waitForBytesWritten())
                {
                    qDebug() << "Nije uspelo slanje blobova na server";
                    return false;
                }

                if (!blobSocket.waitForReadyRead())
                {
                    qDebug() << "Nema odgovora nakon slanja blob-a";
                    return false;
                }

                QByteArray blobResponse = blobSocket.readAll();
                qDebug() << "Odgovor servera na blob: " << blobResponse;

                blobSocket.disconnectFromHost();
            }
            else
            {
                qDebug() << "Nije uspela konekcija na server i prenos blob-a.";
                return false;
            }
        }
    }

    return true;
}

// Sakupljamo sve hash vrednosti kojima tree raspolaze
QStringList Repository::collectTreeHashes(const QString& rootTreeHash)
{
    QStringList treeHashes;
    // Podatke cemo stavljati u red (queue)
    QQueue<QString> queue;
    queue.enqueue(rootTreeHash);

    while (!queue.isEmpty())
    {
        QString currentHash = queue.dequeue();
        treeHashes.append(currentHash);

        Tree currentTree = Tree::fromDisk(currentHash, rootPath);
        for (const QString& subTreeHash : currentTree.getTrees().values())
        {
            if (!treeHashes.contains(subTreeHash))
            {
                // Dodaje na krad reda elemente (isto kao append)
                queue.enqueue(subTreeHash);
            }
        }
    }

    return treeHashes;
}

// Kreiranje nove grane
void Repository::createBranch(const QString &branchName)
{
    branches.append(branchName);
    saveBranchInfo();   // Cuvanje azuriranih informacija

    // Prebaciti se na novi branch
    switchBranch(branchName);

    // Automatski commit kada se napravi novi branch
    QString author = "Auto Commit";
    QString message = "Initial commit for new branch";
    Commit newCommit = createCommit(author, message);

    // Cuvanje commit-a lokalno
    saveCommitLocally(newCommit);
}

// Menjanje grana
void Repository::switchBranch(const QString &branchName)
{
    if (!branches.contains(branchName))
    {
        qDebug() << "Branch" << branchName << " ne postoji";
        return;
    }

    currentBranch = branchName;
    saveBranchInfo();   // Cuvanje trenutnog branch-a u .current_branch
    qDebug() << "Promena branch-a je bila uspesna";
}

// Vracanje trenutno aktivne grane
QString Repository::getCurrentBranch() const
{
    return currentBranch;
}

// Vracanje svih grana
QStringList Repository::getBranches() const
{
    return branches;
}

// Vracanje na odredjeni commit (kao argument saljemo koji tacno)
void Repository::restoreToCommit(const QString &commitHash)
{
    QString commitFilePath = rootPath + "/.commits/commit_" + commitHash + ".json";
    QFile commitFile(commitFilePath);

    if (commitFile.open(QIODevice::ReadOnly))
    {
        QByteArray commitData = commitFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(commitData);
        QJsonObject jsonObj = doc.object();
        QString treeHash = jsonObj["treeHash"].toString();

        // Load the root tree from disk
        Tree rootTree = Tree::fromDisk(treeHash, rootPath);

        qDebug() << "Pre restoreTree\n";
        restoreTree(rootTree, rootPath);
    }
    else
    {
        qDebug() << "Nije uspelo vraćanje prethodnog sadržaja: Commit fajl nije pronađen";
    }
}

// Proces vracanja svih stabala za pruzeni path i glavno stablo
void Repository::restoreTree(const Tree &tree, const QString &currentPath)
{
    for (const QString &blobName : tree.getBlobs().keys())
    {
        QString filePath = currentPath + "/" + blobName;
        QString blobHash = tree.getBlobs().value(blobName);
        QString blobContent = loadBlobContent(blobHash);

        QFile file(filePath);
        QFileInfo fileInfo(filePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        if (file.open(QIODevice::WriteOnly))
        {
            file.write(blobContent.toUtf8());
            file.close();
        }
        else
        {
            qDebug() << "Nije uspelo kreiranje datoteke: " << filePath;
        }
    }

    for (const QString &treeName : tree.getTrees().keys())
    {
        QString subdirPath = currentPath + "/" + treeName;
        QDir dir(subdirPath);
        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        QString subTreeHash = tree.getTrees().value(treeName);
        Tree subTree = Tree::fromDisk(subTreeHash, rootPath);

        restoreTree(subTree, subdirPath);
    }
}

QString Repository::getRepoPath() const
{
    return rootPath;
}

QString Repository::getSecondLastCommitHash() const
{
    return getNthLastCommitForBranch(currentBranch, 2);
}

// Vraca commit za branch koji prosledimo (1-poslednji, 2 pretposlednji...)
QString Repository::getNthLastCommitForBranch(const QString &branchName, int n) const
{
    QDir commitDir(rootPath + "/.commits");
    QStringList commitFiles = commitDir.entryList(QDir::Files, QDir::Time);

    // Struktura za cuvanje commitova odredjenih timestamp-ova
    QVector<QPair<QString, qint64>> commitsWithTimestamps;

    for (const QString &commitFile : commitFiles)
    {
        QString commitFilePath = commitDir.filePath(commitFile);
        QFile file(commitFilePath);
        if (file.open(QIODevice::ReadOnly))
        {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            if (obj["branch"].toString() == branchName)
            {
                QString commitHash = obj["commitHash"].toString();
                qint64 timestamp = obj["timestamp"].toVariant().toLongLong();
                commitsWithTimestamps.append(qMakePair(commitHash, timestamp));
            }
        }
    }

    // Sortiranje commit-ova po timestamp-u (sort iz standardne biblioteke)
    std::sort(commitsWithTimestamps.begin(), commitsWithTimestamps.end(),
              [](const QPair<QString, qint64> &a, const QPair<QString, qint64> &b) {
                  return a.second > b.second;   // .second je vrednost timespama (first je ime commit-a)
              });

    // Vraca n-ti najnoviji commit u slucaju postoji
    if (commitsWithTimestamps.size() >= n)
    {
        return commitsWithTimestamps[n - 1].first;
    }
    else
    {
        return QString();
    }
}

void Repository::clearCurrentDirectory()
{
    QDir dir(rootPath);
    QStringList excludedDirs = { ".commits", ".blobs", ".current_branch", ".tags" , ".trees"};    // Brise sve osim ovih direktorijuma

    // Prvo uklanjamo sve fajlove u rootpath osim gore navedenih
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &info : entries)
    {
        QFile::remove(info.absoluteFilePath());
    }

    // Zatim uklanjamo sve foldere osim onih excluded
    entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &info : entries)
    {
        if (!excludedDirs.contains(info.fileName()))
        {
            QDir subDir(info.absoluteFilePath());
            subDir.removeRecursively();
        }
    }
}

void Repository::addTag(const QString &label, const QString &commitHash)
{
    tags.append(Tag(label, commitHash));
    saveTags();
}

QStringList Repository::getTags() const
{
    QStringList temp_tags;

    QFile file(rootPath + "/.tags");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        while (!in.atEnd())
        {
            temp_tags.append(in.readLine());
        }
    }

    return temp_tags;
}

// Cuvanje tagova koji dodeljujemo za odredjeni commit
void Repository::saveTags()
{
    QFile file(tagsFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        for (const Tag &tag : tags)
        {
            out << tag.getLabel() << ":" << tag.getCommitHash() << "\n";
        }
        file.close();
    }
}

void Repository::loadTags()
{
    QFile file(tagsFilePath);
    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            tags.clear();
            while (!in.atEnd())
            {
                QString line = in.readLine();
                QStringList parts = line.split(":");
                if (parts.size() == 2)
                {
                    tags.append(Tag(parts[0], parts[1]));
                }
            }

            file.close();
        }
    }
    else
    {
        qDebug() << "Ne postoji tag fajl!";
    }

}

// Prinosimo tag za commit
bool Repository::tagCommit(const QString &commitHash, const QString &tagName)
{
    if (commitHash.isEmpty() || tagName.isEmpty())
    {
        return false;
    }

    QFile tagsFile(rootPath + "/.tags");
    if (tagsFile.open(QIODevice::Append | QIODevice::Text))
    {
        QTextStream out(&tagsFile);
        out << tagName << ":" << commitHash << "\n";
        tagsFile.close();
        return true;
    }
    else
    {
        qDebug() << "Nije uspelo otvaranje .tags fajla";
        return false;
    }
}

QString Repository::getCommitHashForTag(const QString &tagName) const
{
    for (const Tag &tag : tags)
    {
        if (tag.getLabel() == tagName)
        {
            return tag.getCommitHash();
        }
    }
    return QString();   // Vracamo prazan string ako nista ne nadjemo
}

QString Repository::getCommitTagForHash(const QString &commitHash) const
{
    for (const Tag &tag : tags)
    {
        if (tag.getCommitHash() == commitHash)
        {
            return tag.getLabel();
        }
    }
    return QString();
}

QString Repository::getCommitMessage(const QString &commitHash) const
{
    QString commitFilePath = rootPath + "/.commits/commit_" + commitHash + ".json";
    QFile commitFile(commitFilePath);

    if (commitFile.open(QIODevice::ReadOnly))
    {
        QByteArray commitData = commitFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(commitData);
        QJsonObject jsonObj = doc.object();
        return jsonObj["message"].toString();
    }
    return QString();
}

// Proveren
QString Repository::getCommitAuthor(const QString &commitHash) const
{
    QString commitFilePath = rootPath + "/.commits/commit_" + commitHash + ".json";
    QFile commitFile(commitFilePath);
    if (commitFile.open(QIODevice::ReadOnly))
    {
        QByteArray commitData = commitFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(commitData);
        QJsonObject jsonObj = doc.object();
        return jsonObj["author"].toString();
    }
    return QString();
}

QMap<QString, QVector<QString> > Repository::getBranchCommits()
{
    QMap<QString, QVector<QString>> branchCommits;

    for (const QString &branch : branches)
    {
        QVector<QString> commits = getCommitsForBranch(branch);
        branchCommits.insert(branch, commits);
    }

    return branchCommits;
}

QVector<QString> Repository::getCommitsForBranch(const QString &branchName) const
{
    QVector<QString> commitHashes;
    QDir commitDir(rootPath + "/.commits");
    QStringList commitFiles = commitDir.entryList(QDir::Files, QDir::Time);

    for (const QString &commitFile : commitFiles)
    {
        QString commitFilePath = commitDir.filePath(commitFile);
        QFile file(commitFilePath);
        if (file.open(QIODevice::ReadOnly))
        {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            if (obj["branch"].toString() == branchName)
            {
                QString commitHash = obj["commitHash"].toString();
                commitHashes.append(commitHash);
            }
        }
    }

    return commitHashes;
}

// Kljucna metoda koja ce vratiti ucitani commit
Commit Repository::loadCommit(const QString &commitHash)
{
    QString commitPath = QString(rootPath + "/.commits/commit_" + commitHash + ".json");
    QFile commitFile(commitPath);

    if (!commitFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Nije uspelo otvaranje commit fajla:" << commitPath;
        return Commit();
    }

    QJsonDocument doc = QJsonDocument::fromJson(commitFile.readAll());
    commitFile.close();

    if (!doc.isObject())
    {
        qWarning() << "Nije ispravan commit format:" << commitPath;
        return Commit();
    }

    // fromJson je staticka metoda koja vraca commit objekat
    return Commit::fromJson(doc.object());
}

Commit Repository::getLastCommit() const
{
    return lastCommit;
}

// Ponovo rad sa serverom gde ovde pribavljamo sa serverske strane veryiju
bool Repository::pullRepositoryFromServer(const QString &repositoryName)
{
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", 9000);

    if (socket.waitForConnected())
    {
        // Trazimo od servera da nam posalje repozitorijum
        QJsonObject requestObj;
        requestObj["action"] = "pullRepository";
        requestObj["repositoryName"] = repositoryName;

        QJsonDocument requestDoc = QJsonDocument(requestObj);
        QByteArray requestData = requestDoc.toJson();

        socket.write(requestData);
        socket.waitForBytesWritten();

        // Pripremimo sada elemente za tar file (arhivu) koji ce stici sa servera
        QByteArray tarData;
        while (socket.waitForReadyRead())
        {
            // Jer podaci pristizu jedni za drugim (narocito ako je arhiva velika)
            tarData.append(socket.readAll());
        }

        if (!tarData.isEmpty())
        {
            // Cuvamo sada sacuvani tar fajl
            QString savePath = QFileDialog::getExistingDirectory(nullptr, "Select Directory to Save Repository");
            if (savePath.isEmpty())
            {
                qDebug() << "Nije ni jedna direktiva selektovana. Prekinuta je pull operacija";
                return false;
            }

            QString repoDirectory = savePath + "/" + repositoryName;
            QDir().mkdir(repoDirectory);

            QString tarFilePath = repoDirectory + ".tar";
            QFile tarFile(tarFilePath);
            if (tarFile.open(QIODevice::WriteOnly))
            {
                tarFile.write(tarData);
                tarFile.close();

                // Ekstraktujemo sada tar fajl pomocu QProcess
                QProcess tarProcess;
                tarProcess.start("tar", QStringList() << "-xvf" << tarFilePath << "-C" << savePath);
                tarProcess.waitForFinished();

                // Brisemo sada privremeni tar fajl
                QFile::remove(tarFilePath);

                qDebug() << "Repozitorijum: " << repositoryName << "je uspesno prebacen sa servera";
                return true;
            }
            else
            {
                qDebug() << "Nije uspelo cuvanje tar fajla";
            }
        }
        else
        {
            qDebug() << "Nisu pristigli podaci sa servera (radi cuvanja .tar)";
        }

        socket.disconnectFromHost();
    }
    else
    {
        qDebug() << "Nije uspela konekcija na server (pull)";
    }

    return false;
}

// Trazenje od servera da nam izlista sve repozitorijume dostupne na njegovoj strani
QStringList Repository::requestRepositoryList()
{
    QStringList stringList;
    QTcpSocket socket;

    socket.connectToHost("127.0.0.1", 9000);

    if (socket.waitForConnected())
    {
        QJsonObject jsonObj;
        jsonObj["action"] = "listRepositories";

        QJsonDocument jsonDoc(jsonObj);
        QByteArray data = jsonDoc.toJson();

        socket.write(data);
        socket.waitForBytesWritten();

        if (socket.waitForReadyRead())
        {
            QByteArray response = socket.readAll();
            // QJsonDocument::fromJson(response) - potrebno je prvo u json dokument da prebacimo
            QJsonDocument responseDoc = QJsonDocument::fromJson(response);
            QJsonObject responseObj = responseDoc.object();

            if (responseObj["action"].toString() == "listRepositories")
            {
                QJsonArray reposArray = responseObj["repositories"].toArray();
                for (const QJsonValue& repoValue : reposArray)
                {
                    stringList.append(repoValue.toString());
                    // qDebug() << "Repozitorijum:" << repoValue.toString();
                }
            }
        }

        socket.disconnectFromHost();
    } else {
        qDebug() << "Nije uspela konekcija na server::requestRepositoryList ";
        return stringList;
    }

    return stringList;
}

// Stastujemo promene
void Repository::stashChanges()
{
    QString commitHash = getLastCommitHash();  // Koristi se poslednji commit kao baza za cuvanje
    QJsonObject stashObj;
    stashObj["commitHash"] = commitHash;

    QJsonObject changesObj;

    // Funkcija za rekurzivno prolazenje kroz direktorijume
    std::function<void(const QString&)> processDirectory = [&](const QString &dirPath) {
        QDir dir(dirPath);
        QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        // Obrada fajlova u trenutnom direktorijumu
        for (const QString &fileName : files)
        {
            QString filePath = dirPath + "/" + fileName;
            QFile file(filePath);

            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QTextStream in(&file);
                QString fileContent = in.readAll();
                // Relativni put fajla u odnosu na rootPath
                QString relativeFilePath = filePath.mid(rootPath.length() + 1);
                changesObj[relativeFilePath] = fileContent;
            }
        }

        // Rekurzivno prolazenje kroz poddirektorijume
        for (const QString &subDirName : subDirs)
        {
            QString subDirPath = dirPath + "/" + subDirName;
            processDirectory(subDirPath);
        }
    };

    // Startujemo rekurziju iz root direktorijuma
    processDirectory(rootPath);

    stashObj["changes"] = changesObj;

    // sacuvati stashObj u .stash fajl
    QFile stashFile(rootPath + "/.stash");
    if (stashFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&stashFile);
        QJsonDocument doc(stashObj);
        out << doc.toJson();
    }

    clearCurrentDirectory(); // Nije neophodno, ocistimo trenutni radni direktorijum, da bi videli rezultate
}

void Repository::applyStash() {
    QFile stashFile(rootPath + "/.stash");
    if (!stashFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Nema stash podataka za primenu";
        return;
    }

    QByteArray stashData = stashFile.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(stashData);
    QJsonObject stashObj = doc.object();

    QJsonObject changesObj = stashObj["changes"].toObject();    // Objekat u objektu :)

    for (const QString &relativeFilePath : changesObj.keys())
    {
        QString filePath = rootPath + "/" + relativeFilePath;
        QFile file(filePath);

        // Kreiramo direktorijum ako ne postoji
        QDir dir;
        dir.mkpath(QFileInfo(filePath).absolutePath());

        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&file);
            out << changesObj[relativeFilePath].toString();
        }
    }

    // Nakon primene stash-a, obrise se stash fajl, tj. moguce je samo jednom povratiti prosli sadrzaj
    stashFile.remove();
}

void Repository::discardStash()
{
    if (QFile::exists(stashFilePath))
    {
        QFile::remove(stashFilePath);
        qDebug() << "Stash podaci su uspesno obrisani";
    }
    else
    {
        qDebug() << "Nije pronadjen ni jedan stash podatak koji bi mogao da se obrise";
    }
}

// Mozda nema adekvatnu primenu! TODO
bool Repository::hasStash() const
{
    return QFile::exists(stashFilePath);
}


// Za sada nije potrebna, mozda se doda neka mogucnost koriscenja (zamenjena je drugim metodama)
// void Repository::createRepositoryOnServer(const QString& repositoryName) {
//     QTcpSocket socket;
//     socket.connectToHost("127.0.0.1", 9000);

//     if (socket.waitForConnected()) {
//         QJsonObject jsonObj;
//         jsonObj["action"] = "createRepository";
//         jsonObj["repositoryName"] = repositoryName;

//         QJsonDocument jsonDoc(jsonObj);
//         QByteArray data = jsonDoc.toJson();

//         socket.write(data);
//         socket.waitForBytesWritten();

//         // Wait for a response from the server
//         if (socket.waitForReadyRead()) {
//             QByteArray response = socket.readAll();
//             qDebug() << "Server response:" << response;
//         }

//         socket.disconnectFromHost();
//     } else {
//         qDebug() << "Failed to connect to server.";
//     }
// }
void Repository::saveBranchInfo() const
{
    QFile file(rootPath + "/.current_branch");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out << currentBranch;
        file.close();
    }
    else
    {
        qDebug() << "Nije uspelo cuvanje trenutnih branch informacija";
    }
}

void Repository::loadBranchInfo()
{
    QDir commitDir(rootPath + "/.commits");
    QStringList commitFiles = commitDir.entryList(QDir::Files);

    branches.clear();

    for (const QString &commitFile : commitFiles)
    {
        QString commitFilePath = commitDir.filePath(commitFile);
        QFile file(commitFilePath);

        if (file.open(QIODevice::ReadOnly))
        {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            QString branch = obj["branch"].toString();

            if (!branch.isEmpty() && !branches.contains(branch))
            {
                branches.append(branch);
            }
        }
    }

    if (branches.isEmpty())
    {
        currentBranch = "main";
        branches.append(currentBranch);
    }
    else if (!branches.contains(currentBranch))
    {
        currentBranch = branches.first();
    }
}

// Ucitavamo stablo za odredjeni commit
Tree Repository::loadCommitTree(const QString& commitHash)
{
    // Mozda treba drugaciji loadCommit() //TODO
    Commit commit = loadCommit(commitHash);
    Tree tree = commit.getTree();

    return tree;
}

bool Repository::mergeBranches(const QString &sourceBranch, const QString &targetBranch)
{
    // Proveravamo da li obe grane postoje
    if (!branches.contains(sourceBranch) || !branches.contains(targetBranch))
    {
        qDebug() << "Prva ili druga grana za spajanje ne postoji!";
        return false;
    }

    // Ucitavamo poslednje commit-ove za obe grane
    QString sourceCommitHash = getNthLastCommitForBranch(sourceBranch, 1);  // 1 - poslednji commit
    QString targetCommitHash = getNthLastCommitForBranch(targetBranch, 1);

    if (sourceCommitHash.isEmpty() || targetCommitHash.isEmpty())
    {
        qDebug() << "Nismo uspeli da nađemo poslednji commit za prvu ili drugu granu";
        return false;
    }

    // Ucitavamo sadrzaj commit-ova
    QMap<QString, QString> sourceFiles = loadCommitContent(sourceCommitHash);
    QMap<QString, QString> targetFiles = loadCommitContent(targetCommitHash);

    // Rekurzivno prolazimo kroz sve fajlove i direktorijume u sourceFiles
    if (!mergeDirectories(sourceFiles, targetFiles, rootPath)) {
        qDebug() << "Greska prilikom spajanja direktorijuma.";
        return false;
    }

    // Commit-ujemo sve kao merge
    QString author = "Merge Commit";
    QString message = "Merged branch " + sourceBranch + " into " + targetBranch;
    Commit mergeCommit = createCommit(author, message);

    saveCommitLocally(mergeCommit);
    // Opciono, možemo odmah poslati na server:
    // sendCommitToServer(mergeCommit);

    return true;
}

bool Repository::mergeDirectories(const QMap<QString, QString>& sourceFiles, const QMap<QString, QString>& targetFiles, const QString& currentPath)
{
    QDir dir(currentPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Prolazimo kroz sve fajlove i direktorijume u sourceFiles
    for (auto it = sourceFiles.begin(); it != sourceFiles.end(); ++it)
    {
        QString relativeFilePath = it.key();
        QString sourceContent = it.value();
        QString absoluteFilePath = currentPath + "/" + relativeFilePath;
        QFileInfo fileInfo(absoluteFilePath);
        QString parentDirPath = fileInfo.path();  // Putanja do nadredjenog direktorijuma

        // Proveravamo da li direktorijum parentDirPath postoji, ako ne, kreiramo ga
        QDir parentDir(parentDirPath);
        if (!parentDir.exists()) {
            parentDir.mkpath(".");
        }

        if (targetFiles.contains(relativeFilePath))
        {
            QString targetContent = targetFiles.value(relativeFilePath);

            if (sourceContent != targetContent)
            {
                // Ako postoji konflikt, pokušavamo da ga resimo
                QString mergedContent = resolveConflict(sourceContent, targetContent);
                QFile targetFile(absoluteFilePath);
                if (targetFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream out(&targetFile);
                    out << mergedContent;
                    targetFile.close();
                }
            }
        }
        else
        {
            // Ako fajl ili direktorijum postoji samo u source grani, kopiramo ga
            if (sourceContent.isEmpty())  // Pretpostavka: prazan sadržaj znači direktorijum
            {
                QDir().mkpath(absoluteFilePath);  // Kreiramo direktorijum
            }
            else
            {
                QFile newFile(absoluteFilePath);
                if (newFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream out(&newFile);
                    out << sourceContent;
                    newFile.close();
                }
            }
        }
    }

    // Potrebno je sacuvati fajlove koji se nalaze u targetFiles, ali ne i u sourceFiles
    for (auto it = targetFiles.begin(); it != targetFiles.end(); ++it)
    {
        QString relativeFilePath = it.key();
        QString absoluteFilePath = currentPath + "/" + relativeFilePath;
        QFileInfo fileInfo(absoluteFilePath);
        QString parentDirPath = fileInfo.path();  // Putanja do nadređenog direktorijuma

        if (!sourceFiles.contains(relativeFilePath))
        {
            QFile targetFile(absoluteFilePath);
            if (!targetFile.exists())
            {
                QString targetContent = it.value();

                QDir parentDir(parentDirPath);
                if (!parentDir.exists()) {
                    parentDir.mkpath(".");
                }

                if (targetContent.isEmpty())  // Pretpostavka: prazan sadržaj znači direktorijum
                {
                    QDir().mkpath(absoluteFilePath);  // Kreiramo direktorijum
                }
                else
                {
                    if (targetFile.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        QTextStream out(&targetFile);
                        out << targetContent;
                        targetFile.close();
                    }
                }
            }
        }
    }

    return true;
}

// U slucaju da je doslo do konflikta prepustice se korisniku da ga resi
// Ova metoda ce oznaciti u kojim linijama je tacno doslo do konflikta
QString Repository::resolveConflict(const QString &sourceContent, const QString &targetContent)
{
    QStringList sourceLines = sourceContent.split('\n');
    QStringList targetLines = targetContent.split('\n');
    QStringList mergedLines;

    int i = 0;
    int j = 0;

    while (i < sourceLines.size() && j < targetLines.size())
    {
        if (sourceLines[i] == targetLines[j])
        {
            mergedLines.append(sourceLines[i]);
            ++i;
            ++j;
        }
        else
        {
            // Ako je doslo do konflikta
            mergedLines.append("<<<<< HEAD");
            mergedLines.append(targetLines[j]);
            mergedLines.append("=====");
            mergedLines.append(sourceLines[i]);
            mergedLines.append(">>>>> MERGED");
            ++i;
            ++j;
        }
    }

    // Ako je nesto preostalo od linija, to samo dodamo
    while (i < sourceLines.size())
    {
        mergedLines.append(sourceLines[i++]);
    }
    while (j < targetLines.size())
    {
        mergedLines.append(targetLines[j++]);
    }

    return mergedLines.join("\n");  // Potrebno je implementirati i .join("\n") izmedju svakog umetni novi red
}

// Cuvamo u memoriji stash za odredjeni commit
void Repository::saveStashInfo(const QString &commitHash)
{
    QFile file(stashFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out << commitHash;
        file.close();
    }
    else
    {
        qDebug() << "Nije uspelo cuvanje stash podataka";
    }
}

// Ucitavamo stasirane podatke
QString Repository::loadStashInfo() const
{
    QFile file(stashFilePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        return in.readAll().trimmed();
    }
    else
    {
        qDebug() << "Nije uspelo ucitavanje stash podataka";
        return QString();
    }

}

// void Repository::restoreProjectToCommit(const Commit &commit)
// {

// }


