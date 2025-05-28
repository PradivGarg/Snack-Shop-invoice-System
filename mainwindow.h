#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_addItemButton_clicked();
    void on_removeItemButton_clicked();
    void on_SaveInoviceButton_clicked();
    void on_loadLastInvoice_clicked();

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    bool initDatabase();

};
#endif // MAINWINDOW_H
