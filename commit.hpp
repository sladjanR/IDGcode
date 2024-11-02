#ifndef COMMIT_HPP
#define COMMIT_HPP

#include "tree.hpp"

// Qt
#include <QString>
#include <QDateTime>
#include <QCryptographicHash>

class Commit
{
private:
    Tree tree;
    QString hash;
    QString author;
    QString message;
    QString parentHash;
    QString branch;
    // Timestamp ce biti neki veliki broj zato obavezno quint64
    quint64 timestamp;
public:
    // Sacuvamo i prazan konstruktor
    Commit() = default;
    // Konstruktor koji prima kao argumente poruku, stablo, autora, hash od oca, granu gde pripada
    Commit(const QString& message, const Tree& projectTree, const QString& author, const QString& parentHash, const QString& branch) :
        message(message),
        tree(projectTree),
        author(author),
        parentHash(parentHash),
        branch(branch)
    {
        // Obratiti paznju da su u pitanju milisekunde
        timestamp = QDateTime::currentMSecsSinceEpoch();
        hash = generateHash();
    }

    QString getTreeHash() const { return tree.getHash();}
    QString getHash() const { return hash;}
    QString getAuthor() const { return author;}
    QString getMessage() const { return message;}
    QString getParentHash() const { return parentHash; }
    QString getBranch() const { return branch; }
    qint64 getTimestamp() const { return timestamp;}

    const Tree& getTree() const { return tree; }

    QJsonObject toJson() const
    {
        QJsonObject json;
        json["commitHash"] = hash;
        json["treeHash"] = tree.getHash();
        json["author"] = author;
        json["message"] = message;
        json["parentHash"] = parentHash;
        json["branch"] = branch;
        json["timestamp"] = QString::number(timestamp);

        // NEMOJ DA ZABORAVIS I TREE da ubacis u commit, mozda nije u json ali mora i to ako nisi (preko konstruktora) TODO
        return json;
    }

    // Metoda koja nam iz jsonObjekta povlaci potrebne informacije i vraca objekat commit-a
    static Commit fromJson(const QJsonObject& json)
    {
        // Moramo koristiti Tree objekat jer je "tree" resiv iskljucivo rekurzivno
        Tree tree = Tree::fromJson(json["tree"].toObject());
        QString message = json["message"].toString();
        QString author = json["author"].toString();
        QString parentHash = json["parentHash"].toString();
        QString branch = json["branch"].toString();

        Commit commit(message, tree, author, parentHash, branch);
        commit.hash = json["commitHash"].toString();
        commit.timestamp = json["timestamp"].toString().toLongLong();

        return commit;
    }

    // Potrebno je imati i setere jer ove informacije ne izvlacimo iz JSON-a
    void setHash(const QString& newHash) { hash = newHash;}
    void setTimestamp(qint64 newTimestamp) { timestamp = newTimestamp;}

private:
    QString generateHash() const
    {
        // Kao ulaz algoritma postavljamo podatke o svim informacijama koje commit raspolaze
        QByteArray data;
        data.append(tree.getHash().toUtf8());
        data.append(parentHash.toUtf8());
        data.append(author.toUtf8());
        data.append(message.toUtf8());
        data.append(branch.toUtf8());
        data.append(QString::number(timestamp).toUtf8());

        return QString(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
    }
};

#endif // COMMIT_HPP
