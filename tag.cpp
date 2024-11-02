#include "tag.hpp"

Tag::Tag(const QString &label, const QString &commitHash) :
    label{label},
    commitHash{commitHash}
{}

QString Tag::getLabel() const
{
    return label;
}

QString Tag::getCommitHash() const
{
    return commitHash;
}
