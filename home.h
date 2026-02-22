#ifndef HOME_H
#define HOME_H

#include <QMainWindow>
#include <QTabWidget>
#include <QFileSystemModel>
#include <QTreeView>
#include <QLineEdit>
#include <QWidget>
#include <QListWidget>
#include "codeeditor.h"
#include "menubar.h"
#include "textanalyzer.h"
#include "QLabel"


class Home : public QMainWindow {
    Q_OBJECT
    enum EditorMode { ModeHex, ModeBinary, ModeUnicode,ModeText };
    EditorMode currentMode = ModeHex;
    EditorMode lastMode;


    struct TabState {
        EditorMode mode = ModeHex;
        int  txtClik = 1;
        int leftCursorPos = 0;
        int rightCursorPos = 0;
        QString leftText;
        QString rightText;
        QString filePath;
        bool lastSearchFromRight = false;
    };
public:
    Home(QWidget *parent = nullptr);

private slots:
    void menu(const QString &name);
    void onCursorChanged();
    void onEditorTextChanged();

private:
    TextAnalyzer *detectType(const QString &text);
    QMap<int, TabState> tabStates;
    void onTabChanged(int index);
    void saveCurrentTabState();
    CodeEditor *leftEditor;
    CodeEditor *rightEditor;
    void updateui();
    void addNewTab();
    void openFile(const QString &path);
    void openFolder(const QString &path);
    int calculateDisplayPosition(const QString &text, int bytePos);
    int calculateByteOffset(const QString &text, int cursorPos);

    void addToHistory(const QString &path);
    void syncEditors(CodeEditor *source, CodeEditor *target);
    void syncTextEditors(CodeEditor *source, CodeEditor *target);
    QString detectFormat(const QString &text);
    void applyEditorGrouping(CodeEditor *editor, EditorMode mode);

    void applySearchToCurrentTab();
    void updateRecentSearchResults();
    void openRecentSearchResult(QListWidgetItem *item);
    void showSearchBar();
    void updateSearchStatus();
    void navigateSearchMatch(bool forward);
    QLineEdit *searchInput = nullptr;
    QWidget *searchBarWidget = nullptr;
    QLabel *searchStatusLabel = nullptr;
    QListWidget *recentSearchResults = nullptr;


    QTabWidget *tabs;
    QTreeView *tree;
    QFileSystemModel *model;
    QString currentFile;
    QString originalText;
    QStringList recentFiles;
    MenuBar *menuBarObj;
    bool isInternalTextSync = false;

};
#endif
