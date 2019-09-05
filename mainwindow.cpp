#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "reflowprofile.h"

#include <QAction>
#include <QChart>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineSeries>
#include <QValueAxis>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QMessageBox>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
    for (QSerialPortInfo port : portList) {
        ui->cboPort->addItem(port.portName());
    }

    m_msgBoxWaitingForOven = new QMessageBox(this);
    m_msgBoxWaitingForOven->setText(tr("Waiting for oven..."));
    m_msgBoxWaitingForOven->setModal(Qt::ApplicationModal);
    m_msgBoxWaitingForOven->setStandardButtons(QMessageBox::Cancel);
    connect(m_msgBoxWaitingForOven, &QMessageBox::buttonClicked, this, &MainWindow::on_btnDisconnect_clicked);

    m_chart = new QtCharts::QChart();
    ui->chartView->setChart(m_chart);

    m_seriesTemp = new QLineSeries();

    m_chart->addSeries(m_seriesTemp);
    m_chart->legend()->setVisible(false);

    QValueAxis *axisX = new QValueAxis;
    axisX->setRange(0, m_chartRange);
    axisX->setLabelFormat("%g");
    axisX->setTickInterval(600);
    axisX->setTickCount((m_chartRange / 600) + 1);
    axisX->setTitleText("Time (1/10 sec)");
    m_chart->setAxisX(axisX, m_seriesTemp);

    QValueAxis *axisYTemperature = new QValueAxis;
    axisYTemperature->setRange(10, 400);
    axisYTemperature->setTitleText("Temperature (C)");
    m_chart->setAxisY(axisYTemperature, m_seriesTemp);

    ui->grpControls->setEnabled(false);
    ui->grpProfile->setEnabled(false);
    ui->grpStatus->setEnabled(false);
    ui->btnDisconnect->setEnabled(false);

    QSettings settings;
    int size = settings.beginReadArray("savedProfiles");
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);

        QAction *action = new QAction(this);
        action->setText(settings.value("name").toString());
        connect(action, &QAction::triggered, this, &MainWindow::on_savedAction_triggered);

        ReflowProfile profile(
                    settings.value("name").toString(),
                    settings.value("soakTemperature").toInt(),
                    settings.value("soakTime").toInt(),
                    settings.value("peakTemperature").toInt(),
                    settings.value("peakTime").toInt());

        m_profileList.append(profile);
        ui->cboProfileSelect->addItem(settings.value("name").toString());
    }

    settings.endArray();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_currentMode != MODE_STANDBY) {
        int result = QMessageBox::question(this,
                              tr("Operation In Progress"),
                              tr("The oven is currently operating. Do you really want to quit?"));
        if (result == QMessageBox::Yes) {
            if (m_serialPort && m_serialPort->isOpen()) {
                m_serialPort->close();
                event->accept();
            }
        }
        else {
            event->ignore();
        }
    }
}

void MainWindow::on_btnConnect_clicked()
{
    QString port = ui->cboPort->currentText();
    int speed = ui->cboBaudRate->currentText().toInt();

    m_serialPort = new QSerialPort(this);
    m_serialPort->setPortName(port);
    m_serialPort->setBaudRate(speed);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    m_serialPort->setDataBits(QSerialPort::Data8);

    connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::on_serialPort_readyRead);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, "Port Error", "Unable to open serial port!");
        m_serialPort->close();
        m_serialPort->deleteLater();
        m_serialPort = nullptr;
    }
    else {
        ui->btnDisconnect->setEnabled(true);
        ui->btnConnect->setEnabled(false);

        ui->grpControls->setEnabled(true);
        ui->grpProfile->setEnabled(true);
        ui->grpStatus->setEnabled(true);

        m_msgBoxWaitingForOven->show();
    }
}

void MainWindow::on_btnDisconnect_clicked()
{
    if (m_serialPort != nullptr) {
        m_serialPort->close();
        m_serialPort->deleteLater();
        m_serialPort = nullptr;
    }

    m_msgBoxWaitingForOven->hide();
    ui->btnDisconnect->setEnabled(false);
    ui->btnConnect->setEnabled(true);
}

