#include "codeeditor.h"
#include <QPainter>
#include <QTextBlock>
#include <QTextDocument>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QTextLayout>

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void CodeEditor::setByteGroupingMode(ByteGroupingMode mode) {
    groupingMode = mode;
}

CodeEditor::ByteGroupingMode CodeEditor::byteGroupingMode() const {
    return groupingMode;
}

int CodeEditor::expectedTokenLength() const {
    switch (groupingMode) {
    case GroupingHex:
        return 2;
    case GroupingBinary:
        return 8;
    case GroupingUnicode:
        return 6;
    case GroupingText:
    default:
        return 0;
    }
}

QChar CodeEditor::groupingSeparator() const {
    if (groupingMode == GroupingUnicode) {
        return QChar();
    }
    return QLatin1Char(' ');
}

bool CodeEditor::isValidTokenCharacter(const QChar &ch) const {
    switch (groupingMode) {
    case GroupingHex:
        return ((ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'))
                || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f')));
    case GroupingBinary:
        return ch == QLatin1Char('0') || ch == QLatin1Char('1');
    case GroupingUnicode:
        return ch == QLatin1Char('\\')
               || ch == QLatin1Char('u')
               || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
               || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'))
               || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'));
    case GroupingText:
    default:
        return true;
    }
}

void CodeEditor::keyPressEvent(QKeyEvent *event) {
    const int tokenLength = expectedTokenLength();
    QTextCursor cursor = textCursor();
    const QString enteredText = event->text();

    if (event->key() == Qt::Key_A && event->modifiers() == Qt::ControlModifier) {
        selectAll();
        updateSelections();

        if (cursor.atEnd()) {
            cursor.setPosition(document()->characterCount() - 1);
            setTextCursor(cursor);
        }
        return;
    }

    if (isReadOnly() || tokenLength <= 0 || enteredText.isEmpty()) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    QChar ch = enteredText.at(0);

    if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) && !cursor.hasSelection()) {
        const int step = tokenLength + (groupingSeparator().isNull() ? 0 : 1);

        if (event->key() == Qt::Key_Backspace) {
            const int removeCount = qMin(step, cursor.position());
            if (removeCount > 0) {
                cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, removeCount);
                cursor.removeSelectedText();
                setTextCursor(cursor);
            }
        } else {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, step);
            if (cursor.hasSelection()) {
                cursor.removeSelectedText();
                setTextCursor(cursor);
            }
        }
        return;
    }

    if (groupingMode == GroupingUnicode) {
        if (ch == '\\' && !cursor.hasSelection()) {
            cursor.insertText("\\u");
            setTextCursor(cursor);
            return;
        }

        if ((ch.isDigit() || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'))) {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        if (event->key() == Qt::Key_Space) {
            QTextBlock block = cursor.block();
            int posInBlock = cursor.position() - block.position();
            QString text = block.text();

            int tokenStart = text.lastIndexOf("\\u", posInBlock - 1);
            if (tokenStart < 0) tokenStart = posInBlock - 1;

            int indexInToken = cursor.position() - (block.position() + tokenStart);
            int remaining = tokenLength - indexInToken;
            if (remaining > 0) {
                QString fill(remaining, '0');
                cursor.insertText(fill);
                setTextCursor(cursor);
            }
            return;
        }

        return;
    }

    const QChar separator = groupingSeparator();
    int blockStart = (cursor.position() / (tokenLength + 1)) * (tokenLength + 1);
    int indexInToken = cursor.position() - blockStart;

    if (event->key() == Qt::Key_Space || indexInToken >= tokenLength) {
        int remaining = tokenLength - indexInToken;
        QString fill = remaining > 0 ? QString(remaining, QLatin1Char('0')) : QString();
        if (!separator.isNull()) fill += separator;

        cursor.insertText(fill);
        setTextCursor(cursor);
        return;
    }

    if (!separator.isNull() && indexInToken + 1 == tokenLength) {
        cursor.insertText(QString(separator));
        setTextCursor(cursor);
    }

    QPlainTextEdit::keyPressEvent(event);
}
int CodeEditor::visibleLineCount() const {
    int lines = 0;
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        const QTextLayout *layout = block.layout();
        lines += qMax(1, layout ? layout->lineCount() : 1);
        block = block.next();
    }
    return qMax(1, lines);
}

int CodeEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = visibleLineCount();
    while (max >= 10) {
        max /= 10;
        digits++;
    }
    const int space = 8 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int ) {
    const int width = lineNumberAreaWidth();
    if (layoutDirection() == Qt::RightToLeft) {
        setViewportMargins(0, 0, width, 0);
    } else {
        setViewportMargins(width, 0, 0, 0);
    }
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e) {
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    const int width = lineNumberAreaWidth();
    const int x = (layoutDirection() == Qt::RightToLeft) ? (cr.right() - width + 1) : cr.left();
    lineNumberArea->setGeometry(QRect(x, cr.top(), width, cr.height()));
}

void CodeEditor::highlightCurrentLine() {
    updateSelections();
}

void CodeEditor::setSearchText(const QString &query) {
    searchQuery = query;
    updateSelections();
}

int CodeEditor::searchMatchCount() const {
    return buildSearchSelections().size();
}

