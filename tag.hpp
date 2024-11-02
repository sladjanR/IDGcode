#ifndef TAG_HPP
#define TAG_HPP

// Qt
#include <QString>


class Tag
{
private:
    QString label;
    QString commitHash;

public:
    Tag(const QString &label, const QString &commitHash);

    QString getLabel() const;
    QString getCommitHash() const;
};

#endif // TAG_HPP
