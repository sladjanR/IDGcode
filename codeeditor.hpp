#ifndef CODEEDITOR_HPP
#define CODEEDITOR_HPP

// Qt
#include <QSyntaxHighlighter>
#include <QPlainTextEdit>

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
private:
    QSyntaxHighlighter *highlighter;
    QWidget *lineNumberArea;
    QAction *copyAction;
    QAction *undoAction;
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    // Metoda changeTheme ce biti kasnije implementirana
    void changeTheme(const QString &theme);
    void setSyntaxHighlighting(QSyntaxHighlighter *highlighter);
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void setCopyShortcut(const QKeySequence &keySequence);
    void setUndoShortcut(const QKeySequence &keySequence);


    // QWidget interfejs
    void changeBackground(const QColor& color);
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    void insertMatchedCharacter(QChar openChar, QChar closeChar);
    void handleAutoIndentation();
    void zoomInText();
    void zoomOutText();
    void resetZoom();
    bool isCursorBetweenBraces();
    void handleEnterBetweenBraces();
};

class LineNumberArea : public QWidget
{
private:
    CodeEditor *codeEditor;

public:
    LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor)
    {}

    QSize sizeHint() const override
    {
        return QSize(codeEditor->lineNumberAreaWidth(),0);
    }

    void setUserFont(QFont& font);

protected:
    void paintEvent(QPaintEvent *event) override
    {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

};

#endif // CODEEDITOR_HPP
