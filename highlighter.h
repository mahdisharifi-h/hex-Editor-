#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>

class Highlighter : public QSyntaxHighlighter
{
public:
    Highlighter(QTextDocument *parent=nullptr);
protected:
    void highlightBlock(const QString &text) override;
};

#endif
