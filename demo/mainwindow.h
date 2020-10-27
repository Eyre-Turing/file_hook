#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Win32FSHook.h"
#include <QTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    static void win32FSCallback(int watchID, int action, const WCHAR* rootPath, const WCHAR* filePath);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    Ui::MainWindow *ui;
    QList<QString> infoBuffer;
    QTimer *timer;
    QMenu *contextMenu;

private slots:
    void onTimeout();

    void on_actionAdd_triggered();
    void on_actionRemove_triggered();
};

#endif // MAINWINDOW_H