int CodeEditor::currentSearchMatchIndex() const {
    if (searchQuery.isEmpty()) {
        return 0;
    }

    const QList<QTextEdit::ExtraSelection> selections = buildSearchSelections();
    if (selections.isEmpty()) {
        return 0;
    }

    return findCurrentMatchIndex(selections);
}

bool CodeEditor::jumpToNextSearchMatch() {
    const auto selections = buildSearchSelections();
    if (selections.isEmpty()) return false;

    int currentIndex = findCurrentMatchIndex(selections);
    int nextIndex = (currentIndex + 1) % selections.size();

    setTextCursor(selections[nextIndex].cursor);
    centerCursor();
    return true;
}

bool CodeEditor::jumpToPreviousSearchMatch() {
    const auto selections = buildSearchSelections();
    if (selections.isEmpty()) return false;

    int currentIndex = findCurrentMatchIndex(selections);
    int prevIndex = (currentIndex - 1 + selections.size()) % selections.size();

    setTextCursor(selections[prevIndex].cursor);
    centerCursor();
    return true;
}

QString CodeEditor::buildFlexibleSearchPattern(const QString &query) const {
    QString pattern;
    pattern.reserve(query.size() * 6);

    for (const QChar ch : query) {
        switch (ch.unicode()) {
        case 0x064A:
        case 0x06CC:
            pattern += "[\\x{064A}\\x{06CC}]";
            break;
        case 0x0643:
        case 0x06A9:
            pattern += "[\\x{0643}\\x{06A9}]";
            break;
        case 0x0629:
        case 0x0647:
            pattern += "[\\x{0629}\\x{0647}]";
            break;
        case 0x0623:
        case 0x0625:
        case 0x0622:
        case 0x0627:
            pattern += "[\\x{0623}\\x{0625}\\x{0622}\\x{0627}]";
            break;
        default:
            pattern += QRegularExpression::escape(QString(ch));
            break;
        }
    }

    return pattern;
}

QList<QTextEdit::ExtraSelection> CodeEditor::buildSearchSelections() const {
    QList<QTextEdit::ExtraSelection> selections;
    if (searchQuery.isEmpty()) {
        return selections;
    }

    QTextCursor cursor(document());
    QTextCharFormat format;
    format.setBackground(QColor(255, 235, 59));
    format.setForeground(Qt::black);

    const QString flexiblePattern = buildFlexibleSearchPattern(searchQuery);
    QRegularExpression queryExpression(
        flexiblePattern,
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption
        );

    while (!cursor.isNull() && !cursor.atEnd()) {
        cursor = document()->find(queryExpression, cursor);
        if (!cursor.isNull()) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format = format;
            selections.append(selection);
        }
    }

    return selections;
}

int CodeEditor::findCurrentMatchIndex(const QList<QTextEdit::ExtraSelection> &selections) const {
    if (selections.isEmpty())
        return -1;

    const QTextCursor current = textCursor();
    const int curPos = current.position();

    for (int i = 0; i < selections.size(); ++i) {
        const QTextCursor selCursor = selections[i].cursor;
        if (selCursor.selectionStart() <= curPos && curPos <= selCursor.selectionEnd()) {
            return i;
        }
    }


    for (int i = 0; i < selections.size(); ++i) {
        if (selections[i].cursor.selectionStart() > curPos)
            return i;
    }

    return 0;
}
void CodeEditor::updateSelections() {
    QList<QTextEdit::ExtraSelection> extraSelections = buildSearchSelections();

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection currentLineSelection;
        currentLineSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
        currentLineSelection.cursor = textCursor();
        currentLineSelection.cursor.clearSelection();
        extraSelections.append(currentLineSelection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), palette().alternateBase());

    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());

    int visualLineNumber = 1;
    QTextBlock counter = document()->begin();
    while (counter.isValid() && counter != block) {
        const QTextLayout *layout = counter.layout();
        visualLineNumber += qMax(1, layout ? layout->lineCount() : 1);
        counter = counter.next();
    }

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible()) {
            const QTextLayout *layout = block.layout();
            const int lineCount = qMax(1, layout ? layout->lineCount() : 1);

            for (int i = 0; i < lineCount; ++i) {
                int lineTop = top;
                int lineHeight = fontMetrics().height();

                if (layout && i < layout->lineCount()) {
                    const QTextLine textLine = layout->lineAt(i);
                    lineTop = top + qRound(textLine.y());
                    lineHeight = qRound(textLine.height());
                }

                const int lineBottom = lineTop + lineHeight;
                if (lineBottom >= event->rect().top() && lineTop <= event->rect().bottom()) {
                    const QString number = QString::number(visualLineNumber);
                    painter.setPen(Qt::black);
                    painter.drawText(0, lineTop, lineNumberArea->width() - 4, lineHeight,
                                     Qt::AlignRight | Qt::AlignVCenter, number);
                }

                ++visualLineNumber;
            }
        }

        top += qRound(blockBoundingRect(block).height());
        block = block.next();
    }
}

void CodeEditor::highlightBinary() {}
void CodeEditor::highlightHex() {}
void CodeEditor::highlightUnicode() {}
