#ifndef LOADPROFILEDIALOG_H
#define LOADPROFILEDIALOG_H

#include <QDialog>

namespace Ui {
class LoadProfileDialog;
}

class LoadProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoadProfileDialog(QWidget *parent = nullptr);
    ~LoadProfileDialog();

private:
    Ui::LoadProfileDialog *ui;
};

#endif // LOADPROFILEDIALOG_H
