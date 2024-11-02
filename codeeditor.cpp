#include "codeeditor.hpp"

// Qt
#include <QPainter>
#include <QPalette>

CodeEditor::CodeEditor(QWidget *parent) :
    QPlainTextEdit(parent),
    highlighter{nullptr},
    lineNumberArea{new LineNumberArea(this)}
{
    // Pocetna tema neka bude recimo dark
    changeTheme("dark");    // Moze recimo da se sacuva u .themes i stalno odatle ucitava podrazumevano
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));   // Jedan tab da bude cetiri karaktera

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    copyAction = new QAction(this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &CodeEditor::copy);
    addAction(copyAction);

    undoAction = new QAction(this);
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &CodeEditor::undo);
    addAction(undoAction);
}

// Promena teme u zavisnosti od argumenta: ,,light" ili ,,dark"
void CodeEditor::changeTheme(const QString &theme)
{
    // Koristimo QPallete kako bi smo upravljali bojama na Widget-u
    QPalette p = palette();
    if (p == QPalette())
    {
        qDebug() << "Paletta nije uzeta";
    }
    if (theme == "dark")
    {
        p.setColor(QPalette::Base, QColor(46, 52, 64));
        p.setColor(QPalette::Text, QColor(230, 230, 230));
    }
    else
    {
        p.setColor(QPalette::Base, QColor(255, 255, 255));
        p.setColor(QPalette::Text, QColor(0, 0, 0));
    }

    // Mozemo da koristimo ovu funkciju jer nasledjujemo QWidget(QPlainText je nasledjuje)
    setPalette(p);
}

void CodeEditor::changeBackground(const QColor &color)
{
    QPalette palette = this->palette();
    palette.setColor(QPalette::Base, color);
    this->setPalette(palette);
}

// Ovde postavljamo jednostavno highlighter za dokument, ako vec postoji brisemo prosli
void CodeEditor::setSyntaxHighlighting(QSyntaxHighlighter *newHighlighter)
{
    if (highlighter)
    {
        delete highlighter;
    }

    highlighter = newHighlighter;
    highlighter->setDocument(document());
}

// U zavisnoti od event-a pokrecemo odredjene akcije
void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    // CTRL - modifier
    if (event->modifiers() == Qt::ControlModifier)
    {
        switch (event->key()) {
            // U prvom slucaju proveravamo da li korisnik zeli da zoomIn (+ i = u zavisnosti od tastature)
            // OBRATITI PAZNJU DA KORISTIMO RETURN umesto BREAK;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomInText();
            return;
            // Minus za smanjenje editora
        case Qt::Key_Minus:
            zoomOutText();
            return;
        case Qt::Key_0:
            resetZoom();
            return;
        default:
            break;
        }
    }

    // Prethodni se aktivirao samo uz ctrl ovo je kada control taster nije pritisnut
    switch (event->key()) {
    case Qt::Key_ParenLeft: // Otvorena obicna zagrada '('
        insertMatchedCharacter('(', ')');
        return;
    case Qt::Key_BraceLeft: // Otvorena viticasta zagrada '{'
        insertMatchedCharacter('{', '}');
        return;
    case Qt::Key_BracketLeft: // Otvorena uglasta zagrada '['
        insertMatchedCharacter('[', ']');
        return;
    case Qt::Key_QuoteDbl: // Otvoreni navodnici ' " '
        insertMatchedCharacter('"', '"');
        return;
    case Qt::Key_Apostrophe: // Otvoreni jednostruki ' ' '
        insertMatchedCharacter('\'', '\'');
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (isCursorBetweenBraces()) {  // Prvo vrsimo proveru gde se kursor nalazi
            handleEnterBetweenBraces();  // Obradimo enter izmedju zagrada
        } else {
            QPlainTextEdit::keyPressEvent(event); // Standardno ponasanje za enter  ako nije izmedju zagrada
            handleAutoIndentation(); // Automatsko uvlacenje
        }
        return;
    case Qt::Key_Tab:
        insertPlainText("    "); // Cetiri space-a umesto tab-a
        return;
    default:
        QPlainTextEdit::keyPressEvent(event);
        break;
    }
}

// Metoda za ubacivanje zavrsnog karaktera
void CodeEditor::insertMatchedCharacter(QChar openChar, QChar closeChar)
{
    // Uzmemo objekat kursora koji se koristi
    QTextCursor cursor = textCursor();  // textCursor() smo nasledili iz QPlainTextEdit() - kursor koji se nalazi na dokumentu
    int pos = cursor.position();

    if (isCursorBetweenBraces())
    {
        cursor.movePosition(QTextCursor::Right);
    }
    else
    {
        insertPlainText(QString(openChar) + QString(closeChar));
    }

    // Pomera i poziciju kursora za jedno mesto udesno;
    cursor.setPosition(pos + 1);
    setTextCursor(cursor);
}

