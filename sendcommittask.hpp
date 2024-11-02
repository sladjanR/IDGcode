#ifndef SENDCOMMITTASK_HPP
#define SENDCOMMITTASK_HPP

#include "commit.hpp"
#include "repository.hpp"

// Qt
#include <QThread>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>


class CommitWorker : public QObject
{
    Q_OBJECT

private:
    Repository *repository;
    Commit commit;

public:
    explicit CommitWorker(Repository *repo, const Commit &commit)
        : repository(repo), commit(commit) {}

public slots:
    void process()
    {
        // Pozovi metodu za slanje commita i svih pratećih elemenata na server
        if (repository->sendCommitToServer(commit)) {
            qDebug() << "Commit i prateći elementi uspešno poslati na server.";
        } else {
            qDebug() << "Greška prilikom slanja commit-a na server.";
        }

        // Signaliziraj da je posao završen
        emit finished();
    }

signals:
    void finished();
};

#endif // SENDCOMMITTASK_HPP
