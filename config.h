#define SLEEP_NUMBER 1

/* TODO 
 when using sys basic battery info can be found in /sys/class/power_supply/BAT0/uevent
    
 
 */
char *config[] = { 
    "/proc/acpi/battery/BAT0/state",
    "/proc/acpi/ibm/volume",
};

char *bat_files[] = {
    "/proc/acpi/battery/BAT%d/state",
    "/sys/class/power_supply/BAT%d/uevent",
};
