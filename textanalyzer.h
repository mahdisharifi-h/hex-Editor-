#ifndef TEXTANALYZER_H
#define TEXTANALYZER_H

#include <QString>

enum TextType {
    TYPE_TEXT,
    TYPE_HEX,
    TYPE_BINARY,
    TYPE_UNICODE,
    TYPE_UNKNOWN
};

class TextAnalyzer
{
public:
    static TextType detectType(const QString &text);
    static QString typeName(TextType type);
};

#endif
