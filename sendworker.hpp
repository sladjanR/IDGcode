#ifndef SENDWORKER_HPP
#define SENDWORKER_HPP

#include "repository.hpp"
#include "commit.hpp"

// Qt
#include <QObject>
#include <QTcpSocket>


class SendRepoWorker : public QObject
{
    Q_OBJECT

private:
    Repository* repository;
    Commit commit;

public:
    SendRepoWorker(Repository* repo, const Commit& commit)
        : repository(repo), commit(commit) {}

public slots:
    void process()
    {
        bool success = repository->sendCommitToServer(commit);
        emit finished(success);
    }

signals:
    void finished(bool success);

};


#endif // SENDWORKER_HPP
