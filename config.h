#define SLEEP_NUMBER 1

#define WEATHER_FREQ 60
/*#define OPENWEATHER_URL "curl -s http://openweathermap.org/data/2.1/weather/city/5025219" */
#define OPENWEATHER_URL "curl -s 'http://api.openweathermap.org/data/2.5/weather?id=4487042&appid=f5ed0489a9a80c51c6098755ab21a281'"
/* TODO 
 when using sys basic battery info can be found in /sys/class/power_supply/BAT0/uevent
    
 
 */
char *config[] = {
    "/sys/class/power_supply/BAT0/uevent",
    /* "/proc/acpi/battery/BAT0/state", */
    "/proc/acpi/ibm/volume",
};

char *bat_files[] = {
    "/sys/class/power_supply/BAT%d/uevent",
    "/proc/acpi/battery/BAT%d/state",
};