// Provera da li se kursor nalazi izmedju zagrada ,, { } "
bool CodeEditor::isCursorBetweenBraces()
{
    QTextCursor cursor = textCursor();
    int currentPos = cursor.position();

    // Provera da li se kursor nalazi između '{' i '}', ako je kursor ,,|,, ond je ,,}" na poziciji ,,|}"
    return cursor.document()->characterAt(currentPos) == '}' &&
           cursor.document()->characterAt(currentPos - 1) == '{';
}

// U slucaju da korisnik pritisne enter kod zagrada
void CodeEditor::handleEnterBetweenBraces()
{
    QTextCursor cursor = textCursor();

    // Dobijanje uvlačenja trenutne linije
    QString currentLineText = cursor.block().text();
    int indentLevel = 0;
    for (QChar ch : currentLineText)
    {
        if (ch == ' ')
        {
            indentLevel++;
        }
        else if (ch == '\t')
        {
            indentLevel += 4; // Pod pretpostavkom da je tab jednak 4 space-a
        }
        else
        {
            break;
        }
    }

    // Pomeri zatvorenu zagradu na sledeću liniju pre umetanja novog reda
    cursor.deleteChar(); // Brisanje postojeće zatvorene zagrade '}'
    cursor.insertText("\n" + QString(indentLevel + 4, ' ')); // Umetanje nove linije sa uvlačenjem unutar bloka
    cursor.insertText("\n" + QString(indentLevel, ' ') + '}'); // Zatvorena zagrada na sledećem nivou uvlačenja

    // Postavljanje kursora na poziciju unutar bloka
    cursor.movePosition(QTextCursor::Up);
    cursor.movePosition(QTextCursor::EndOfLine);
    setTextCursor(cursor);
}

// Metoda cija implementacija resava automatsko uvlacenje
void CodeEditor::handleAutoIndentation()
{
    QTextCursor cursor = this->textCursor();
    cursor.movePosition(QTextCursor::PreviousBlock);
    QString previousLineText = cursor.block().text();
    int indentLevel = 0;

    for (QChar ch : previousLineText)
    {
        if (ch == ' ')
        {
            indentLevel++;
        }
        else if (ch == '\t')
        {
            indentLevel += 4; // Pod pretpostavkom da je tab 4 space-a
        }
        else
        {
            break;
        }
    }

    if (indentLevel > 0)
    {
        insertPlainText(QString(indentLevel, ' ')); // Umetanje uvlačenja za novi red
    }
}



// Bocna leva strana koja prikazuje broj reda gde se tekst nalazi
int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());    // blockCount je metoda iz QPlainText i vraca gde je trenutno red lociran
    while (max >= 10)
    {
        max /= 10;
        ++digits;
    }

    // Sirina koja vec sada odredjujemo za pravougaonik
    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

// Moramo da imamo argument int, jer kao slot moramo da uhvatimo signal koji nam stize sa argumentom (mozda cemo kasnije i uraditi nesto)
void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

// Privatan slot koji mora imati dva argumenta, kako bi zadovoljio signal sa kojim se povezuje
void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
    {
        lineNumberArea->scroll(0, dy);
    }
    else
    {
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect()))
    {
        updateLineNumberAreaWidth(0);
    }
}

// Pokrivamo slucaj promene velicine prozora ovom metodom
void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect content = contentsRect();
    lineNumberArea->setGeometry(QRect(content.left(), content.top(), lineNumberAreaWidth(), content.height()));
}

// Vrsimo obradu bloka
void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor(133, 193, 233 ));

    QTextBlock block = firstVisibleBlock(); // QPlainText metoda
    int blockNumber = block.blockNumber();
    int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// Podvalaci liniju gde se nalazi kursor, postavlja boju
void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection;
        QColor lineColor = QColor(133, 193, 233 );
        QColor textColor = Qt::black;

        selection.format.setBackground(lineColor);
        selection.format.setForeground(textColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

// Metoda da uveca editor
void CodeEditor::zoomInText()
{
    QFont font = this->font();
    int newSize = font.pointSize() + 1;
    font.setPointSize(newSize);
    setFont(font);
}

// Smanji editor
void CodeEditor::zoomOutText()
{
    QFont font = this->font();
    int newSize = font.pointSize() - 1;
    font.setPointSize(newSize);
    setFont(font);
}

// Resetuje na podrazumevanu velicinu
void CodeEditor::resetZoom()
{
    QFont font = this->font();
    font.setPointSize(10);
    setFont(font);
}


void LineNumberArea::setUserFont(QFont& font)
{
    setFont(font);
}

void CodeEditor::setCopyShortcut(const QKeySequence &keySequence)
{
    copyAction->setShortcut(keySequence);  // Update shortcut
    qDebug() << "Copy shortcut je postavljen uspesno na:" << keySequence.toString();
}

void CodeEditor::setUndoShortcut(const QKeySequence &keySequence)
{
    undoAction->setShortcut(keySequence);  // Update shortcut
    qDebug() << "Undo shortcut je postavljen uspesno na:" << keySequence.toString();
}
