#include "highlighter.h"

Highlighter::Highlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {}

void Highlighter::highlightBlock(const QString &text)
{
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(0x569CD6));
    keywordFormat.setFontWeight(QFont::Bold);

    QStringList keywords = {"int", "float", "double", "if", "else", "for", "while", "class", "return", "void"};

    for (const QString &word : keywords) {
        QRegularExpression re("\\b" + word + "\\b");
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), keywordFormat);
        }
    }

    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(0xCE9178));

    QRegularExpression strRe("\".*?\"");
    auto it = strRe.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        setFormat(m.capturedStart(), m.capturedLength(), stringFormat);
    }
}
