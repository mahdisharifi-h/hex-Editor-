#include "home.h"
#include "textconverter.h"
#include <QSplitter>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QRegularExpression>
#include "QTimer"
#include "QScrollBar"
#include "QSettings"
#include "textanalyzer.h"
#include "QMessageBox"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QDesktopServices>
#include <QtMath>
#include <QLabel>
#include <QChar>
#include <QStatusBar>

namespace {

bool isValidHexQuery(const QString &query) {
    QString clean = query;
    clean.remove(QRegularExpression("\\s+"));
    if (clean.isEmpty() || (clean.size() % 2) != 0) {
        return false;
    }
    return QRegularExpression("^[0-9A-Fa-f]+$").match(clean).hasMatch();
}

bool isValidBinaryQuery(const QString &query) {
    QString clean = query;
    clean.remove(QRegularExpression("\\s+"));
    if (clean.isEmpty() || (clean.size() % 8) != 0) {
        return false;
    }
    return QRegularExpression("^[01]+$").match(clean).hasMatch();
}

bool isValidUnicodeQuery(const QString &query) {
    if (!query.contains("\\u")) {
        return false;
    }

    const QRegularExpression unicodeToken("(?:\\\\u[0-9A-Fa-f]{4})+");
    const auto match = unicodeToken.match(query.trimmed());
    return match.hasMatch() && match.capturedLength() == query.trimmed().length();
}

QString convertQueryToText(const QString &query, TextType type) {
    switch (type) {
    case TYPE_HEX:
        return isValidHexQuery(query) ? TextConverter::fromHex(query) : QString();
    case TYPE_BINARY:
        return isValidBinaryQuery(query) ? TextConverter::fromBinary(query) : QString();
    case TYPE_UNICODE:
        return isValidUnicodeQuery(query) ? TextConverter::fromUnicode(query) : QString();
    case TYPE_TEXT:
        return query;
    default:
        return QString();
    }
}

QString convertTextToTarget(const QString &text, TextType targetType) {
    switch (targetType) {
    case TYPE_HEX:
        return TextConverter::toHex(text, 1);
    case TYPE_BINARY:
        return TextConverter::toBinary(text);
    case TYPE_UNICODE:
        return TextConverter::toUnicode(text);
    case TYPE_TEXT:
    case TYPE_UNKNOWN:
    default:
        return text;
    }
}

QPair<int, QString> countOccurrencesInRecentFile(const QString &path, const QString &needleText) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return qMakePair(0, QString());
    }

    const QString content = QString::fromUtf8(file.readAll());
    if (content.isEmpty() || needleText.isEmpty()) {
        return qMakePair(0, QString());
    }

    int count = 0;
    int pos = 0;
    while ((pos = content.indexOf(needleText, pos, Qt::CaseInsensitive)) != -1) {
        ++count;
        pos += needleText.length();
    }

    return qMakePair(count, QFileInfo(path).fileName());
}

TextType detectSearchQueryType(const QString &query) {
    if (isValidUnicodeQuery(query)) {
        return TYPE_UNICODE;
    }
    if (isValidBinaryQuery(query)) {
        return TYPE_BINARY;
    }
    if (isValidHexQuery(query)) {
        return TYPE_HEX;
    }
    return TYPE_TEXT;
}

int chunkSizeForType(TextType type) {
    switch (type) {
    case TYPE_BINARY:
        return 9;
    case TYPE_HEX:
        return 3;
    case TYPE_UNICODE:
        return 6;
    case TYPE_TEXT:
    case TYPE_UNKNOWN:
    default:
        return 1;
    }
}

int detectChunkSizeForEditor(const CodeEditor *editor) {
    if (!editor) {
        return 1;
    }

    return chunkSizeForType(TextAnalyzer::detectType(editor->toPlainText()));
}

void alignSelectionToChunk(QTextCursor &cursor, int chunkSize, int docLength) {
    if (chunkSize <= 1 || !cursor.hasSelection()) {
        return;
    }

    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    start = (start / chunkSize) * chunkSize;
    end = ((end + chunkSize - 1) / chunkSize) * chunkSize;
    end = qBound(0, end, qMax(0, docLength - 1));

    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
}

void alignCursorToChunk(QTextCursor &cursor, int chunkSize, int docLength) {
    if (chunkSize <= 1) {
        return;
    }

    int pos = cursor.position();
    pos = (pos / chunkSize) * chunkSize;
    pos = qBound(0, pos, qMax(0, docLength - 1));
    cursor.setPosition(pos);
}

}

