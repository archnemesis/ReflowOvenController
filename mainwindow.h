#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCharts/QChartGlobal>
#include <stdint.h>

QT_CHARTS_BEGIN_NAMESPACE
class QChart;
class QLineSeries;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class QMessageBox;
class QSerialPort;
class QAction;
class ReflowProfile;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    static const int SET_MODE_MESSAGE_ID = 1;
    static const int SET_PROFILE_MESSAGE_ID = 2;
    static const int MODE_STANDBY = 0;
    static const int MODE_HEATING = 1;
    static const int MODE_HOLDING = 2;
    static const int MODE_COOLING = 3;
    static const int RUN_MODE_OFF = 0;
    static const int RUN_MODE_PROFILE = 1;
    static const int RUN_MODE_HOLD = 2;
    static const int STAGE_STANDBY = 0;
    static const int STAGE_SOAK_RAMP = 1;
    static const int STAGE_SOAK_HOLD = 2;
    static const int STAGE_PEAK_RAMP = 3;
    static const int STAGE_PEAK_HOLD = 4;
    static const int STAGE_COOL = 5;

#pragma pack(push, 1)
    typedef struct status_message {
      uint8_t mode;
      uint8_t run_mode;
      uint8_t stage;
      uint8_t fan_state;
      uint8_t lamp_state;
      uint32_t temp1;
      uint32_t temp2;
    } status_message_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct reflow_profile {
      uint16_t soak_time;
      uint16_t soak_temp;
      uint16_t peak_time;
      uint16_t peak_temp;
    } reflow_profile_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct set_mode_message {
      uint8_t mode;
      uint8_t run_mode;
    } set_mode_message_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct set_profile_message {
      reflow_profile_t profile;
    } set_profile_message_t;
#pragma pack(pop)

    Ui::MainWindow *ui;
    QChart *m_chart = nullptr;
    QLineSeries *m_seriesTemp = nullptr;
    QSerialPort *m_serialPort = nullptr;
    QMessageBox *m_msgBoxWaitingForOven = nullptr;
    int m_chartRange = 3000;
    int m_gotMarker1 = 0;
    int m_gotMarker2 = 0;
    int m_statusBufferIndex = 0;
    char m_statusBuffer[sizeof(status_message_t)];

    int m_currentStage = STAGE_STANDBY;
    int m_currentMode = MODE_STANDBY;
    int m_currentRunMode = RUN_MODE_OFF;

    QList<ReflowProfile> m_profileList;

    void updatePlot(QLineSeries *series, double value);
    void saveProfiles();

private slots:
    void on_btnConnect_clicked();
    void on_btnDisconnect_clicked();
    void on_serialPort_readyRead();
    void on_btnUploadProfile_clicked();
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void on_savedAction_triggered();
    void on_action_Quit_triggered();
    void on_actionExport_Data_triggered();
    void on_actionAbout_triggered();
    void on_btnNewProfile_clicked();
    void on_btnSaveProfile_clicked();
    void on_cboProfileSelect_currentIndexChanged(int index);
    void on_spnSoakTemp_valueChanged(int arg1);
    void on_spnSoakTime_valueChanged(int arg1);
    void on_spnPeakTemp_valueChanged(int arg1);
    void on_spnPeakTime_valueChanged(int arg1);
    void on_actionNew_triggered();
    void on_actionSave_triggered();
    void on_actionDelete_triggered();
};

#endif // MAINWINDOW_H
