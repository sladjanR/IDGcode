#ifndef MERGEWORKER_HPP
#define MERGEWORKER_HPP

// Qt
#include <QObject>
#include "repository.hpp"

class MergeWorker : public QObject
{
    Q_OBJECT

private:
    Repository* repository;
    QString sourceBranch;
    QString targetBranch;

public:
    MergeWorker(Repository* repo, const QString& sourceBranch, const QString& targetBranch)
        : repository(repo), sourceBranch(sourceBranch), targetBranch(targetBranch) {}

public slots:
    void process()
    {
        bool success = repository->mergeBranches(sourceBranch, targetBranch);
        emit finished(success);
    }

signals:
    // Potrebno je kreirati slot koji ce prihvatiti ovaj signal sa jednim argumentom
    void finished(bool success);

};

#endif // MERGEWORKER_HPP