Home::Home(QWidget *parent) : QMainWindow(parent) {
    QSettings settings("MyCompany", "MyApplication");
    recentFiles = settings.value("history/recentFiles").toStringList();
    model = new QFileSystemModel(this);
    model->setRootPath("");

    tree = new QTreeView();
    tree->setModel(model);
    tree->setAnimated(true);
    tree->setAlternatingRowColors(true);
    tree->setUniformRowHeights(true);

    recentSearchResults = new QListWidget(this);
    recentSearchResults->setVisible(false);
    recentSearchResults->setMaximumHeight(180);
    recentSearchResults->setObjectName("recentSearchResults");
    connect(recentSearchResults, &QListWidget::itemClicked, this, &Home::openRecentSearchResult);

    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    leftLayout->addWidget(tree);
    leftLayout->addWidget(recentSearchResults);

    tabs = new QTabWidget();
    tabs->setTabsClosable(true);
    tabs->setDocumentMode(true);
    connect(tabs, &QTabWidget::tabCloseRequested, [=](int index){
        tabs->removeTab(index);
        updateui();
    });

    QSplitter *mainSplit = new QSplitter(Qt::Horizontal);
    mainSplit->addWidget(leftPanel);
    mainSplit->addWidget(tabs);
    mainSplit->setStretchFactor(1, 1);
    setCentralWidget(mainSplit);

    menuBarObj = new MenuBar(this);
    connect(menuBarObj, &MenuBar::triggered, this, &Home::menu);

    connect(tree, &QTreeView::doubleClicked, [=](const QModelIndex &index) {
        QString path = model->filePath(index);
        if (QFileInfo(path).isFile()) openFile(path);
    });
    connect(tabs, &QTabWidget::currentChanged, this, &Home::onTabChanged);

    searchBarWidget = new QWidget(this);
    searchBarWidget->setObjectName("searchBarWidget");
    QHBoxLayout *searchLayout = new QHBoxLayout(searchBarWidget);
    searchLayout->setContentsMargins(8, 4, 8, 4);
    QLabel *searchLabel = new QLabel("Search:", searchBarWidget);
    searchLabel->setObjectName("searchLabel");
    searchLayout->addWidget(searchLabel);

    searchInput = new QLineEdit(searchBarWidget);
    searchInput->setClearButtonEnabled(true);
    searchInput->setPlaceholderText("Type to highlight matches...");
    searchLayout->addWidget(searchInput);

    QPushButton *prevSearchBtn = new QPushButton("▼", searchBarWidget);
    prevSearchBtn->setFixedWidth(28);
    searchLayout->addWidget(prevSearchBtn);

    QPushButton *nextSearchBtn = new QPushButton("▲", searchBarWidget);
    nextSearchBtn->setFixedWidth(28);
    searchLayout->addWidget(nextSearchBtn);

    searchStatusLabel = new QLabel("0/0", searchBarWidget);
    searchStatusLabel->setObjectName("searchStatusLabel");
    searchLayout->addWidget(searchStatusLabel);

    QPushButton *closeSearchBtn = new QPushButton("✕", searchBarWidget);
    closeSearchBtn->setFixedWidth(28);
    searchLayout->addWidget(closeSearchBtn);

    statusBar()->addPermanentWidget(searchBarWidget, 1);
    searchBarWidget->hide();
    searchBarWidget->setStyleSheet(
        "#searchBarWidget {"
        "  background-color: #0f141b;"
        "  border: 1px solid #2d3a4b;"
        "  border-radius: 8px;"
        "}"
        "#searchLabel {"
        "  color: #9fb0c3;"
        "  font-weight: 600;"
        "}"
        "#searchStatusLabel {"
        "  color: #9fb0c3;"
        "  min-width: 56px;"
        "  padding: 0 4px;"
        "}"
        );

    connect(searchInput, &QLineEdit::textChanged, this, [this](const QString &){
        applySearchToCurrentTab();
        updateRecentSearchResults();
    });

    connect(prevSearchBtn, &QPushButton::clicked, this, [this]() {
        navigateSearchMatch(false);
    });

    connect(nextSearchBtn, &QPushButton::clicked, this, [this]() {
        navigateSearchMatch(true);
    });

    connect(closeSearchBtn, &QPushButton::clicked, this, [this]() {
        searchInput->clear();
        searchBarWidget->hide();
        applySearchToCurrentTab();
        updateRecentSearchResults();
        updateSearchStatus();
    });

    updateui();
}

void Home::applyEditorGrouping(CodeEditor *editor, EditorMode mode) {
    if (!editor) {
        return;
    }

    switch (mode) {
    case ModeHex:
        editor->setByteGroupingMode(CodeEditor::GroupingHex);
        break;
    case ModeBinary:
        editor->setByteGroupingMode(CodeEditor::GroupingBinary);
        break;
    case ModeUnicode:
        editor->setByteGroupingMode(CodeEditor::GroupingUnicode);
        break;
    case ModeText:
    default:
        editor->setByteGroupingMode(CodeEditor::GroupingText);
        break;
    }
}

