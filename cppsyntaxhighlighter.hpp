#ifndef CPPSYNTAXHIGHLIGHTER_HPP
#define CPPSYNTAXHIGHLIGHTER_HPP

// Qt
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class CPPSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

private:
    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;


public:
    CPPSyntaxHighlighter(QTextDocument *parent = nullptr);

    // QSyntaxHighlighter interfejs
protected:
    void highlightBlock(const QString &text) override;

private:
    // Ugnjezdena struktura koja definise pravila markiranja
    struct HighlightingRule
    {
        // Sablon ce biti regularExpression
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    // Vektor ciji su elementi objketi strukture HighlightingRule
    QVector<HighlightingRule> highlightingRules;

};

#endif // CPPSYNTAXHIGHLIGHTER_HPP
