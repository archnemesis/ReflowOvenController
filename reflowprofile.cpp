#include "reflowprofile.h"

ReflowProfile::ReflowProfile()
{

}

ReflowProfile::ReflowProfile(QString name, int soak_temp, int soak_time, int peak_temp, int peak_time)
{
    this->name = name;
    soakTemperature = soak_temp;
    soakTime = soak_time;
    peakTemperature = peak_temp;
    peakTime = peak_time;
}
