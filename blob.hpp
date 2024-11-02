#ifndef BLOB_H
#define BLOB_H

// Qt
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QCryptographicHash>


class Blob
{
private:
    QByteArray content;
    QString hash;
public:
    // Potrebno je sacuvati i konstruktor bez argumenata
    Blob() = default;
    // Konstruktor sa argumentima koji cuva blob na disku
    Blob(const QString& filePath, const QString& rootPath)
    {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly))
        {
            // Citamo sve podatke i i smestamo u niz bajtova
            QByteArray fileContent = file.readAll();
            content = fileContent;
            // Generisemo hash za kontent koji smo procitali
            hash = generateHash(content);
            file.close();

            // Na kraju cuvamo napravljeni blob u memoriju
            saveToDisk(rootPath);
        }
    }

    // Metoda pomocu koje citamo podatke vezane za blob, pristupamo nevezano za objekat
    static Blob fromDisk(const QString& blobHash, const QString& rootPath)
    {
        QString blobFilePath = rootPath + "/.blobs/blob_" + blobHash + ".txt";
        QFile blobFile(blobFilePath);
        Blob blob;
        // Ako taj blob postoji procitacemo kontent
        if (blobFile.open(QIODevice::ReadOnly))
        {
            blob.content = blobFile.readAll();
            blob.hash = blobHash;
            blobFile.close();
        }
        return blob;
    }

    // Geter za hash vrednost
    QString getHash() const
    {
        return hash;
    }
    // Geter za content
    QByteArray getContent() const
    {
        return content;
    }

public:
    // Metoda koja generise hash na osnovu kontenta (od bytearray-a)
    QString generateHash(const QByteArray& data)
    {
        // Generisemo pomocu Sha1 algoritma
        QByteArray hashData = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
        return hashData.toHex();
    }

    // Metoda za cuvanje bloba
    bool saveToDisk(const QString& rootPath) const
    {
        // Proveravamo prvo da li direktorijum postoji
        QString blobDir = rootPath + "/.blobs";

        if (!QDir(blobDir).exists())
        {
            QDir().mkpath(blobDir);
        }

        // Definisemo putanju do naseg blob-a (do fajla)
        QString blobFilePath = blobDir + "/blob_" + hash + ".txt";

        // Upisujemo sadrzaj u blob
        QFile blobFile(blobFilePath);
        if (blobFile.open(QIODevice::WriteOnly))
        {
            blobFile.write(content);
            blobFile.close();
            return true;
        }
        return false;
    }
};


#endif // BLOB_H
