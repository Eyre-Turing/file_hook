#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileInfo>
#include <QDir>
#include <QMouseEvent>
#include <QInputDialog>
#include <QDebug>

MainWindow *hookWindow = NULL;
Win32FSHook win32FSHook;

char *wchar2char(const wchar_t *wchar, char *m_char);
wchar_t *char2wchar(const char *cchar, wchar_t *m_wchar);

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 4);

    ui->listWidget->installEventFilter(this);
    contextMenu = new QMenu(this);

    contextMenu->addAction(ui->actionAdd);
    contextMenu->addAction(ui->actionRemove);

    hookWindow = this;
    win32FSHook.init(win32FSCallback);
    DWORD err;
    wchar_t path[256];
    foreach(QFileInfo i, QDir::drives())
    {
        QString pn = i.absoluteFilePath();
        qDebug()<<pn;
        QListWidgetItem *item = new QListWidgetItem;
        item->setText(pn);
        char2wchar(pn.toLocal8Bit().data(), path);
        item->setWhatsThis(QString::number(win32FSHook.add_watch(path, 1|2|4|8, true, err)));
        ui->listWidget->addItem(item);
    }

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    timer->start(10);
}

void MainWindow::onTimeout()
{
    if(!infoBuffer.isEmpty())
    {
        QString info = infoBuffer.takeFirst();
        ui->textEdit->append(info);
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == ui->listWidget)
    {
        if(event->type() == QEvent::ContextMenu)
        {
            qDebug()<<"弹右键菜单";
            QListWidgetItem *item = ui->listWidget->currentItem();
            if(item)
            {
                QRect rect = ui->listWidget->visualItemRect(item);
                QPoint mousePos = ui->listWidget->mapFromGlobal(QCursor::pos());
                if(rect.contains(mousePos)) //选中了一个项目
                {
                    ui->actionRemove->setEnabled(true);
                }
                else    //未选中任何项目
                {
                    ui->actionRemove->setEnabled(false);
                }
            }
            else    //未选中任何项目
            {
                ui->actionRemove->setEnabled(false);
            }
            contextMenu->exec(QCursor::pos());
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

MainWindow::~MainWindow()
{
    delete ui;
}

char *wchar2char(const wchar_t *wchar, char *m_char)
{
    int len = WideCharToMultiByte(CP_ACP, 0, wchar, wcslen(wchar), NULL, 0, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, wchar, wcslen(wchar), m_char, len, NULL, NULL);
    m_char[len] = '\0';
    return m_char;
}

wchar_t *char2wchar(const char *cchar, wchar_t *m_wchar)
{
    int len = MultiByteToWideChar(CP_ACP, 0, cchar, strlen(cchar), NULL, 0);
    MultiByteToWideChar(CP_ACP, 0, cchar, strlen(cchar), m_wchar, len);
    m_wchar[len] = '\0';
    return m_wchar;
}

QString operBuffer;

void MainWindow::win32FSCallback(int watchID, int action, const WCHAR* rootPath, const WCHAR* filePath)
{
    char tmp[256];
    QString filename;
    filename = QString::fromLocal8Bit(wchar2char(rootPath, tmp));
    filename.append(QString::fromLocal8Bit(wchar2char(filePath, tmp)));
    filename.replace("\\", "/");
    qDebug()<<"watchID:"<<watchID;
    switch(action)
    {
    case 1:
    {
        qDebug()<<"创建："<<filename;
        hookWindow->infoBuffer.append(QString("<font color=\"#00FF00\">创建文件：<br>&nbsp;%1</font>").arg(filename));
        break;
    }
    case 2:
    {
        qDebug()<<"删除："<<filename;
        hookWindow->infoBuffer.append(QString("<font color=\"#FF0000\">删除文件：<br>&nbsp;%1</font>").arg(filename));
        break;
    }
    case 3:
    {
        qDebug()<<"复制："<<filename;
        hookWindow->infoBuffer.append(QString("<font color=\"#000000\">复制文件：<br>&nbsp;%1</font>").arg(filename));
        break;
    }
    case 4:
    {
        operBuffer = QString("<font color=\"#008080\">重命名：<br>&nbsp;原文件名：")+filename;
        break;
    }
    case 5:
    {
        operBuffer += QString("<br>&nbsp;新文件名：")+filename;
        hookWindow->infoBuffer.append(operBuffer);
        break;
    }
    default:
    {
        qDebug()<<"不明操作，操作代码"<<action<<"："<<filename;
        hookWindow->infoBuffer.append(QString("<font color=\"#0000FF\">不明操作，代码%1：<br>&nbsp;%2</font>").arg(action).arg(filename));
    }
    }
}

void MainWindow::on_actionAdd_triggered()
{
    qDebug()<<"添加";
    QString pn = QInputDialog::getText(this, "添加监听路径", "路径：");
    if(pn != "")
    {
        DWORD err;
        wchar_t path[256];
        qDebug()<<pn;
        QListWidgetItem *item = new QListWidgetItem;
        item->setText(pn);
        char2wchar(pn.toLocal8Bit().data(), path);
        item->setWhatsThis(QString::number(win32FSHook.add_watch(path, 1|2|4|8, true, err)));
        ui->listWidget->addItem(item);
    }
}

void MainWindow::on_actionRemove_triggered()
{
    QListWidgetItem *item = ui->listWidget->takeItem(ui->listWidget->currentRow());
    qDebug()<<"移除"<<item->text();
    win32FSHook.remove_watch(item->whatsThis().toInt());
    delete item;
}
