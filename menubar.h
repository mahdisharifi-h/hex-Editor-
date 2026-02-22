#ifndef MENUBAR_H
#define MENUBAR_H

#include <QObject>
#include <QMenuBar>
#include <QMainWindow>

class MenuBar : public QObject
{
    Q_OBJECT
public:
    explicit MenuBar(QMainWindow *parent);
    void setMenusEnabled(bool enabled);

signals:
    void triggered(const QString &name);

private slots:
    void onAction();

private:
    QMenuBar *bar;
};

#endif