void Home::openFile(const QString &path) {
    QFile f(path);
    int index = tabs->currentIndex();
    tabStates[index].filePath = path;
    addToHistory(path);


    if (!f.open(QIODevice::ReadOnly)) return;

    QByteArray data = f.readAll();
    originalText = QString::fromUtf8(data);
    currentFile = path;

    QSplitter *editorSplit = new QSplitter(Qt::Horizontal);
    CodeEditor *leftEd = new CodeEditor();
    CodeEditor *rightEd = new CodeEditor();

    leftEd->setByteGroupingMode(CodeEditor::GroupingText);
    applyEditorGrouping(rightEd, ModeHex);

    leftEd->setPlainText(originalText);
    rightEd->setPlainText(TextConverter::toHex(originalText, 1));

    editorSplit->addWidget(leftEd);
    editorSplit->addWidget(rightEd);
    tabs->addTab(editorSplit, QFileInfo(path).fileName());


    connect(leftEd, &CodeEditor::cursorPositionChanged, this, &Home::onCursorChanged);
    connect(rightEd, &CodeEditor::cursorPositionChanged, this, &Home::onCursorChanged);
    connect(leftEd, &CodeEditor::textChanged, this, &Home::onEditorTextChanged);
    connect(rightEd, &CodeEditor::textChanged, this, &Home::onEditorTextChanged);
    connect(leftEd->verticalScrollBar(), &QScrollBar::valueChanged,
            rightEd->verticalScrollBar(), &QScrollBar::setValue);
    connect(rightEd->verticalScrollBar(), &QScrollBar::valueChanged,
            leftEd->verticalScrollBar(), &QScrollBar::setValue);

    updateui();
    applySearchToCurrentTab();

}

void Home::onCursorChanged() {
    QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
    if (!split) return;

    CodeEditor *leftEd = qobject_cast<CodeEditor*>(split->widget(0));
    CodeEditor *rightEd = qobject_cast<CodeEditor*>(split->widget(1));

    if (!leftEd || !rightEd) return;

    if (leftEd->hasFocus()) {
        lastActiveEditor = leftEd;
        tabStates[tabs->currentIndex()].lastSearchFromRight = false;
        syncEditors(leftEd, rightEd);
    }
    else if (rightEd->hasFocus()) {
        lastActiveEditor = rightEd;
        tabStates[tabs->currentIndex()].lastSearchFromRight = true;
        syncEditors(rightEd, leftEd);
    }

    updateSearchStatus();
}

