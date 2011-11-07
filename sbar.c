/* sbar is simple status bar for dwm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


char *cpuconf = "/proc/cpuinfo"; 
char *batstate= "/proc/acpi/battery/BAT0/state";
char *vol_acpi= "/proc/acpi/ibm/volume";

static const unsigned int sleep_number = 1;

static char card[64] = "default";
static int smixer_level = 0;
static struct snd_mixer_selem_regopt smixer_options;


typedef struct cpu_info {
    char *conf;
    char *lastval;
    int interval;
} cpu_info;

typedef struct bat_uinfo {
    char *conf;
    char *lastval;
    int interval;
} bat_info;

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


typedef struct sbar_info {
    int cpustr;
    int batmin;
    char volstr[10];
    char datestr[32];

} sbar_info;

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

int get_localtime(sbar_info *sbi) {
    time_t t;
    char outstr[100];
    char result[100];
    int ret;
    t = time(NULL);
    ret = strftime(outstr,sizeof(outstr),"%c",localtime(&t));
    strcpy(sbi->datestr,outstr);
    return 0;
}

int get_cpustr(sbar_info *sbi,cpu_info *cpu ) {
    cpu->interval = 60;
    cpu->conf     = cpuconf;
    char result[60];
    char b1[100];
    int b3;
    FILE *fd;
    fd = fopen(cpuconf,"r");
    while ( fscanf(fd,"%s",b1) != EOF) {
        if (strcmp(b1,"MHz") == 0) {
            char b2[20];
            fscanf(fd,"%s %d",b2,&b3);
            sbi->cpustr = (int)b3;
        } 
    }
    fclose(fd);
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
    fdbat = fopen(batstate,"r");
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

int get_battstr(sbar_info *sbi, bat_info *b) {
    sbi->batmin = read_battery_state();
    return 0;
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

int get_vol(sbar_info *sbi, vol_info *v) {
    v->mute = 0;
    v->percent  = alsa_get_vol();
    char buf_1[100];
    char vol[10]= " ";
    FILE *fd;
    fd = fopen(vol_acpi,"r");
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
    if (v->mute == 1 ) { 
        sprintf(vol,"%d%% muted",v->percent);
    } else {
        sprintf(vol,"%d%%",v->percent);
    }

    strcpy(sbi->volstr,vol);
    return 0; 
}

int x_root_title ( char *new_title) {
   Display *d;
   Window w;
   XEvent e;
   XTextProperty t;
   int s;
   char **list;
   int n;
   d = XOpenDisplay(NULL);
   if (d == NULL) {
      fprintf(stderr, "Cannot open display\n");
      exit(1);
   }
 
   s = DefaultScreen(d);

   w = XDefaultRootWindow(d);
   XGetWMName(d,w,&t); 
   if(!t.nitems)
		printf("missing XTextProperty");
   if(t.encoding == XA_STRING) {
/*		printf("CURR %s",(char *)t.value); */
   } else {
		if(XmbTextPropertyToTextList(d, &t, &list, &n) >= Success && n > 0 && *list) {
			printf("SSSS %s", *list);
			XFreeStringList(list);
		}
	}
    char *text = (char *)new_title;
    XStringListToTextProperty(&text,1,&t);

    XSetWMName(d,w,&t); 
    XFree(t.value);
    XCloseDisplay(d);
    return 0;
}
int main( int argc, char **argv) {
    cpu_info c;
    bat_info b;
    vol_info v;

    char new_title[100];
    
    while(1) {
        sbar_info sb;
        sb.cpustr = 0;
        sb.batmin= 0;
        get_cpustr(&sb,&c);
        //get_localtime(&sb);
        //get_battstr(&sb,&b);
        //get_vol(&sb,&v);
        sprintf(new_title,"[vol:%s][bat:%dm][cpu:%dMHz] %s", sb.volstr , sb.batmin,sb.cpustr,sb.datestr);
        /*
        perror("Error");
        set me free */
        x_root_title(new_title);
        sleep(sleep_number);
    }
    return 0;
}
