#ifndef COMPAREWORKER_HPP
#define COMPAREWORKER_HPP

#include "tree.hpp"
#include "repository.hpp"

// Qt
#include <QObject>
#include <QElapsedTimer>


class CompareWorker : public QObject
{
    Q_OBJECT

private:
    Repository* repository;
    QString lastCommitHash;
    QString secondLastCommitHash;

public:
    // Compare ce u posebnom thread-u da poredi posledji i predposlednji commit
    CompareWorker(Repository* repo, const QString& lastCommitHash, const QString& secondLastCommitHash)
        : repository(repo), lastCommitHash(lastCommitHash), secondLastCommitHash(secondLastCommitHash) {}

public slots:
    // Posto smo nasledili QObject, mozemo koristiti process()
    // Moglo je to biti i run() da je nasledjena klasa bila QThread
    void process()
    {
        Tree lastCommitTree = loadCommitTree(lastCommitHash);
        Tree secondLastCommitTree = loadCommitTree(secondLastCommitHash);

        compareTrees(lastCommitTree, secondLastCommitTree, repository->getRepoPath());

        // Eksplicitno emitijujemo signal da je zavrsen proces, signal je bez argumenata
        emit finished();
    }

signals:
    void finished();

private:
    Tree loadCommitTree(const QString& commitHash)
    {
        QString commitFilePath = repository->getRepoPath() + "/.commits/commit_" + commitHash + ".json";
        QFile commitFile(commitFilePath);

        if (commitFile.open(QIODevice::ReadOnly))
        {
            QByteArray commitData = commitFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(commitData);
            QJsonObject jsonObj = doc.object();
            QString treeHash = jsonObj["treeHash"].toString();

            // Citanje stabla sa diska, odredjenih parametara koje prosledjujemo kao arhument
            return Tree::fromDisk(treeHash, repository->getRepoPath());
        }
        else
        {
            qDebug() << "Nije uspelo ucitavanje sadrzaja: Commit fajl nije pronadjen";
            return Tree();
        }
    }

    void compareTrees(const Tree& tree1, const Tree& tree2, const QString& currentPath)
    {
        // Komparacija blobova koji se nalaze u stablima
        for (const QString& blobName : tree1.getBlobs().keys())
        {
            QString filePath = currentPath + "/" + blobName;    // Realno ime, ne hash
            QString blobHash1 = tree1.getBlobs().value(blobName);
            // Mudro resenje, ako postoji key sa zeljenim value vratice key
            // Ali ako ne postoji key sa tim value, onda ce da vrati prazan string!!!
            QString blobHash2 = tree2.getBlobs().value(blobName, QString());


            // QString blobContent1 = repository->loadBlobContent(blobHash1);
            // // Ternarni, ako nam je blobHash2 prazan onda nema ni kontenta za taj hash
            // QString blobContent2 = blobHash2.isEmpty() ? QString() : repository->loadBlobContent(blobHash2);
            // if (blobContent1 != blobContent2)
            // {

            //     // Obratiti paznju da ovaj signal koji emitujemo ima argumente, tj. nosi vrednosti sa sobom!!!
            //     // Potrebno je da postoji slot koji ce primiti te vrednosti (argumenti isto tipa)
            //     emit displayDiff(filePath, blobContent2, blobContent1);
            // }

            // QElapsedTimer timer;
            // timer.start();
            // TODO (nekada je odmah bila provera content-a, ali sada je pojednostavljeno)
            // DOSTAJ JE BRZE - MERENO PREKO BENCHMARKA, tj. preko elapsed timer-a
            if (blobHash1 != blobHash2)
            {
                QString blobContent1 = repository->loadBlobContent(blobHash1);
                // Ternarni, ako nam je blobHash2 prazan onda nema ni kontenta za taj hash
                QString blobContent2 = blobHash2.isEmpty() ? QString() : repository->loadBlobContent(blobHash2);
                // Obratiti paznju da ovaj signal koji emitujemo ima argumente, tj. nosi vrednosti sa sobom!!!
                // Potrebno je da postoji slot koji ce primiti te vrednosti (argumenti isto tipa)
                emit displayDiff(filePath, blobContent2, blobContent1);
            }
            // qint64 elapsed = timer.nsecsElapsed();
            // qDebug() << "Proteklo vreme u nanosekundama: " << elapsed;


        }

        // Komparacija subdirektiva
        // Sada uzimamo .getTrees(), tj. ako postoje podstabla
        for (const QString& subTreeName : tree1.getTrees().keys())
        {
            QString subdirPath = currentPath + "/" + subTreeName;
            QString subTreeHash1 = tree1.getTrees().value(subTreeName);
            // Objsnjeno gore zasto (subTreeName, QString())
            QString subTreeHash2 = tree2.getTrees().value(subTreeName, QString());

            Tree subTree1 = Tree::fromDisk(subTreeHash1, repository->getRepoPath());
            Tree subTree2 = subTreeHash2.isEmpty() ? Tree() : Tree::fromDisk(subTreeHash2, repository->getRepoPath());

            // REKURZIJA!!!
            // Mozda ce u buducnosti postojati neko bolje resenje, bez koriscenja rekurzije
            compareTrees(subTree1, subTree2, subdirPath);
        }

        // Proveravamo fajlove i direktorijume koji su postojali u stablu 2 (ali obrisani u stablu 1)
        for (const QString& blobName : tree2.getBlobs().keys())
        {
            if (!tree1.getBlobs().contains(blobName))
            {
                QString filePath = currentPath + "/" + blobName;
                QString blobHash2 = tree2.getBlobs().value(blobName);
                QString blobContent2 = repository->loadBlobContent(blobHash2);

                // Emitovan signal ce kao drugi argument proslediti prazan string
                // Prazan string zato sto je daj dokument obrisan ili nije ni postojao
                emit displayDiff(filePath, blobContent2, "");
            }
        }

        for (const QString& subTreeName : tree2.getTrees().keys())
        {
            if (!tree1.getTrees().contains(subTreeName))
            {
                QString subdirPath = currentPath + "/" + subTreeName;
                Tree subTree2 = Tree::fromDisk(tree2.getTrees().value(subTreeName), repository->getRepoPath());

                compareTrees(Tree(), subTree2, subdirPath);
            }
        }
    }

signals:
    void displayDiff(const QString& fileName, const QString& oldContent, const QString& newContent);
};

#endif // COMPAREWORKER_HPP
