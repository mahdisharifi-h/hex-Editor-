

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QStyleFactory>
#include <QTextStream>
#include <QScreen>
#include "home.h"
#include "textconverter.h"

namespace {

QString readInput(const QString &text, const QString &inputFile, QTextStream &err)
{
    if (!text.isEmpty()) {
        return text;
    }

    if (!inputFile.isEmpty()) {
        QFile file(inputFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            err << "Cannot open input file: " << inputFile << Qt::endl;
            return {};
        }

        return QString::fromUtf8(file.readAll());
    }

    return {};
}

bool writeOutput(const QString &outputPath, const QString &content, QTextStream &out, QTextStream &err)
{
    if (outputPath.isEmpty()) {
        out << content << Qt::endl;
        return true;
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        err << "Cannot write output file: " << outputPath << Qt::endl;
        return false;
    }

    file.write(content.toUtf8());
    out << "Saved output to: " << outputPath << Qt::endl;
    return true;
}

QString convertText(const QString &input, const QString &to, const QString &from)
{
    if (to == "hex") {
        return TextConverter::toHex(input, 1);
    }

    if (to == "binary") {
        return TextConverter::toBinary(input);
    }

    if (to == "unicode") {
        return TextConverter::toUnicode(input);
    }

    if (to == "text") {
        return TextConverter::toText(input, from);
    }

    return {};
}

int runTerminalMode(const QCommandLineParser &parser)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QString command = parser.value("command").trimmed().toLower();
    if (command.isEmpty()) {
        err << "Missing --command. Use convert or add." << Qt::endl;
        return 1;
    }

    if (command == "convert") {
        const QString to = parser.value("to").trimmed().toLower();
        const QString from = parser.value("from").trimmed().toLower();

        if (to.isEmpty()) {
            err << "Missing --to for convert command. Use: hex, binary, unicode, text." << Qt::endl;
            return 1;
        }

        if (to == "text" && from.isEmpty()) {
            err << "When --to text is selected, --from must be one of: hex, binary, unicode." << Qt::endl;
            return 1;
        }

        const QString input = readInput(parser.value("text"), parser.value("input-file"), err);
        if (input.isEmpty() && parser.value("text").isEmpty() && parser.value("input-file").isEmpty()) {
            err << "No input provided. Use --text or --input-file." << Qt::endl;
            return 1;
        }

        const QString output = convertText(input, to, from);
        if (output.isEmpty() && !input.isEmpty()) {
            err << "Unsupported conversion target: " << to << Qt::endl;
            return 1;
        }

        return writeOutput(parser.value("output"), output, out, err) ? 0 : 1;
    }

    if (command == "add") {
        const QString textValue = parser.value("text");
        const QString filePath = parser.value("input-file");

        if (textValue.isEmpty() && filePath.isEmpty()) {
            err << "Add command needs --text or --input-file." << Qt::endl;
            return 1;
        }

        QString payload;
        if (!textValue.isEmpty()) {
            payload += textValue;
        }

        if (!filePath.isEmpty()) {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                err << "Cannot open file for add command: " << filePath << Qt::endl;
                return 1;
            }

            if (!payload.isEmpty()) {
                payload += "\n";
            }
            payload += QString::fromUtf8(file.readAll());
        }

        const QString outputPath = parser.value("output");
        if (outputPath.isEmpty()) {
            out << payload << Qt::endl;
            return 0;
        }

