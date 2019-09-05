#ifndef REFLOWPROFILE_H
#define REFLOWPROFILE_H

#include <QString>

class ReflowProfile
{
public:
    ReflowProfile();
    ReflowProfile(QString name, int soak_temp, int soak_time, int peak_temp, int peak_time);

    QString name;
    int soakTemperature;
    int soakTime;
    int peakTemperature;
    int peakTime;
};

#endif // REFLOWPROFILE_H
