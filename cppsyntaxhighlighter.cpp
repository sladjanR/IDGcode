#include "cppsyntaxhighlighter.hpp"

CPPSyntaxHighlighter::CPPSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter{parent}
{
    HighlightingRule rule;

    // Sada jednostavno dodajemo sto vise kljucnih reci iz C++
    keywordFormat.setForeground(Qt::darkCyan);
    keywordFormat.setFontWeight(QFont::Bold);

    const QString keywordPatterns[] =
    {
        "\\bclass\\b", "\\bconst\\b", "\\bdouble\\b", "\\benum\\b", "\\bexplicit\\b",
        "\\bfriend\\b", "\\binline\\b", "\\bint\\b", "\\blong\\b", "\\bnamespace\\b",
        "\\boperator\\b", "\\bprivate\\b", "\\bprotected\\b", "\\bpublic\\b", "\\bshort\\b",
        "\\bsignals\\b", "\\bsigned\\b", "\\bslots\\b", "\\bstatic\\b", "\\bstruct\\b",
        "\\btemplate\\b", "\\btypedef\\b", "\\btypename\\b", "\\bunion\\b", "\\bunsigned\\b",
        "\\bvirtual\\b", "\\bvoid\\b", "\\bvolatile\\b"
    };

    for (const QString &pattern : keywordPatterns)
    {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // Klase
    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(Qt::darkMagenta);
    rule.pattern = QRegularExpression("\\bQ[A-Za-z]+\\b");
    /*
     * \\b - oznacava granicu reci, znaci uzorak mora da pocne na pocetku reci
     * Q - oznacava da uzorak mora poceti sa slovom Q
     * [A-Za-z]+ - bilo koji niz slova (velika i mala) koji se pojavljuju jedan ili vise puta
     * \\b - oznacava kraj reci
     *
    */
    rule.format = classFormat;
    highlightingRules.append(rule);

    // Funkcije
    functionFormat.setFontItalic(true);
    functionFormat.setForeground(QColor(244, 208, 63 ));
    rule.pattern =  QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
    /*
     * (?=\\() - pozitivno gledanje unapred (lookhead) koji trazi ovorenu zagradu ,,(" odmah nakon niza
    */

    rule.format = functionFormat;
    highlightingRules.append(rule);

    // Stringovi
    quotationFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegularExpression("\".*\"");
    /*
     * \" - navodnici
     * .* - bilo koji niz znakova (cak i nula ili vise znakova)
     * \" - jos jednom navodnici
    */
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    // Komentari (jednolinijski)
    singleLineCommentFormat.setForeground(QColor(130, 224, 170 ));
    rule.pattern = QRegularExpression("//[^\n]*");
    /*
     * // - dvostruka kosa crta oznacava pocetak komentara nama
     * [^\n]* - bilo koji niz znakova OSIM novog reda (vise puta ili ni jednom)
    */
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    // Komentari (viselinijski)
    multiLineCommentFormat.setForeground(QColor(130, 224, 170 ));
}

void CPPSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Koriscenjem konstante reference izbegavamo kopiranje
    // qAsConst koristi se za castovanje
    for (const  HighlightingRule &rule : qAsConst(highlightingRules))
    {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext())
        {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
    {
        startIndex = text.indexOf(QRegularExpression("/\\*"));
        // Prepoznaje blok komenara koji krece sa "/*"
    }

    while (startIndex >= 0)
    {
        QRegularExpressionMatch endMatch = QRegularExpression("\\*/").match(text, startIndex);
        // Zvezda je specijalni znak zato pre nje mora da stoji "\\"
        int endIndex = endMatch.capturedStart();
        int commentLength = 0;
        if (endIndex == -1)
        {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;

        }
        else
        {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf(QRegularExpression("/\\*"), startIndex + commentLength);
    }
}
