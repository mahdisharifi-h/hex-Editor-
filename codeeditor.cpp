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
    if (isReadOnly() || tokenLength <= 0) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Space && groupingMode != GroupingUnicode) {
        QTextCursor cursor = textCursor();
        if (!cursor.hasSelection()) {
            const int blockStart = (cursor.position() / (tokenLength + 1)) * (tokenLength + 1);
            const int indexInToken = cursor.position() - blockStart;

            if (indexInToken > 0 && indexInToken < tokenLength) {
                QString zeros(tokenLength - indexInToken, QLatin1Char('0'));
                zeros += QLatin1Char(' ');
                cursor.insertText(zeros);
                setTextCursor(cursor);
                return;
            }
        }
    }

    const QString enteredText = event->text();
    QChar c = enteredText.at(0);
    QChar::Category cat = c.category();
    if (cat != QChar::Other_Control) {
        const QChar ch = enteredText.at(0);
        const QChar separator = groupingSeparator();

        if ((separator.isNull() || ch != separator) && !isValidTokenCharacter(ch)) {
            return;
        }

        if (!separator.isNull() && ch != separator) {
            QTextCursor cursor = textCursor();
            if (!cursor.hasSelection()) {
                const int groupSize = tokenLength + 1;
                const int indexInToken = cursor.position() % groupSize;
                if (indexInToken == tokenLength) {
                    cursor.insertText(QString(separator));
                    setTextCursor(cursor);
                }
            }
        }
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

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
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
    if (searchQuery.isEmpty()) {
        return false;
    }

    const QList<QTextEdit::ExtraSelection> selections = buildSearchSelections();
    if (selections.isEmpty()) {
        return false;
    }

    int nextIndex = findCurrentMatchIndex(selections);
    if (nextIndex >= selections.size()) {
        nextIndex = 1;
    }

    QTextCursor targetCursor = selections[nextIndex - 1].cursor;
    setTextCursor(targetCursor);
    centerCursor();
    return true;
}

bool CodeEditor::jumpToPreviousSearchMatch() {
    if (searchQuery.isEmpty()) {
        return false;
    }

    const QList<QTextEdit::ExtraSelection> selections = buildSearchSelections();
    if (selections.isEmpty()) {
        return false;
    }

    const int currentIndex = findCurrentMatchIndex(selections);
    int previousIndex = currentIndex - 1;
    if (previousIndex <= 0) {
        previousIndex = selections.size();
    }

    QTextCursor targetCursor = selections[previousIndex - 1].cursor;
    setTextCursor(targetCursor);
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
    if (selections.isEmpty()) {
        return 0;
    }

    const QTextCursor current = textCursor();
    const int currentPos = current.position();
    const int anchorPos = current.anchor();

    for (int i = 0; i < selections.size(); ++i) {
        const QTextCursor matchCursor = selections[i].cursor;
        if (matchCursor.selectionStart() == qMin(currentPos, anchorPos)
            && matchCursor.selectionEnd() == qMax(currentPos, anchorPos)) {
            return i + 1;
        }
    }

    for (int i = 0; i < selections.size(); ++i) {
        if (selections[i].cursor.selectionStart() >= currentPos) {
            return i + 1;
        }
    }

    return 1;
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
