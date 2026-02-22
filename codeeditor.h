#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QWidget>
#include <QList>

class LineNumberArea;

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    enum ByteGroupingMode {
        GroupingText,
        GroupingHex,
        GroupingBinary,
        GroupingUnicode
    };

    CodeEditor(QWidget *parent = nullptr);
    void setByteGroupingMode(ByteGroupingMode mode);
    ByteGroupingMode byteGroupingMode() const;

    void highlightBinary();
    void highlightHex();
    void highlightUnicode();

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void setSearchText(const QString &query);
    int searchMatchCount() const;
    int currentSearchMatchIndex() const;
    bool jumpToNextSearchMatch();
    bool jumpToPreviousSearchMatch();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    int visibleLineCount() const;
    void updateSelections();
    QList<QTextEdit::ExtraSelection> buildSearchSelections() const;
    QString buildFlexibleSearchPattern(const QString &query) const;
    int findCurrentMatchIndex(const QList<QTextEdit::ExtraSelection> &selections) const;

    QWidget *lineNumberArea;
    QString searchQuery;
    ByteGroupingMode groupingMode = GroupingText;

    int expectedTokenLength() const;
    QChar groupingSeparator() const;
    bool isValidTokenCharacter(const QChar &ch) const;
};

class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}
    QSize sizeHint() const override { return QSize(codeEditor->lineNumberAreaWidth(), 0); }
protected:
    void paintEvent(QPaintEvent *event) override { codeEditor->lineNumberAreaPaintEvent(event); }
private:
    CodeEditor *codeEditor;
};

#endif
