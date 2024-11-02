#ifndef TREE_HPP
#define TREE_HPP

#include "blob.hpp"

// Qt
#include <QMap>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QString>

class Tree
{
private:
    // QMap od naziva fajla i hash imena
    QMap<QString, QString> blobs;
    // Qmap od naziva direktorijuma i subdirektorijuma hash
    QMap<QString, QString> trees;
    // Kesiramo (privremena vrednost) hash od trenutnog stabla
    // Kao mutable moze da se nadje i u instancama klase const (function() const) i tu se isto moze promeniti
    mutable QString hash;

public:
    // Dodajemo fajl sa njegovim hashom kao novi blob u stablu
    void addBlob(const QString& name, const Blob& blob)
    {
        // name - realno ime, value - hash od konteksta, novo ime
        blobs[name] = blob.getHash();
    }

    // Dodajemo subdirektorijum na stablo
    void addTree(const QString& name, const Tree& tree)
    {
        trees[name] = tree.getHash();
    }

    // Vracamo hash stabla
    QString getHash() const
    {
        if (hash.isEmpty()) // Generisemo hash, jedino ako pre nije korisceno
        {
            QByteArray combinedData;
            for (const auto& blobHash : blobs)
            {
                combinedData.append(blobHash.toUtf8());
            }
            for (const auto& treeHash : trees)
            {
                combinedData.append(treeHash.toUtf8());
            }

            hash = generateHash(combinedData);
        }
        return hash;
    }

    // Geter za dobijanje SVHIH blobova (ime-hash)
    QMap<QString, QString> getBlobs() const
    {
        return blobs;
    }

    // Geter za dobijanje svih podstabala
    QMap<QString, QString> getTrees() const
    {
        return trees;
    }

    // Pretvaramo vrednosti u JSON fajl
    QJsonObject toJson() const
    {
        QJsonObject json;
        QJsonObject treeJson;
        QJsonObject blobJson;

        // Koristimo iterator od mape
        for (auto it = blobs.begin(); it != blobs.end(); ++it)
        {
            // Json objekat je zapisan kao ["ime"] = hash
            blobJson[it.key()] = it.value();
        }

        for (auto it = trees.begin(); it != trees.end(); ++it)
        {
            treeJson[it.key()] = it.value();
        }

        // Objekat koji ce sadrzati objekat
        json["blobs"] = blobJson;
        json["trees"] = treeJson;

        return json;
    }

    // Kreiranje stabla od json-a koji je prosledjen kao argument
    static Tree fromJson(const QJsonObject& json)
    {
        Tree tree;
        QJsonObject blobJson = json["blobs"].toObject();
        QJsonObject treeJson = json["trees"].toObject();

        for (const QString& key : blobJson.keys())
        {
            tree.blobs[key] = blobJson[key].toString();
        }

        for (const QString& key : treeJson.keys())
        {
            tree.trees[key] = treeJson[key].toString();
        }

        return tree;
    }

    // Cuvanje stabla na lokalnoj memoriji
    bool saveToDisk(const QString& rootPath) const
    {
        QString treeDir = rootPath + "/.trees";
        if (!QDir(treeDir).exists())
        {
            QDir().mkpath(treeDir);
        }

        // Poseban nacin dodele imena prefiks i sufiks na hash vrednost
        QString treeFilePath = treeDir + "/tree_" + getHash() + ".json";
        QFile treeFile(treeFilePath);

        if (treeFile.open(QIODevice::WriteOnly))
        {
            QJsonDocument doc(toJson());    // Kao argument konstruktora dajemo povratnu vrednost nase metode (QJsonObject)
            treeFile.write(doc.toJson());
            treeFile.close();
            return true;
        }
        return false;
    }

    // Citanje stabla sa diska
    static Tree fromDisk(const QString& treeHash, const QString& rootPath)
    {
        QString treeFilePath = rootPath + "/.trees/tree_" + treeHash + ".json";
        QFile treeFile(treeFilePath);
        Tree tree;

        if (treeFile.open(QIODevice::ReadOnly))
        {
            QByteArray treeData = treeFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(treeData);  // fromJson staticka koja kao argument trazi QByteArray
            QJsonObject jsonObj = doc.object();

            tree = Tree::fromJson(jsonObj);
            tree.hash = treeHash; // Privremena hash vrednost se dodeljuje (kesira)
            treeFile.close();
        }
        return tree;
    }

private:
    // Generisemo hash pomocu SHA-1 algoritma
    QString generateHash(const QByteArray& data) const
    {
        QByteArray hashData = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
        // Treba vratiti hash vrednost, a ne niz bajtova
        return hashData.toHex();
    }
};

#endif // TREE_HPP
