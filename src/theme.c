#define _POSIX_C_SOURCE 200809L
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
Palette palette_fallback(void){
    return (Palette){{2,9,5,255},{151,220,164,255},{93,139,105,255},{211,187,103,255},
        {104,193,127,96},{12,26,18,255},{24,48,31,255},{42,78,49,255}};
}
static SDL_Color mix(SDL_Color a,SDL_Color b,double t){
    return (SDL_Color){(Uint8)lround(a.r+(b.r-a.r)*t),(Uint8)lround(a.g+(b.g-a.g)*t),(Uint8)lround(a.b+(b.b-a.b)*t),255};
}
static double linear(unsigned char c){double v=c/255.;return v<=.04045?v/12.92:pow((v+.055)/1.055,2.4);}
static double lum(SDL_Color c){return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b);}
double palette_contrast(SDL_Color a,SDL_Color b){double x=lum(a),y=lum(b);return (fmax(x,y)+.05)/(fmin(x,y)+.05);}
static SDL_Color readable(SDL_Color c,SDL_Color background,double minimum){
    SDL_Color white={255,255,255,255},original=c;
    for(int i=1;i<=100&&palette_contrast(c,background)<minimum;i++)c=mix(original,white,i/100.);
    return c;
}
static int hexcolor(const char *value,SDL_Color *c){
    if(strlen(value)!=7||value[0]!='#')return 0;
    for(int i=1;i<7;i++)if(!isxdigit((unsigned char)value[i]))return 0;
    unsigned n=(unsigned)strtoul(value+1,NULL,16);*c=(SDL_Color){n>>16,(n>>8)&255,n&255,255};return 1;
}
int palette_parse(const char *data,Palette *out){
    SDL_Color background={0},foreground={0},accent={0},yellow={211,187,103,255};
    int seen=0;const char *p=data;
    while(*p){
        const char *end=strchr(p,'\n');if(!end)end=p+strlen(p);
        size_t length=(size_t)(end-p);if(length>=512)return 0;
        char line[512];memcpy(line,p,length);line[length]=0;p=*end?end+1:end;
        char *q=line;while(isspace((unsigned char)*q))q++;
        if(!*q||*q=='#')continue;
        char key[80];int k=0;
        while(isalnum((unsigned char)*q)||*q=='_'){if(k==79)return 0;key[k++]=*q++;}key[k]=0;
        while(isspace((unsigned char)*q))q++;
        if(!k||*q++!='=')return 0;
        int role=!strcmp(key,"background")?1:!strcmp(key,"foreground")?2:!strcmp(key,"accent")?4:
            !strcmp(key,"yellow")?8:!strcmp(key,"color3")?16:0;
        if(!role)continue; /* This is a bounded Omarchy flat-palette reader, not general TOML. */
        if(seen&role)return 0;
        while(isspace((unsigned char)*q))q++;
        char quote=*q++;if(quote!='"'&&quote!='\'')return 0;
        char value[8];if(strlen(q)<8)return 0;memcpy(value,q,7);value[7]=0;q+=7;
        if(*q++!=quote)return 0;
        while(isspace((unsigned char)*q))q++;
        if(*q&&*q!='#')return 0;
        SDL_Color c;if(!hexcolor(value,&c))return 0;
        if(role==1)background=c;else if(role==2)foreground=c;else if(role==4)accent=c;
        else if(role==8||!(seen&8))yellow=c;
        seen|=role;
    }
    if((seen&7)!=7)return 0;
    /* The instrument stays dark even for light desktop palettes: tinted glass derived from text. */
    if(lum(background)>.18)background=mix((SDL_Color){0,0,0,255},foreground,.14);
    SDL_Color signal=mix(accent,foreground,.20);
    unsigned peak=signal.r>signal.g?signal.r:signal.g;if(signal.b>peak)peak=signal.b;
    if(peak>193)signal=mix((SDL_Color){0,0,0,255},signal,193./peak);
    signal=readable(signal,background,4.5);
    Palette result={.background=background,.foreground=readable(foreground,background,4.5)};
    result.dim=readable(mix(background,result.foreground,.65),background,3);
    result.warning=readable(yellow,background,4.5);
    result.minor=mix(background,signal,.10);result.major=mix(background,signal,.22);result.axis=mix(background,signal,.35);
    signal.a=96;result.signal=signal;*out=result;return 1;
}
static uint64_t hash(uint64_t h,const char *s){for(;*s;s++){h^=(unsigned char)*s;h*=UINT64_C(1099511628211);}return h;}
/* Detect an in-place write during reading as well as directory replacement. */
static int read_file(const char *path,char *data,size_t capacity){
    int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)return 0;
    FILE *f=fdopen(fd,"rb");if(!f){close(fd);return 0;}
    struct stat before,after,current;
    int ok=fstat(fileno(f),&before)==0&&S_ISREG(before.st_mode)&&before.st_size>0&&(uint64_t)before.st_size<capacity;
    size_t n=ok?fread(data,1,capacity-1,f):0;
    if(ok)ok=!ferror(f)&&n==(size_t)before.st_size&&fstat(fileno(f),&after)==0&&stat(path,&current)==0&&
        before.st_ino==after.st_ino&&before.st_dev==after.st_dev&&before.st_size==after.st_size&&
        before.st_mtim.tv_sec==after.st_mtim.tv_sec&&before.st_mtim.tv_nsec==after.st_mtim.tv_nsec&&
        before.st_ino==current.st_ino&&before.st_dev==current.st_dev;
    fclose(f);data[n]=0;return ok&&strlen(data)==n;
}
int theme_set_current(Theme *t,const char *directory){
    int a=snprintf(t->file,sizeof t->file,"%s/theme/colors.toml",directory);
    int b=snprintf(t->name_file,sizeof t->name_file,"%s/theme.name",directory);
    return a>0&&(size_t)a<sizeof t->file&&b>0&&(size_t)b<sizeof t->name_file;
}
int theme_init(Theme *t,int fixed,const char *file){
    *t=(Theme){.palette=palette_fallback(),.fixed=fixed};snprintf(t->name,sizeof t->name,"phosphor-fallback");
    if(fixed)return 1;
    if(file){int n=snprintf(t->file,sizeof t->file,"%s",file);return n>0&&(size_t)n<sizeof t->file;}
    /* The inspected Omarchy commands use HOME/.local/state, not XDG_STATE_HOME. */
    const char *home=getenv("HOME");char current[4096];if(!home||!*home)return 0;
    int n=snprintf(current,sizeof current,"%s/.local/state/omarchy/current",home);
    return n>0&&(size_t)n<sizeof current&&theme_set_current(t,current);
}
int theme_poll(Theme *t,double now){
    if(t->initialized&& (t->fixed||now<t->next_poll))return 0;
    t->next_poll=now+1;
    int first=!t->initialized;t->initialized=1;
    if(t->fixed){strcpy(t->event,"theme_fallback");return THEME_EVENT;}
    char data[16384],name[80]="file-palette",again[80];Palette candidate;
    int ok=1;
    if(*t->name_file)ok=read_file(t->name_file,name,sizeof name);
    if(ok)ok=read_file(t->file,data,sizeof data)&&palette_parse(data,&candidate);
    if(ok&&*t->name_file)ok=read_file(t->name_file,again,sizeof again)&&!strcmp(name,again);
    if(!ok){
        t->pending=0;
        if(!t->failed||first){t->failed=1;strcpy(t->event,t->valid?"theme_retained":"theme_fallback");return THEME_EVENT;}
        return 0;
    }
    for(char *p=name;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_'&&*p!=' ') *p=0;
    if(!*name)strcpy(name,"unnamed");
    uint64_t signature=hash(hash(UINT64_C(1469598103934665603),data),name);
    if(!first&&signature!=t->applied&&signature!=t->pending){t->pending=signature;return 0;}
    if(signature==t->applied&&!t->failed)return 0;
    int changed=memcmp(&t->palette,&candidate,sizeof candidate)!=0;
    snprintf(t->event,sizeof t->event,"%s",t->failed?"theme_recovered":t->valid?"theme_changed":"theme_loaded");
    t->palette=candidate;t->applied=signature;t->pending=0;t->valid=1;t->failed=0;snprintf(t->name,sizeof t->name,"%s",name);
    return THEME_EVENT|(changed?THEME_COLORS:0);
}
