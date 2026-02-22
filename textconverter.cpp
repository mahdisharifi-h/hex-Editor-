#include "textconverter.h"
#include <QChar>
#include <QStringList>
#include <QByteArray>



QString TextConverter::toBinary(const QString &text) {
    QString result;
    QByteArray data = text.toUtf8();
    for (unsigned char c : data) {
        result += QString("%1 ").arg(c, 8, 2, QLatin1Char('0'));
    }
    return result.trimmed();
}

QString TextConverter::fromBinary(const QString &binary) {
    QString result;
    QStringList list = binary.split(" ", Qt::SkipEmptyParts);
    QByteArray data;
    for (const QString &b : list) {
        bool ok;
        data.append(static_cast<char>(b.toUInt(&ok, 2)));
    }
    return QString::fromUtf8(data);
}

QString TextConverter::toHex(const QString &text, int bytesPerGroup) {
    QString result;
    QByteArray data = text.toUtf8();
    for (int i = 0; i < data.size(); ++i) {
        result += QString("%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0')).toUpper();
        if ((i + 1) % bytesPerGroup == 0) result += " ";
    }
    return result.trimmed();
}

QString TextConverter::fromHex(const QString &hex) {
    QString clean = hex;
    clean.remove(' ').remove('\n');

    return QString::fromUtf8(QByteArray::fromHex(clean.toUtf8()));
}

QString TextConverter::toUnicode(const QString &text) {
    QString result;
    for (QChar c : text) {
        result += QString("\\u%1").arg(static_cast<ushort>(c.unicode()), 4, 16, QChar('0'));
    }
    return result;
}

QString TextConverter::fromUnicode(const QString &unicode) {
    QString result;

    QStringList parts = unicode.split("\\u", Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        bool ok;
        ushort code = static_cast<ushort>(p.left(4).toUInt(&ok, 16));
        if (ok) result += QChar(code);
        if (p.length() > 4) {
            result += p.mid(4);
        }
    }
    return result;
}

QString TextConverter::toText(const QString &text, const QString &format) {
    if (format == "hex") return fromHex(text);
    if (format == "binary") return fromBinary(text);
    if (format == "unicode") return fromUnicode(text);
    return text;
}