void Home::syncEditors(CodeEditor *source, CodeEditor *target) {
    if (!source || !target) return;

    QSignalBlocker blocker(target);
    int currentIndex = tabs->currentIndex();

    EditorMode activeMode = tabStates[currentIndex].mode;

    QTextCursor sc = source->textCursor();
    QTextCursor tc = target->textCursor();

    int factor = 3;

    if (activeMode == 1) factor = 9;
    else if (activeMode == 2) factor = 6;
    else if (activeMode == 3) factor = 1;


    QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
    if(!split) return;
    bool isRightToLeft = (source == split->widget(1));
    int sourceDocLength = source->document()->characterCount();
    int targetDocLength = target->document()->characterCount();

    /*if (isRightToLeft) {
        const int chunkSize = detectChunkSizeForEditor(source);

        if (sc.hasSelection()) {
            alignSelectionToChunk(sc, chunkSize, sourceDocLength);
        } else {
            alignCursorToChunk(sc, chunkSize, sourceDocLength);
        }
    }*/



    if (sc.hasSelection()) {
        int startPos = sc.selectionStart();
        int endPos = sc.selectionEnd();
        int newStart, newEnd;


        //uonisod ==3  hex ==1  text==0 bayj ===2

        if (isRightToLeft) {

            if(activeMode == 3 && TextAnalyzer::detectType(target->toPlainText())==0){
                if (TextAnalyzer::detectType(source->toPlainText())==0 )factor = 1 ;
                else if (TextAnalyzer::detectType(source->toPlainText())==1 )factor = 3;
                else if (TextAnalyzer::detectType(source->toPlainText())==2 )factor = 9;
                else if (TextAnalyzer::detectType(source->toPlainText())==3 )factor = 6;
                newStart = startPos * factor;

                newEnd = endPos * factor;

                newEnd = newEnd == targetDocLength ?newEnd-1:newEnd;
                newEnd = round(newEnd);
            }

            else if(activeMode == 3 && TextAnalyzer::detectType(target->toPlainText())==1){
                if (TextAnalyzer::detectType(source->toPlainText())==0 )factor = 3 ;
                else if (TextAnalyzer::detectType(source->toPlainText())==1 )factor = 1;
                else if (TextAnalyzer::detectType(source->toPlainText())==2 )factor = 9;
                else if (TextAnalyzer::detectType(source->toPlainText())==3 )factor = 6;
                newStart = startPos * factor;

                newEnd = endPos * factor;

                newEnd = newEnd == targetDocLength ?newEnd-1:newEnd;
                newEnd = round(newEnd);

            }
            else if(activeMode == 3 && TextAnalyzer::detectType(target->toPlainText())==2){
                if (TextAnalyzer::detectType(source->toPlainText())==0 )factor = 9 ;
                else if (TextAnalyzer::detectType(source->toPlainText())==1 )factor = 3;
                else if (TextAnalyzer::detectType(source->toPlainText())==2 )factor = 1;
                else if (TextAnalyzer::detectType(source->toPlainText())==3 )factor = 6;

                newStart = startPos * factor;

                newEnd = endPos * factor;

                newEnd = newEnd == targetDocLength ?newEnd-1:newEnd;
                newEnd = round(newEnd);
            }
            else if(activeMode == 3 && TextAnalyzer::detectType(target->toPlainText())==3 ){

                if (TextAnalyzer::detectType(source->toPlainText())==0 )factor = 1 ;
                else if (TextAnalyzer::detectType(source->toPlainText())==1 )factor = 6;
                else if (TextAnalyzer::detectType(source->toPlainText())==2 )factor = 9;
                else if (TextAnalyzer::detectType(source->toPlainText())==3 )factor = 6;
                newStart = startPos * factor;

                newEnd = endPos * factor;

                newEnd = newEnd == targetDocLength ?newEnd-1:newEnd;
                newEnd = round(newEnd);
            }

            else{
                newStart = startPos / factor;
                newEnd =endPos / factor;
                newEnd = newEnd+2 == targetDocLength ?newEnd+1:newEnd;
                newEnd = round(newEnd);

            }

        } else {

            if(activeMode == 3 && TextAnalyzer::detectType(target->toPlainText())==0){
                if (TextAnalyzer::detectType(source->toPlainText())==0 )factor = 1 ;
                else if (TextAnalyzer::detectType(source->toPlainText())==1 )factor = 3;
                else if (TextAnalyzer::detectType(source->toPlainText())==2 )factor = 9;
                else if (TextAnalyzer::detectType(source->toPlainText())==3 )factor = 6;
                newStart = startPos / factor;
                newEnd =endPos / factor;
                newEnd = newEnd+2 == targetDocLength ?newEnd+1:newEnd;
                newEnd = round(newEnd);
            }
            else if(activeMode == 3 && TextAnalyzer::detectType(target->toPlainText())==1){
                if (TextAnalyzer::detectType(source->toPlainText())==0 )factor = 3 ;
                else if (TextAnalyzer::detectType(source->toPlainText())==1 )factor = 1;
                else if (TextAnalyzer::detectType(source->toPlainText())==2 )factor = 9;
                else if (TextAnalyzer::detectType(source->toPlainText())==3 )factor = 6;

                newStart = startPos / factor;
                newEnd =endPos / factor;
                newEnd = newEnd+2 == targetDocLength ?newEnd+1:newEnd;
                newEnd = round(newEnd);

            }
            else if(activeMode == 3 && TextAnalyzer::detectType(target->toPlainText())==2){
                if (TextAnalyzer::detectType(source->toPlainText())==0 )factor = 9 ;
                else if (TextAnalyzer::detectType(source->toPlainText())==1 )factor = 3;
                else if (TextAnalyzer::detectType(source->toPlainText())==2 )factor = 1;
                else if (TextAnalyzer::detectType(source->toPlainText())==3 )factor = 6;

                newStart = startPos / factor;
                newEnd =endPos / factor;
                newEnd = newEnd+2 == targetDocLength ?newEnd+1:newEnd;
                newEnd = round(newEnd);
            }
            else if(activeMode == 3 && TextAnalyzer::detectType(target->toPlainText())==3 ){
                if (TextAnalyzer::detectType(source->toPlainText())==0 )factor = 1 ;
                else if (TextAnalyzer::detectType(source->toPlainText())==1 )factor = 3;
                else if (TextAnalyzer::detectType(source->toPlainText())==2 )factor = 9;
                else if (TextAnalyzer::detectType(source->toPlainText())==3 )factor = 6;

                newStart = startPos / factor;
                newEnd =endPos / factor;
                newEnd = newEnd+2 == targetDocLength ?newEnd+1:newEnd;
                newEnd = round(newEnd);
            }

            else{

                newStart = startPos * factor;

                newEnd = endPos * factor;

                newEnd = newEnd == targetDocLength ?newEnd-1:newEnd;
                newEnd = round(newEnd);
            }
        }


        const int targetMaxPos = qMax(0, targetDocLength - 1);
        newStart = qBound(0, newStart, targetMaxPos);
        newEnd = qBound(0, newEnd, targetMaxPos);

        if (newEnd < newStart) {
            qSwap(newStart, newEnd);
        }


        tc.setPosition(newStart);

        tc.setPosition(newEnd, QTextCursor::KeepAnchor);
    } else {
        const int sourcePos = sc.position();
        int newPos = isRightToLeft ? sourcePos * factor : sourcePos / factor;
        newPos = qBound(0, newPos, qMax(0, targetDocLength - 1));


        const bool shouldHighlightHexPair =
            activeMode == ModeHex && !isRightToLeft && target == split->widget(1);
        if (shouldHighlightHexPair && targetDocLength > 0) {
            const int sourceByteIndex = qMax(0, sourcePos - 1);
            int highlightStart = qBound(0, sourceByteIndex * 3, targetDocLength - 1);
            int highlightEnd = qMin(highlightStart + 2, targetDocLength - 1);

            tc.setPosition(highlightStart);
            tc.setPosition(highlightEnd + 1, QTextCursor::KeepAnchor);
        } else {

            tc.setPosition(newPos);
        }
    }

    target->setTextCursor(tc);
}