void MainWindow::on_serialPort_readyRead()
{
    QByteArray data = m_serialPort->readAll();

    for (int i = 0; i < data.length(); i++) {
        if (m_gotMarker1 == 1 && m_gotMarker2 == 1) {
            m_statusBuffer[m_statusBufferIndex++] = data.at(i);
            if (m_statusBufferIndex == sizeof(status_message_t)) {
                status_message_t *msg = new status_message_t;
                memcpy((void *)msg, (void *)m_statusBuffer, sizeof(status_message_t));

                // message received, close waiting dialog
                if (m_msgBoxWaitingForOven->isVisible()) {
                    m_msgBoxWaitingForOven->hide();
                }

                double temp1 = (double)msg->temp1 / 100.0;
                double temp2 = (double)msg->temp2 / 100.0;
                double tempA = (temp1 + temp2) / 2.0;

                if (m_currentMode != MODE_STANDBY) {
                    updatePlot(m_seriesTemp, tempA);
                }

                m_statusBufferIndex = 0;
                m_gotMarker1 = 0;
                m_gotMarker2 = 0;

                ui->lblTemperature->setText(QString("%1C").arg(tempA, 4, 'f', 2, '0'));

                m_currentMode = msg->mode;

                switch (msg->mode) {
                case MODE_STANDBY:
                    ui->lblMode->setText("Standby");
                    ui->btnStart->setEnabled(true);
                    ui->btnStop->setEnabled(false);
                    break;
                case MODE_HEATING:
                    ui->lblMode->setText("Heating");
                    ui->btnStart->setEnabled(false);
                    ui->btnStop->setEnabled(true);
                    break;
                case MODE_HOLDING:
                    ui->lblMode->setText("Holding");
                    ui->btnStart->setEnabled(false);
                    ui->btnStop->setEnabled(true);
                    break;
                case MODE_COOLING:
                    ui->lblMode->setText("Cooling");
                    ui->btnStart->setEnabled(false);
                    ui->btnStop->setEnabled(true);
                    break;
                }

                m_currentRunMode = msg->run_mode;

                switch (msg->run_mode) {
                case RUN_MODE_OFF:
                    ui->lblRunMode->setText("Off/Standby");
                    break;
                case RUN_MODE_PROFILE:
                    ui->lblRunMode->setText("Reflow Profile");
                    break;
                case RUN_MODE_HOLD:
                    ui->lblRunMode->setText("Preheat/Hold");
                    break;
                }

                m_currentStage = msg->stage;

                switch (msg->stage) {
                case STAGE_STANDBY:
                    ui->lblStage->setText("Standby");
                    break;
                case STAGE_SOAK_RAMP:
                    ui->lblStage->setText("Ramp to Soak");
                    break;
                case STAGE_SOAK_HOLD:
                    ui->lblStage->setText("Soak");
                    break;
                case STAGE_PEAK_RAMP:
                    ui->lblStage->setText("Ramp to Peak");
                    break;
                case STAGE_PEAK_HOLD:
                    ui->lblStage->setText("Peak Hold");
                    break;
                case STAGE_COOL:
                    ui->lblStage->setText("Cooling");
                    break;
                }

                if (msg->fan_state == 1) {
                    ui->lblFanState->setText("ON");
                }
                else {
                    ui->lblFanState->setText("OFF");
                }

                if (msg->lamp_state == 1) {
                    ui->lblLampState->setText("ON");
                }
                else {
                    ui->lblLampState->setText("OFF");
                }
            }
        }
        else {
            if (m_gotMarker1 == 0) {
                if ((uint8_t)data.at(i) == 0xFA) {
                    m_gotMarker1 = 1;
                }
            }
            else {
                if ((uint8_t)data.at(i) == 0xAF) {
                    m_gotMarker2 = 1;
                }
                else {
                    m_gotMarker1 = 0;
                }
            }
        }
    }
}

void MainWindow::updatePlot(QLineSeries *series, double value)
{
    QVector<QPointF> oldPoints = series->pointsVector();
    QVector<QPointF> points;

    if (oldPoints.count() < m_chartRange) {
        points = series->pointsVector();
    }
    else {
        for (int i = 1; i < oldPoints.count(); i++) {
            points.append(QPointF(i - 1, oldPoints.at(i).y()));
        }
    }

    points.append(QPointF(points.count(), value));
    series->replace(points);
}

void MainWindow::saveProfiles()
{
    QSettings settings;
    settings.remove("savedProfiles");

    settings.beginWriteArray("savedProfiles");
    for (int i = 0; i < m_profileList.length(); i++) {
        settings.setArrayIndex(i);
        settings.setValue("name", m_profileList.at(i).name);
        settings.setValue("soakTemperature", m_profileList.at(i).soakTemperature);
        settings.setValue("soakTime", m_profileList.at(i).soakTime);
        settings.setValue("peakTemperature", m_profileList.at(i).peakTemperature);
        settings.setValue("peakTime", m_profileList.at(i).peakTime);
    }
    settings.endArray();
}

void MainWindow::on_btnUploadProfile_clicked()
{
    set_profile_message_t msg;
    msg.profile.peak_time = ui->spnPeakTime->value();
    msg.profile.peak_temp = ui->spnPeakTemp->value();
    msg.profile.soak_time = ui->spnSoakTime->value();
    msg.profile.soak_temp = ui->spnSoakTemp->value();

    struct {
        uint8_t a;
        uint8_t b;
        uint8_t id;
    } pkt_header = { 0xFA, 0xAF, SET_PROFILE_MESSAGE_ID };

    m_serialPort->write((const char *)&pkt_header, sizeof(pkt_header));
    m_serialPort->write((const char *)&msg, sizeof(set_profile_message_t));
}

