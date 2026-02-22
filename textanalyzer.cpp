#include "textanalyzer.h"
#include <QRegularExpression>

TextType TextAnalyzer::detectType(const QString &text)
{
    QString t = text.trimmed();


    if(QRegularExpression("^[01\\s]+$").match(t).hasMatch())
        return TYPE_BINARY;


    if(QRegularExpression("^[0-9A-Fa-f\\s]+$").match(t).hasMatch())
        return TYPE_HEX;


    if(t.contains("\\u"))
        return TYPE_UNICODE;


    if(!t.isEmpty())
        return TYPE_TEXT;

    return TYPE_UNKNOWN;
}

QString TextAnalyzer::typeName(TextType type)
{
    switch(type) {
    case TYPE_TEXT: return "Text";
    case TYPE_HEX: return "Hex";
    case TYPE_BINARY: return "Binary";
    case TYPE_UNICODE: return "Unicode";
    default: return "Unknown";
    }
}