void Home::onEditorTextChanged() {
    if (isInternalTextSync) {
        return;
    }

    QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
    if (!split) {
        return;
    }

    CodeEditor *leftEd = qobject_cast<CodeEditor*>(split->widget(0));
    CodeEditor *rightEd = qobject_cast<CodeEditor*>(split->widget(1));
    if (!leftEd || !rightEd) {
        return;
    }

    CodeEditor *source = qobject_cast<CodeEditor*>(sender());
    if (!source) {
        return;
    }

    CodeEditor *target = (source == leftEd) ? rightEd : leftEd;
    syncTextEditors(source, target);
    applySearchToCurrentTab();
}

void Home::syncTextEditors(CodeEditor *source, CodeEditor *target) {
    if (!source || !target) {
        return;
    }

    int currentIndex = tabs->currentIndex();
    if (currentIndex < 0) {
        return;
    }

    QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
    if (!split) {
        return;
    }

    CodeEditor *leftEd = qobject_cast<CodeEditor*>(split->widget(0));
    const bool sourceIsLeft = (source == leftEd);

    const EditorMode mode = tabStates[currentIndex].mode;
    if (mode == ModeText && !sourceIsLeft) {
        return;
    }

    const QString sourceText = source->toPlainText();

    QString converted;
    if (sourceIsLeft) {
        switch (mode) {
        case ModeHex:
            converted = TextConverter::toHex(sourceText, 1);
            break;
        case ModeBinary:
            converted = TextConverter::toBinary(sourceText);
            break;
        case ModeUnicode:
            converted = TextConverter::toUnicode(sourceText);
            break;
        case ModeText:
        default:
            converted = sourceText;
            break;
        }
    } else {
        switch (mode) {
        case ModeHex:
            converted = TextConverter::fromHex(sourceText);
            break;
        case ModeBinary:
            converted = TextConverter::fromBinary(sourceText);
            break;
        case ModeUnicode:
            converted = TextConverter::fromUnicode(sourceText);
            break;
        case ModeText: {
            TextType type = TextAnalyzer::detectType(sourceText);
            if (type == TYPE_HEX) {
                converted = TextConverter::fromHex(sourceText);
            } else if (type == TYPE_BINARY) {
                converted = TextConverter::fromBinary(sourceText);
            } else if (type == TYPE_UNICODE) {
                converted = TextConverter::fromUnicode(sourceText);
            } else {
                converted = sourceText;
            }
            break;
        }
        default:
            converted = sourceText;
            break;
        }
    }

    if (target->toPlainText() == converted) {
        return;
    }

    const int sourcePos = source->textCursor().position();
    const QString sourceUtf8 = sourceText.left(sourcePos);
    const int byteOffset = sourceUtf8.toUtf8().size();

    isInternalTextSync = true;
    QSignalBlocker blocker(target);
    target->setPlainText(converted);

    QTextCursor tc = target->textCursor();
    const QString targetText = target->toPlainText();
    int targetPos = 0;
    int acc = 0;
    while (targetPos < targetText.size()) {
        const int chBytes = QString(targetText.at(targetPos)).toUtf8().size();
        if (acc + chBytes > byteOffset) {
            break;
        }
        acc += chBytes;
        ++targetPos;
    }
    tc.setPosition(qBound(0, targetPos, target->document()->characterCount() - 1));
    target->setTextCursor(tc);
    isInternalTextSync = false;
}

