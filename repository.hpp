#ifndef REPOSITORY_HPP
#define REPOSITORY_HPP

#include "commit.hpp"
#include "tree.hpp"
#include "tag.hpp"

// Qt
#include <QString>
#include <QMap>
#include <QGraphicsView>
#include <QDir>

class Repository
{
private:
    QString rootPath;
    QString currentBranch;
    QStringList branches;
    QString stashFilePath;

    QList<Tag> tags;
    QString tagsFilePath;
    Commit lastCommit;

public:
    Repository(const QString &rootPath);

    void addFilesToTree(Tree &tree, const QDir &dir);
    void saveCommitLocally(const Commit &commit);
    QString getLastCommitHash() const;
    QMap<QString, QString> loadCommitContent(const QString& commitHash) const;
    QString loadBlobContent(const QString& blobHash) const;

    Commit createCommit(const QString& author, const QString& message);
    bool sendCommitToServer(const Commit& commit);

    void createBranch(const QString &branchName);
    void switchBranch(const QString &branchName);
    QString getCurrentBranch() const;
    QStringList getBranches() const;

    void restoreToCommit(const QString &commitHash);
    QString getRepoPath() const;

    QString getSecondLastCommitHash() const;
    QString getNthLastCommitForBranch(const QString &branchName, int n) const;
    void clearCurrentDirectory();

    void addTag(const QString &label, const QString &commitHash);
    QStringList getTags() const;
    void saveTags();
    void loadTags();
    bool tagCommit(const QString &commitHash, const QString &tagName);

    QString getCommitHashForTag(const QString &tagName) const;
    QString getCommitTagForHash(const QString &commitHash) const;

    QString getCommitMessage(const QString &commitHash) const;
    QString getCommitAuthor(const QString &commitHash) const;
    QMap<QString, QVector<QString>> getBranchCommits();
    QVector<QString> getCommitsForBranch(const QString &branchName) const;

    Commit loadCommit(const QString& commitHash);
    Commit getLastCommit() const;

    bool pullRepositoryFromServer(const QString &repositoryName);
    QStringList requestRepositoryList();
    // void createRepositoryOnServer(const QString &repositoryName);

    void stashChanges();
    void applyStash();
    void discardStash();
    bool hasStash() const;

    bool mergeBranches(const QString &sourceBranch, const QString &targetBrach);

    bool sendCommitDataToServer(const QString &commitHash);
private:
    Tree loadCommitTree(const QString &commitHash);
    bool mergeTrees(Tree &sourceTree, Tree &targetTree, const QString &currentPath);
    bool saveMergedContent(const QString &filePath, const QString &content);
private:
    void saveBranchInfo() const;
    void loadBranchInfo();
    QString resolveConflict(const QString &sourceContent, const QString &targetContent);
    //void restoreProjectToCommit(const Commit &commit);

    void saveStashInfo(const QString &commitHash);
    QString loadStashInfo() const;

    void restoreTree(const QJsonObject &treeObj, const QString &currentPath);
    void restoreTree(const Tree &tree, const QString &currentPath);
    void loadTreeContent(const QString &treeHash, const QString &currentPath, QMap<QString, QString> &contentMap) const;

    Blob mergeBlobs(const Blob &targetBlob, const Blob &sourceBlob);
    bool mergeDirectories(const QMap<QString, QString> &sourceFiles, const QMap<QString, QString> &targetFiles, const QString &currentPath);
    bool sendTreeToServer(const Tree &tree);
    bool sendBlob(const QString &blobHash, const QString &currentPath);
    bool sendTreeAndBlobs(const QString &treeHash, const QString &currentPath);
    QStringList collectTreeHashes(const QString &rootTreeHash);
};

class RestoreWorker : public QObject
{
    Q_OBJECT

public:
    RestoreWorker(Repository* repo, const QString& commitHash)
        : repository(repo), commitHash(commitHash) {}

public slots:
    void process()
    {
        repository->restoreToCommit(commitHash);
        emit finished();
    }

signals:
    void finished();

private:
    Repository* repository;
    QString commitHash;
};

#endif // REPOSITORY_HPP
