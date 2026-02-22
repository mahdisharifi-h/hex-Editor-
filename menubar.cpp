#include "menubar.h"
#include <QAction>
#include <QMenu>
#include <QKeySequence>

MenuBar::MenuBar(QMainWindow *parent) : QObject(parent)
{
    bar = parent->menuBar();
    bar->setStyleSheet(
        "QMenuBar::item {"
        "  color: #1f1f1f;"
        "}"
        "QMenuBar::item:enabel {"
        "  color: white;"
        "}"
        "QMenuBar::item:disabled {"
        "  color: #8a8a8a;"
        "}"
        );

    QMenu *file = bar->addMenu("File");
    QAction *newAct = file->addAction("New");
    newAct->setShortcut(QKeySequence::New);

    QAction *openAct = file->addAction("Open File");
    openAct->setShortcut(QKeySequence::Open);

    QAction *openFolderAct = file->addAction("Open Folder");
    openFolderAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

    QAction *saveAct = file->addAction("Save");
    saveAct->setShortcut(QKeySequence::Save);

    QAction *saveAsAct = file->addAction("Save As");
    saveAsAct->setShortcut(QKeySequence::SaveAs);

    file->addSeparator();
    QAction *recentAct = file->addAction("Recent Files");
    file->addSeparator();
    recentAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    QAction *exitAct = file->addAction("Exit");
    exitAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q));

    connect(saveAct, &QAction::triggered, this, &MenuBar::onAction);
    connect(saveAsAct, &QAction::triggered, this, &MenuBar::onAction);
    connect(recentAct, &QAction::triggered, this, &MenuBar::onAction);


    connect(newAct, &QAction::triggered, this, &MenuBar::onAction);
    connect(openAct, &QAction::triggered, this, &MenuBar::onAction);
    connect(openFolderAct, &QAction::triggered, this, &MenuBar::onAction);
    connect(exitAct, &QAction::triggered, this, &MenuBar::onAction);

    QMenu *edit = bar->addMenu("Edit");
    QStringList edits = {"Undo","Redo","Cut", "Copy", "Paste"};
    for(const QString &s : edits){
        QAction *a = edit->addAction(s);
        if (s == "Undo") a->setShortcut(QKeySequence::Undo);
        else if (s == "Redo") a->setShortcut(QKeySequence::Redo);
        else if (s == "Cut") a->setShortcut(QKeySequence::Cut);
        else if (s == "Copy") a->setShortcut(QKeySequence::Copy);
        else if (s == "Paste") a->setShortcut(QKeySequence::Paste);
        connect(a,&QAction::triggered,this,&MenuBar::onAction);
    }

    QMenu *select = bar->addMenu("Select");
    QStringList selects = {"SelectAll","SelectLine","SelectWord"};
    for(const QString &s : selects){
        QAction *a = select->addAction(s);
        if (s == "SelectAll") a->setShortcut(QKeySequence::SelectAll);
        else if (s == "SelectLine") a->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
        else if (s == "SelectWord") a->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
        connect(a,&QAction::triggered,this,&MenuBar::onAction);
    }

    QMenu *find = bar->addMenu("Find");
    QAction *startFindAct = find->addAction("StartFind");
    startFindAct->setShortcut(QKeySequence::Find);
    connect(startFindAct, &QAction::triggered, this, &MenuBar::onAction);


    QMenu *view = bar->addMenu("View");
    QStringList conversions = {
        "To Binary",
        "To Hex",
        "To Unicode",
        "To Text"

    };

    for (const QString &s : conversions) {
        QAction *a = view->addAction(s);
        if (s == "To Binary") a->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
        else if (s == "To Hex") a->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));
        else if (s == "To Unicode") a->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_U));
        else if (s == "To Text") a->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
        connect(a, &QAction::triggered, this, &MenuBar::onAction);
    }


    QMenu *help = bar->addMenu("Help");
    QAction *helpAct = help->addAction("Help");
    helpAct->setShortcut(QKeySequence::HelpContents);
    connect(helpAct, &QAction::triggered, this, &MenuBar::onAction);
}

void MenuBar::onAction()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if(act) emit triggered(act->text());
}

void MenuBar::setMenusEnabled(bool enabled) {

    QList<QAction*> menuActions = bar->actions();

    for (QAction *action : menuActions) {
        QString title = action->text();

        if (title == "Edit" || title == "Select" || title == "View" || title == "Find") {
            action->setEnabled(enabled);
        }
    }
}