void Home::menu(const QString &name){
    if (name == "New") {
        addNewTab();
        return;
    }
    else if (name == "Open File") {
        QString f = QFileDialog::getOpenFileName(this, "Open File");
        if (!f.isEmpty()) openFile(f);
        return;
    }
    else if (name == "Open Folder") {
        QString f = QFileDialog::getExistingDirectory(this, "Select Folder");
        if (!f.isEmpty()) openFolder(f);
        return;
    }
    else if (name == "Exit") {
        this->close();
        return;
    }
    else if (name == "Recent Files") {
        if (recentFiles.isEmpty()) {
            return;
        }

        QMenu historyMenu(this);
        for (const QString &path : qAsConst(recentFiles)) {
            QAction *act = historyMenu.addAction(QFileInfo(path).fileName());
            act->setData(path);
            act->setToolTip(path);
        }

        QAction *selected = historyMenu.exec(QCursor::pos());
        if (selected) {
            QString selectedPath = selected->data().toString();
            if (QFile::exists(selectedPath)) {
                openFile(selectedPath);
            } else {
                recentFiles.removeAll(selectedPath);
                QSettings settings("MyCompany", "MyApplication");
                settings.setValue("history/recentFiles", recentFiles);
            }
        }
        return;
    }

    if (name == "StartFind") {
        showSearchBar();
        return;
    }

    if (name == "Help") {
        QMessageBox::information(
            this,
            "Help",
            "Welcome to Hex Editor!\n\n"
            "- Use File > Open File/Open Folder to load content.\n"
            "- Use Edit and Select to modify your text quickly.\n"
            "- Use Find > StartFind to search in current tab.\n"
            "- Use View to convert text to Hex/Binary/Unicode/Text."
            );
        return;
    }

    QWidget *currentTab = tabs->currentWidget();
    if (!currentTab) return;

    QSplitter *split = qobject_cast<QSplitter*>(currentTab);
    CodeEditor *ed = nullptr;

    if (split) {
        if (split->widget(1)->hasFocus())
            ed = qobject_cast<CodeEditor*>(split->widget(1));
        else
            ed = qobject_cast<CodeEditor*>(split->widget(0));
    }

    if (!ed) return;

    if (name == "Save") {
        if (!currentFile.isEmpty()) {
            QFile file(currentFile);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(ed->toPlainText().toUtf8());
                file.close();
                originalText = ed->toPlainText();
            }
        }
    }


    else if (name == "Save As") {
        QString f = QFileDialog::getSaveFileName(this, "Save As");
        if (f.isEmpty()) return;

        QFile file(f);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(ed->toPlainText().toUtf8());
            file.close();

            currentFile = f;
        }
    }
    else if (name == "Undo") ed->undo();
    else if (name == "Redo") ed->redo();
    else if (name == "Cut") ed->cut();
    else if (name == "Copy") ed->copy();
    else if (name == "Paste") ed->paste();
    else if (name == "SelectAll") {
        ed->setFocus();
        ed->selectAll();
        onCursorChanged();
    }
    else if (name == "SelectLine") {
        ed->setFocus();
        QTextCursor cursor = ed->textCursor();
        cursor.select(QTextCursor::LineUnderCursor);
        ed->setTextCursor(cursor);
        onCursorChanged();
    }
    else if (name == "SelectWord") {
        ed->setFocus();
        QTextCursor cursor = ed->textCursor();
        cursor.select(QTextCursor::WordUnderCursor);
        ed->setTextCursor(cursor);
        onCursorChanged();
    }

    else if (name == "To Hex") {
        QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());

        if (!split) return;
        int index = tabs->currentIndex();
        tabStates[index].mode = ModeHex;
        CodeEditor *textEd = qobject_cast<CodeEditor*>(split->widget(0));
        currentMode = ModeHex;
        saveCurrentTabState();

        CodeEditor *hexEd = qobject_cast<CodeEditor*>(split->widget(1));
        hexEd->setPlainText(TextConverter::toHex(textEd->toPlainText(), 1));
        applyEditorGrouping(hexEd, ModeHex);
        applySearchToCurrentTab();
    }
    else if (name == "To Binary") {

        QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
        if (!split) return;
        int index = tabs->currentIndex();
        tabStates[index].mode = ModeBinary;

        CodeEditor *textEd = qobject_cast<CodeEditor*>(split->widget(0));
        CodeEditor *hexEd = qobject_cast<CodeEditor*>(split->widget(1));

        if (textEd && hexEd) {

            QString binaryText = TextConverter::toBinary(textEd->toPlainText());
            currentMode = ModeBinary;
            saveCurrentTabState();

            hexEd->setPlainText(binaryText);
            applyEditorGrouping(hexEd, ModeBinary);
            applySearchToCurrentTab();
        }
    } else if (name == "To Unicode") {
        QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
        if (!split) return;
        int index = tabs->currentIndex();
        tabStates[index].mode = ModeUnicode;
        CodeEditor *textEd = qobject_cast<CodeEditor*>(split->widget(0));
        CodeEditor *hexEd = qobject_cast<CodeEditor*>(split->widget(1));

        if (textEd && hexEd) {

            QString unicodeText = TextConverter::toUnicode(textEd->toPlainText());
            currentMode = ModeUnicode;
            saveCurrentTabState();

            hexEd->setPlainText(unicodeText);
            applyEditorGrouping(hexEd, ModeUnicode);
            applySearchToCurrentTab();
        }
    }else if (name == "To Text") {
        QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
        if (!split) return;
        int index = tabs->currentIndex();
        tabStates[index].mode = ModeText;

        CodeEditor *textEd = qobject_cast<CodeEditor*>(split->widget(0));
        CodeEditor *hexEd = qobject_cast<CodeEditor*>(split->widget(1));

        if (textEd && hexEd) {
            currentMode = ModeText;
            saveCurrentTabState();

            QString currentContent = textEd->toPlainText();


            TextType type = TextAnalyzer::detectType(currentContent);


            if (type == TYPE_UNICODE) {
                hexEd->setPlainText(TextConverter::fromUnicode(currentContent));

            }
            else if (type == TYPE_HEX) hexEd->setPlainText(TextConverter::fromHex(currentContent));
            else if (type == TYPE_BINARY) hexEd->setPlainText(TextConverter::fromBinary(currentContent));
            else hexEd->setPlainText(currentContent);

            applyEditorGrouping(hexEd, ModeText);

            applySearchToCurrentTab();
        }
    }
}

