#include "loadprofiledialog.h"
#include "ui_loadprofiledialog.h"

LoadProfileDialog::LoadProfileDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoadProfileDialog)
{
    ui->setupUi(this);
}

LoadProfileDialog::~LoadProfileDialog()
{
    delete ui;
}