void MainWindow::on_btnStart_clicked()
{
    on_btnUploadProfile_clicked();

    m_seriesTemp->clear();

    set_mode_message_t msg;
    msg.mode = MODE_HEATING;
    msg.run_mode = RUN_MODE_PROFILE;

    struct {
        uint8_t a;
        uint8_t b;
        uint8_t id;
    } pkt_header = { 0xFA, 0xAF, SET_MODE_MESSAGE_ID };

    m_serialPort->write((const char *)&pkt_header, sizeof(pkt_header));
    m_serialPort->write((const char *)&msg, sizeof(set_mode_message_t));
}

void MainWindow::on_btnStop_clicked()
{
    set_mode_message_t msg;
    msg.mode = MODE_STANDBY;
    msg.run_mode = RUN_MODE_PROFILE;

    struct {
        uint8_t a;
        uint8_t b;
        uint8_t id;
    } pkt_header = { 0xFA, 0xAF, SET_MODE_MESSAGE_ID };

    m_serialPort->write((const char *)&pkt_header, sizeof(pkt_header));
    m_serialPort->write((const char *)&msg, sizeof(set_mode_message_t));
}

void MainWindow::on_savedAction_triggered()
{

}

void MainWindow::on_action_Quit_triggered()
{
    close();
}

void MainWindow::on_actionExport_Data_triggered()
{
    QString fileName = QFileDialog::getSaveFileName(
                this,
                tr("Export Data"),
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                tr("CSV (*.csv)"));

    if (!fileName.isEmpty()) {
        QFile fp(fileName);

        if (!fp.open(QIODevice::WriteOnly)) {
            int result = QMessageBox::critical(this,
                                  tr("File Error"),
                                  tr("Unable to open output file."),
                                  QMessageBox::Retry | QMessageBox::Cancel);

            if (result == QMessageBox::Retry) {
                on_actionExport_Data_triggered();
            }
        }

        QTextStream stream(&fp);
        stream << "Time,Temperature\n";

        QVector<QPointF> points = m_seriesTemp->pointsVector();

        for (int i = 0; i < points.length(); i++) {
            stream << points.at(i).x() << "," << points.at(i).y() << "\n";
        }

        fp.close();
    }
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::on_btnNewProfile_clicked()
{
    QString profileName = QInputDialog::getText(
                this,
                tr("New Profile"),
                tr("Enter a name to give to the new profile."));

    if (!profileName.isEmpty()) {
        ReflowProfile profile(
                    profileName,
                    ui->spnSoakTemp->value(),
                    ui->spnSoakTime->value(),
                    ui->spnPeakTemp->value(),
                    ui->spnPeakTime->value());
        m_profileList.append(profile);
        ui->cboProfileSelect->addItem(profileName);
        saveProfiles();
    }
}

void MainWindow::on_btnSaveProfile_clicked()
{
    int currentIndex = ui->cboProfileSelect->currentIndex();
    ReflowProfile profile = m_profileList.at(currentIndex);
    profile.soakTemperature = ui->spnSoakTemp->value();
    profile.soakTime = ui->spnSoakTime->value();
    profile.peakTemperature = ui->spnPeakTemp->value();
    profile.peakTime = ui->spnPeakTime->value();
    m_profileList.replace(currentIndex, profile);
    saveProfiles();
}

void MainWindow::on_cboProfileSelect_currentIndexChanged(int index)
{
    ReflowProfile profile = m_profileList.at(index);
    ui->spnSoakTemp->setValue(profile.soakTemperature);
    ui->spnSoakTime->setValue(profile.soakTime);
    ui->spnPeakTemp->setValue(profile.peakTemperature);
    ui->spnPeakTime->setValue(profile.peakTime);
    ui->btnSaveProfile->setEnabled(false);
}

void MainWindow::on_spnSoakTemp_valueChanged(int arg1)
{
    ui->btnSaveProfile->setEnabled(true);
}

void MainWindow::on_spnSoakTime_valueChanged(int arg1)
{
    ui->btnSaveProfile->setEnabled(true);
}

void MainWindow::on_spnPeakTemp_valueChanged(int arg1)
{
    ui->btnSaveProfile->setEnabled(true);
}

void MainWindow::on_spnPeakTime_valueChanged(int arg1)
{
    ui->btnSaveProfile->setEnabled(true);
}

void MainWindow::on_actionNew_triggered()
{
    on_btnNewProfile_clicked();
}

void MainWindow::on_actionSave_triggered()
{
    on_btnSaveProfile_clicked();
}

void MainWindow::on_actionDelete_triggered()
{
    int result = QMessageBox::question(
                this,
                tr("Delete Profile?"),
                tr("Are you sure you want to delete the profile \"%1\"?").arg(ui->cboProfileSelect->currentText()));

    if (result == QMessageBox::Yes) {

    }
}