void Home::openFolder(const QString &path) {
    tree->setRootIndex(model->index(path));
    updateRecentSearchResults();
}

void Home::addNewTab() {

    QSplitter *editorSplit = new QSplitter(Qt::Horizontal);
    CodeEditor *leftEd = new CodeEditor();
    CodeEditor *rightEd = new CodeEditor();

    leftEd->setByteGroupingMode(CodeEditor::GroupingText);
    applyEditorGrouping(rightEd, ModeHex);

    editorSplit->addWidget(leftEd);
    editorSplit->addWidget(rightEd);

    tabs->addTab(editorSplit, "Untitled");
    tabs->setCurrentWidget(editorSplit);


    connect(leftEd, &CodeEditor::cursorPositionChanged, this, &Home::onCursorChanged);
    connect(rightEd, &CodeEditor::cursorPositionChanged, this, &Home::onCursorChanged);
    connect(leftEd, &CodeEditor::textChanged, this, &Home::onEditorTextChanged);
    connect(rightEd, &CodeEditor::textChanged, this, &Home::onEditorTextChanged);

    connect(leftEd, &CodeEditor::selectionChanged, this, &Home::onCursorChanged);
    connect(rightEd, &CodeEditor::selectionChanged, this, &Home::onCursorChanged);

    connect(rightEd, &CodeEditor::cursorPositionChanged, this, &Home::onCursorChanged);



    connect(leftEd->verticalScrollBar(), &QScrollBar::valueChanged,
            rightEd->verticalScrollBar(), &QScrollBar::setValue);
    connect(leftEd->verticalScrollBar(), &QScrollBar::valueChanged,
            rightEd->verticalScrollBar(), &QScrollBar::setValue);

    updateui();
    applySearchToCurrentTab();
}

void Home::addToHistory(const QString &path) {

    if (path.isEmpty()) return;

    recentFiles.removeAll(path);
    recentFiles.prepend(path);

    while (recentFiles.size() > 10) {
        recentFiles.removeLast();
    }


    QSettings settings("MyCompany", "MyApplication");
    settings.setValue("history/recentFiles", recentFiles);
    updateRecentSearchResults();
}

void Home::updateui() {

    bool hasTabs = (tabs->count() > 0);
    menuBarObj->setMenusEnabled(hasTabs);
}

void Home::onTabChanged(int index) {
    saveCurrentTabState();

    if (index == -1) return;



    QSplitter *split = qobject_cast<QSplitter*>(tabs->widget(index));
    if (!split) return;
    CodeEditor *leftEd = qobject_cast<CodeEditor*>(split->widget(0));
    CodeEditor *rightEd = qobject_cast<CodeEditor*>(split->widget(1));

    if (tabStates.contains(index)) {
        TabState state = tabStates[index];

        currentMode = state.mode;
        applyEditorGrouping(rightEd, state.mode);

        QSignalBlocker b1(leftEd);
        QSignalBlocker b2(rightEd);

        leftEd->setPlainText(state.leftText);
        rightEd->setPlainText(state.rightText);

        syncEditors(leftEd, rightEd);

        QTextCursor tcL = leftEd->textCursor();
        tcL.setPosition(state.leftCursorPos);
        leftEd->setTextCursor(tcL);

        QTextCursor tcR = rightEd->textCursor();
        tcR.setPosition(state.rightCursorPos);
        rightEd->setTextCursor(tcR);
    }

    applySearchToCurrentTab();
}

void Home::saveCurrentTabState() {
    int index = tabs->currentIndex();
    if (index == -1) return;

    QSplitter *split = qobject_cast<QSplitter*>(tabs->widget(index));
    if (!split) return;

    CodeEditor *leftEd = qobject_cast<CodeEditor*>(split->widget(0));
    CodeEditor *rightEd = qobject_cast<CodeEditor*>(split->widget(1));

    if (!leftEd || !rightEd) return;


    TabState state;
    state.mode = currentMode;
    state.leftCursorPos = leftEd->textCursor().position();
    state.rightCursorPos = rightEd->textCursor().position();
    state.leftText = leftEd->toPlainText();
    state.rightText = rightEd->toPlainText();

    tabStates[index] = state;
}