        QFile outputFile(outputPath);
        if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            err << "Cannot append to output file: " << outputPath << Qt::endl;
            return 1;
        }

        outputFile.write(payload.toUtf8());
        outputFile.write("\n");
        out << "Added content to: " << outputPath << Qt::endl;
        return 0;
    }

    err << "Unsupported command: " << command << ". Supported commands: convert, add." << Qt::endl;
    return 1;
}

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCommandLineParser parser;

    parser.setApplicationDescription("Hex Editor GUI + Terminal mode");
    parser.addHelpOption();

    QCommandLineOption terminalModeOption(
        "terminal",
        "Run in terminal mode without opening GUI.");
    QCommandLineOption commandOption(
        "command",
        "Terminal command name: convert | add.",
        "command");
    QCommandLineOption textOption(
        "text",
        "Inline text input.",
        "text");
    QCommandLineOption inputFileOption(
        "input-file",
        "Read input from file.",
        "path");
    QCommandLineOption outputOption(
        "output",
        "Write output to file. For add command this file is appended.",
        "path");
    QCommandLineOption toOption(
        "to",
        "Convert destination type: hex | binary | unicode | text.",
        "type");
    QCommandLineOption fromOption(
        "from",
        "Source type when using --to text: hex | binary | unicode.",
        "type");

    parser.addOption(terminalModeOption);
    parser.addOption(commandOption);
    parser.addOption(textOption);
    parser.addOption(inputFileOption);
    parser.addOption(outputOption);
    parser.addOption(toOption);
    parser.addOption(fromOption);

    parser.process(app);

    if (parser.isSet(terminalModeOption)) {
        return runTerminalMode(parser);
    }

    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(
        "QMainWindow {"
        "  background-color: #141a22;"
        "  color: #e6edf3;"
        "}"
        "QStatusBar {"
        "  background-color: #0f141b;"
        "  color: #9fb0c3;"
        "  border-top: 1px solid #253041;"
        "}"
        "QMenuBar {"
        "  background-color: #0f141b;"
        "  color: #d8e2ee;"
        "  border-bottom: 1px solid #253041;"
        "}"
        "QMenuBar::item {"
        "  padding: 6px 10px;"
        "  border-radius: 6px;"
        "  margin: 2px;"
        "}"
        "QMenuBar::item:selected {"
        "  background: #1d2733;"
        "}"
        "QMenu {"
        "  background-color: #121923;"
        "  color: #d8e2ee;"
        "  border: 1px solid #2b3747;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #223245;"
        "}"
        "QTreeView, QPlainTextEdit {"
        "  background-color: #0f141b;"
        "  color: #d8e2ee;"
        "  border: 1px solid #273243;"
        "  selection-background-color: #264a72;"
        "  selection-color: #ffffff;"
        "  alternate-background-color: #111822;"
        "}"
        "QTreeView::item {"
        "  padding: 4px;"
        "}"
        "QHeaderView::section {"
        "  background-color: #1a2430;"
        "  color: #b8c6d6;"
        "  border: none;"
        "  border-right: 1px solid #2d3a4c;"
        "  padding: 6px;"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid #2a3443;"
        "  background: #111823;"
        "  border-radius: 8px;"
        "}"
        "QTabBar::tab {"
        "  background: #1a2230;"
        "  color: #b7c5d6;"
        "  padding: 8px 14px;"
        "  margin-right: 4px;"
        "  border-top-left-radius: 8px;"
        "  border-top-right-radius: 8px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #264a72;"
        "  color: white;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background: #243345;"
        "}"
        "QPushButton {"
        "  background-color: #2f81f7;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 6px 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #5396f9;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #1f6ed4;"
        "}"
        "QLineEdit {"
        "  background-color: #0f141b;"
        "  color: #d8e2ee;"
        "  border: 1px solid #2d3a4b;"
        "  border-radius: 8px;"
        "  padding: 6px 8px;"
        "  selection-background-color: #376da7;"
        "}"
        "QScrollBar:vertical {"
        "  background: #0f141b;"
        "  width: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #2a3a4f;"
        "  min-height: 24px;"
        "  border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #38506d;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        );


    Home w;
    w.resize(800, 600);
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();

    int x = (screenGeometry.width() - w.width()) / 2;
    int y = (screenGeometry.height() - w.height()) / 2;

    w.move(x, y);
    w.show();

    return app.exec();
}

