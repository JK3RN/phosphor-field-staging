#define _POSIX_C_SOURCE 200809L
#include "../src/theme.h"
#include "../src/observe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
static const char green[]="background = \"#020905\"\nforeground = \"#97dca4\"\naccent = \"#68c17f\"\nyellow = \"#d3bb67\"\n";
static const char light[]="mode = \"light\"\nbackground = \"#eff1f5\"\nforeground = \"#4c4f69\"\naccent = \"#1e66f5\"\nyellow = \"#df8e1d\"\n";
static void write_palette(const char *path,const char *data){FILE *f=fopen(path,"w");assert(f);assert(fputs(data,f)>=0);assert(fclose(f)==0);}
int main(void){
    Palette p;assert(palette_parse(green,&p));assert(palette_contrast(p.foreground,p.background)>=4.5);
    assert(palette_contrast(p.dim,p.background)>=3);assert(p.signal.a==96);
    assert(palette_parse(light,&p));assert(palette_contrast(p.signal,p.background)>=4.5);
    assert(palette_contrast(p.axis,p.background)<palette_contrast(p.signal,p.background));
    assert(!palette_parse("background = \"#000000\"",&p));
    assert(!palette_parse("background = \"#xxxxxx\"\nforeground = \"#ffffff\"\naccent = \"#88ffaa\"",&p));
    assert(!palette_parse("background = \"#000000\"\nbackground = \"#ffffff\"",&p));
    char temp[]="/tmp/phosphor-operational-XXXXXX";assert(mkdtemp(temp));
    char file[512];snprintf(file,sizeof file,"%s/colors.toml",temp);write_palette(file,green);
    Theme t;assert(theme_init(&t,0,file));assert(theme_poll(&t,0)&THEME_EVENT);assert(t.valid);
    Palette saved=t.palette;write_palette(file,light);
    assert(!theme_poll(&t,1));assert(theme_poll(&t,2)&THEME_COLORS);assert(t.valid);
    write_palette(file,"accent = \"broken\"");assert(theme_poll(&t,3)==THEME_EVENT);assert(!strcmp(t.event,"theme_retained"));
    assert(!theme_poll(&t,4));unlink(file);assert(!theme_poll(&t,5));
    write_palette(file,green);assert(!theme_poll(&t,6));assert(theme_poll(&t,7)&THEME_COLORS);assert(!memcmp(&saved,&t.palette,sizeof saved));
    Theme missing;unlink(file);assert(theme_init(&missing,0,file));assert(theme_poll(&missing,0)==THEME_EVENT);assert(!missing.valid);
    p=palette_fallback();assert(!memcmp(&p,&missing.palette,sizeof p));
    assert(theme_init(&t,1,NULL));assert(theme_poll(&t,0)==THEME_EVENT);assert(!theme_poll(&t,100));
    char quoted[100];json_quote(quoted,sizeof quoted,"a\"b\\c\n\001");assert(!strcmp(quoted,"\"a\\\"b\\\\c\\u000a\\u0001\""));
    Log log;assert(log_open(&log,temp,4096));char record[201];memset(record,' ',sizeof record);memcpy(record,"{\"schema\":1}",12);record[200]='\n';
    for(int i=0;i<100;i++){assert(log_record(&log,record,sizeof record));}
    assert(log.records==100);
    Log second;assert(!log_open(&second,temp,4096));log_close(&second);log_close(&log);
    char child[600];const char *names[]={"events.jsonl","events.1.jsonl","events.2.jsonl"};
    for(int i=0;i<3;i++){snprintf(child,sizeof child,"%s/phosphor-field/%s",temp,names[i]);struct stat st;assert(stat(child,&st)==0&&st.st_size<=4096);unlink(child);}
    snprintf(child,sizeof child,"%s/phosphor-field/.lock",temp);unlink(child);
    snprintf(child,sizeof child,"%s/phosphor-field/.owner",temp);unlink(child);
    assert(!log_open(&log,temp,4096));log_close(&log); /* refuses a foreign/unmarked directory */
    snprintf(child,sizeof child,"%s/phosphor-field",temp);rmdir(child);rmdir(temp);
    log=(Log){.directory=-1,.lock=-1,.file=open("/dev/full",O_WRONLY),.limit=4096};assert(log.file>=0);
    assert(!log_record(&log,"{}\n",3)&&log.failed&&log.file==-1);log_close(&log);
    Work work={0};work_add(&work,.5);work_add(&work,4);work_add(&work,110);
    assert(work.frames==3&&work.bins[0]==1&&work.bins[3]==1&&work.bins[8]==1&&work.max_ms==110);
    puts("PASS: palette validation, contrast, stabilization, retention/recovery/fallback, bounded rotation, ownership/lock, ENOSPC, JSON escaping, histogram");
}