void Home::showSearchBar() {
    if (!searchBarWidget || !searchInput) return;
    searchBarWidget->show();
    searchInput->setFocus();
    searchInput->selectAll();
    applySearchToCurrentTab();
    updateRecentSearchResults();
    updateSearchStatus();
}

void Home::navigateSearchMatch(bool forward)
{
    QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
    if (!split) return;

    CodeEditor *leftEd = qobject_cast<CodeEditor*>(split->widget(0));
    CodeEditor *rightEd = qobject_cast<CodeEditor*>(split->widget(1));
    if (!leftEd || !rightEd) return;

    CodeEditor *activeEditor = lastActiveEditor;

    if (!activeEditor)
        activeEditor = leftEd;

    const bool moved = forward ?
                           activeEditor->jumpToNextSearchMatch() :
                           activeEditor->jumpToPreviousSearchMatch();

    if (moved)
        activeEditor->setFocus();

    updateSearchStatus();
}

void Home::updateSearchStatus() {
    if (!searchStatusLabel) return;

    if (!searchBarWidget || !searchBarWidget->isVisible() || !searchInput || searchInput->text().isEmpty()) {
        searchStatusLabel->setText("0/0");
        return;
    }

    QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
    if (!split) {
        searchStatusLabel->setText("0/0");
        return;
    }

    CodeEditor *leftEd = qobject_cast<CodeEditor*>(split->widget(0));
    CodeEditor *rightEd = qobject_cast<CodeEditor*>(split->widget(1));
    if (!leftEd || !rightEd) {
        searchStatusLabel->setText("0/0");
        return;
    }

    CodeEditor *activeEditor = nullptr;

    if (rightEd->hasFocus()) {
        activeEditor = rightEd;
    } else if (leftEd->hasFocus()) {
        activeEditor = leftEd;
    } else {
        activeEditor = leftEd;
    }
    const int total = activeEditor->searchMatchCount();
    const int index = activeEditor->currentSearchMatchIndex();
    if (total == 0) {
        searchStatusLabel->setText("0 / 0");
    } else {
        searchStatusLabel->setText(QString("%1 / %2").arg(index + 1).arg(total));
    }
}

void Home::updateRecentSearchResults() {
    if (!recentSearchResults || !searchInput) {
        return;
    }

    recentSearchResults->clear();

    const QString query = searchInput->text().trimmed();
    if (query.isEmpty()) {
        recentSearchResults->hide();
        return;
    }

    for (const QString &path : qAsConst(recentFiles)) {
        if (!QFileInfo::exists(path)) {
            continue;
        }

        const QPair<int, QString> matchInfo = countOccurrencesInRecentFile(path, query);
        if (matchInfo.first <= 0) {
            continue;
        }

        QListWidgetItem *item = new QListWidgetItem(
            QString("%1 (%2)").arg(matchInfo.second).arg(matchInfo.first),
            recentSearchResults
            );
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }

    recentSearchResults->setVisible(recentSearchResults->count() > 0);
}

void Home::openRecentSearchResult(QListWidgetItem *item) {
    if (!item) {
        return;
    }

    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return;
    }

    openFile(path);
}

void Home::applySearchToCurrentTab() {
    QSplitter *split = qobject_cast<QSplitter*>(tabs->currentWidget());
    if (!split) return;

    CodeEditor *leftEd = qobject_cast<CodeEditor*>(split->widget(0));
    CodeEditor *rightEd = qobject_cast<CodeEditor*>(split->widget(1));
    if (!leftEd || !rightEd) return;

    const QString query = (searchInput && searchBarWidget->isVisible()) ? searchInput->text() : QString();
    if (query.isEmpty()) {
        leftEd->setSearchText(QString());
        rightEd->setSearchText(QString());
        updateSearchStatus();
        return;
    }


    const TextType queryType = detectSearchQueryType(query);
    QString queryAsText = convertQueryToText(query, queryType);
    if (queryAsText.isEmpty()) {
        queryAsText = query;
    }

    const TextType leftType = TextAnalyzer::detectType(leftEd->toPlainText());
    const TextType rightType = TextAnalyzer::detectType(rightEd->toPlainText());

    QString leftQuery = convertTextToTarget(queryAsText, leftType);
    QString rightQuery = convertTextToTarget(queryAsText, rightType);

    if (leftType == TYPE_UNKNOWN) {
        leftQuery = query;
    }
    if (rightType == TYPE_UNKNOWN) {
        rightQuery = query;
    }


    leftEd->setSearchText(leftQuery);
    rightEd->setSearchText(rightQuery);
    updateSearchStatus();
}
