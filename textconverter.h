#ifndef TEXTCONVERTER_H
#define TEXTCONVERTER_H

#include <QString>


class TextConverter {
public:

    static QString toBinary(const QString &text);
    static QString fromBinary(const QString &binary);
    static QString toHex(const QString &text, int bytesPerGroup = 1);
    static QString fromHex(const QString &hex);
    static QString toUnicode(const QString &text);
    static QString fromUnicode(const QString &unicode);
    static QString toText(const QString &text, const QString &format);

private:


};


#endif
