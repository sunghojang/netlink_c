#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
struct trigger_results {
    int done;
    int aborted;
};


struct handler_args {  // For family_handler() and nl_get_multicast_id().
    const char *group;
    int id;
};
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
public:





};

#endif // MAINWINDOW_H
