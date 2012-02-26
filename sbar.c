/* sbar is simple status bar for dwm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <sys/sysinfo.h>
#include <sensors/sensors.h>

char *config[] = { 
    "/proc/acpi/battery/BAT0/state",
    "/proc/acpi/ibm/volume",
};

double get_chip_temp(const sensors_chip_name *name);
static const unsigned int sleep_number = 1;

static char card[64] = "default";
static int smixer_level = 0;
static struct snd_mixer_selem_regopt smixer_options;

static int sensors_loaded = 0;
const sensors_chip_name *temp_chip;


struct bat_info {
    char *conf;
    int interval;
} bat_info;

struct bat_info *bat_info_init(char *batconf) {
    struct bat_info *bat = malloc(sizeof(struct bat_info));
    assert( bat != NULL);
    bat->conf = strdup(batconf);
    bat->interval = 30;
    return bat;
}

typedef struct vol_uinfo {
    int mute; /* 0|1 */
    int percent; /* current volume percent 0..100 */

} vol_info;

typedef struct bat_ustate {
    int present; /* 1|0 */
    char charge_state[10];
    int prate;
    int remaining;
    int voltage;
} bat_state;


void cleanup(struct bat_info *bat)  {
    free(bat->conf);
    free(bat);
}


static int alsa_parse_simple_id(const char *str, snd_mixer_selem_id_t *sid)
{
	int c, size;
	char buf[128];
	char *ptr = buf;


	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	size = 1;	/* for '\0' */
	if (*str != '"' && *str != '\'') {
		while (*str && *str != ',') {
			if (size < (int)sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
	} else {
		c = *str++;
		while (*str && *str != c) {
			if (size < (int)sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
		if (*str == c)
			str++;
	}
	if (*str == '\0') {
		snd_mixer_selem_id_set_index(sid, 0);
		*ptr = 0;
		goto _set;
	}
	if (*str != ',')
		return -EINVAL;
	*ptr = 0;	/* terminate the string */
	str++;
	if (!isdigit(*str))
		return -EINVAL;
	snd_mixer_selem_id_set_index(sid, atoi(str));
       _set:
	snd_mixer_selem_id_set_name(sid, buf);
	return 0;
}


static void alsa_error(const char *fmt,...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "amixer: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}

static int alsa_convert_prange(int val, int min, int max)
{
	int range = max - min;
	int tmp;

	if (range == 0)
		return 0;
	val -= min;
	tmp = rint((double)val/(double)range * 100);
	return tmp;
}

int get_localtime(char *sbi) {
    time_t t;
    struct tm *tmp;

    char outstr[100];
    char result[100];
    int ret;
    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL ) {
        perror("localtime");
    }
    if ( strftime(outstr,sizeof(outstr),"%c",tmp) == 0 ) {
        printf("ERROR\n");
    }
    strcpy(sbi,outstr);
    return 0;
}


int read_battery_state() {
    bat_state bs;
    char result[100];
    char b1[100];
    char b2[30];
    char b3[30]; 
    unsigned int  ibuf = 0;
    FILE *fdbat;
    fdbat = fopen(config[0],"r");
    while (!feof(fdbat) ) {
        fgets(b1,sizeof(b1),fdbat);
        if ( sscanf(b1,"%s %s %u",b2,b3,&ibuf) == 2 ) {
            sscanf(b1,"%s&%s %s",b2,b3);
        } else {
            if (strcmp("capacity:",b3) == 0 ) {
                bs.remaining = ibuf;
                /* printf("> %s\n",b2); */
            }
            if (strcmp("rate:",b3) == 0 ) {
                /* printf("> %s\n",b2); */
                bs.prate = ibuf;
            }
        }
    }
    fclose(fdbat);
    if (bs.prate == 0 ) {
        /* printf("currently charging\n"); */
        return 0;
    } else {
        /* printf("Calculate %f * 60  %f \n",bs.remaining,bs.prate); */
        return (bs.remaining*60)/bs.prate;
    }
}

int get_bat_left(struct bat_info *b) {
    int bat = 0;
    bat = read_battery_state();
    return bat;
}


static int alsa_get_vol() {
    int err = 0;
    static snd_mixer_t *handle = NULL;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    char *simpled = "Master";

    if (alsa_parse_simple_id(simpled,sid) ) {
        fprintf(stderr,"Wrong scontrol %s\n",simpled);
    } else {
        if (handle == NULL ) {
            if((err = snd_mixer_open(&handle,0)) < 0 ) {
    		    error("Control device %s open error: %s", card, snd_strerror(err));
        		return err;
            }
    		if (smixer_level == 0 && (err = snd_mixer_attach(handle, card)) < 0) {
    			error("Mixer attach %s error: %s", card, snd_strerror(err));
    			snd_mixer_close(handle);
    			handle = NULL;
    			return err;
    		}
    		if ((err = snd_mixer_selem_register(handle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
    			error("Mixer register error: %s", snd_strerror(err));
    			snd_mixer_close(handle);
    			handle = NULL;
    			return err;
	    	}
    		err = snd_mixer_load(handle);
    		if (err < 0) {
    			error("Mixer %s load error: %s", card, snd_strerror(err));
    			snd_mixer_close(handle);
    			handle = NULL;
    			return err;
    		}
        }
		/* printf("Simple mixer control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid)); */
    }
	elem = snd_mixer_find_selem(handle, sid);
	if (!elem) {
		error("Mixer %s simple element not found", card);
		return -ENOENT;
	}
    long pvol,pmin,pmax;
	snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
    snd_mixer_close(handle);
	handle = NULL;
    return alsa_convert_prange(pvol,pmin,pmax);
}

int get_vol(char *volstr, vol_info *v, int has_mute) {
    v->mute = 0;
    v->percent  = alsa_get_vol();
    char buf_1[100];
    char vol[10]= " ";

    if (has_mute) {

        FILE *fd;
        fd = fopen(config[1],"r");
        while (fscanf(fd,"%s",buf_1) != EOF ) {
            if (strcmp(buf_1,"mute:") == 0 ) {
                char buf_2[10];
                fscanf(fd,"%s",buf_2);
                if (strcmp(buf_2,"off")  == 1 ) {
                    v->mute = 1;
                }
                break;
            }
        }
        fclose(fd);
    }
    if (v->mute == 1 ) { 
        sprintf(vol,"%d%% muted",v->percent);
    } else {
        sprintf(vol,"%d%%",v->percent);
    }

    strcpy(volstr,vol);
    return 0; 
}

int x_root_title ( char *new_title) {
   Display *d;
   Window w;
   XTextProperty t;

   int s;
   int n;
   d = XOpenDisplay(NULL);
   if (d == NULL) {
      fprintf(stderr, "Cannot open display\n");
      exit(1);
   }
 
   s = DefaultScreen(d);
   w = XDefaultRootWindow(d);
   
   char *text = (char *)new_title;

    XStringListToTextProperty(&text,1,&t);

    XSetWMName(d,w,&t); 
    XFree(t.value);
    XCloseDisplay(d);
    return 0;
}

int sbar_sensors_init() {
    char input[] ="/etc/sensors3.conf";
    FILE *config;
    int err;

    config = fopen(input,"r");

    if (!config) { 
        fprintf(stderr,"Could not open config file\n");
        return 1;
    }

    err = sensors_init(config);

    if ( err > 0 ) {
        printf("Fail to init senosrs %d",err);
        return 1;
    } else {
        int chip_nr;
        double ret =0.0;

        chip_nr = 0;
        while ( ( temp_chip = sensors_get_detected_chips(NULL,&chip_nr)  )) {
            ret = get_chip_temp(temp_chip);
            if(ret) {
                sensors_loaded = 1;
                return 0;
            }
        }
    }
    sensors_cleanup();
}

double get_chip_temp(const sensors_chip_name *name ) {

    const sensors_feature *feature;
    
    int i = 0;
    char *label;
    char str_t[6] = "temp1";
    const sensors_subfeature *sf;
    double val = 0.0;

    while ( ( feature = sensors_get_features(name,&i) )) {
        if (( label = sensors_get_label(name,feature)) ) {
            if ( strcmp(label,str_t) == 0 && feature->type == SENSORS_FEATURE_TEMP ) {
                int err;
                sf = sensors_get_subfeature(name,feature,SENSORS_SUBFEATURE_TEMP_INPUT);
                err = sensors_get_value(name,sf->number,&val);
            }
            free(label);
            if (val > 0 )
                return val ;
         }
    }
}

int file_exists ( char * file_name ) { 
    struct stat buf;
    int i = stat(file_name ,&buf);

    return (i ? 0 : 1);

}

int main( int argc, char **argv) {
    int str_out = 0;
    int bat_exists  = file_exists(config[0]); 
    int mute_exists = file_exists(config[1]);

    if (argc > 1 && strcmp(argv[1],"stdout") == 0) {
        str_out = 1;
    }
    
    while(1) {
        vol_info v;

        char volstr[10];
        struct sysinfo sys_info;
        char timestr[30];
        char new_title[100];
        
        get_vol(volstr,&v,mute_exists);
        sysinfo(&sys_info);
        

        get_localtime(timestr);
        if (sensors_loaded == 0 ) {
            sbar_sensors_init(); 
        }

        double t1 =  get_chip_temp(temp_chip); 
        int    bm = 0 ;
        if (bat_exists) {            
            struct bat_info *b = bat_info_init(config[0]);
            bm = get_bat_left(b);
            cleanup(b);
        }
         
        if (bm == 0 ) {
            sprintf(new_title,"[V:%s|L:%0.2f|Free:%dM|%0.1fC] %s", volstr ,((float)sys_info.loads[0]/65536.0),sys_info.freeram*sys_info.mem_unit/(1024*1024),t1,timestr);
        } else {
            sprintf(new_title,"[V:%s|B:%dm|L:%0.2f|Free:%dM|%0.1fC] %s", volstr ,bm,((float)sys_info.loads[0]/65536.0),sys_info.freeram*sys_info.mem_unit/(1024*1024),t1,timestr);
        }

        if ( str_out == 1 ) {
            /* printf("%s\n",new_title);
            sprintf(new_title,"^i(/opt/bitmaps/volume.xbm)%s ^i(/opt/bitmaps/battery.xbm)%dm [load:%0.2f][free:%dM] %s", volstr ,get_battstr(b),((float)sys_info.loads[0]/65536.0),sys_info.freeram*sys_info.mem_unit/(1024*1024),timestr); */
            printf("%s\n",new_title);
        } else {
            /*
            perror("Error");
            set me free */
            x_root_title(new_title);
        }
        fflush(stdout);
        sleep(sleep_number);
        
    }
    
    return 0;
}
