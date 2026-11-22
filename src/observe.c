#define _POSIX_C_SOURCE 200809L
#include "observe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
double monotonic_seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
double process_cpu_seconds(void){struct timespec t;if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t))return -1;return t.tv_sec+t.tv_nsec/1e9;}
long long process_rss_bytes(void){
    FILE *f=fopen("/proc/self/statm","r");if(!f)return -1;
    unsigned long ignored,resident;int ok=fscanf(f,"%lu %lu",&ignored,&resident)==2;fclose(f);
    long page=sysconf(_SC_PAGESIZE);return ok&&page>0?(long long)resident*page:-1;
}
void work_add(Work *w,double ms){
    static const double bounds[]={1,2,4,8,16,33,66,100,250};
    unsigned b=0;while(b<9&&ms>=bounds[b])b++;
    w->frames++;w->sum_ms+=ms;if(ms>w->max_ms)w->max_ms=ms;w->bins[b]++;
}
static void log_fail(Log *l){
    if(!l->failed)fputs("phosphor-field: file logging disabled after an I/O or ownership error\n",stderr);
    l->failed=1;if(l->file>=0){close(l->file);l->file=-1;}
}
static int own_regular(int fd){struct stat s;return fstat(fd,&s)==0&&S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&s.st_nlink==1;}
static int safe_entry(Log *l,const char *name){
    int fd=openat(l->directory,name,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return errno==ENOENT;
    int ok=own_regular(fd);close(fd);return ok;
}
void log_close(Log *l){
    if(l->file>=0)close(l->file);
    if(l->lock>=0)close(l->lock);
    if(l->directory>=0)close(l->directory);
    l->file=l->lock=l->directory=-1;
}
int log_open(Log *l,const char *base,size_t limit){
    *l=(Log){.directory=-1,.lock=-1,.file=-1,.limit=limit};if(!base)return 1;
    if(mkdir(base,0700)<0&&errno!=EEXIST)goto fail;
    int parent=open(base,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(parent<0)goto fail;
    int created=mkdirat(parent,"phosphor-field",0700)==0;
    l->directory=openat(parent,"phosphor-field",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(parent);
    if(l->directory<0)goto fail;
    struct stat ds;if(fstat(l->directory,&ds)<0||ds.st_uid!=geteuid()||(ds.st_mode&0022))goto fail;
    const char magic[]="phosphor-field-log-v1\n";
    int marker=openat(l->directory,".owner",(created?O_WRONLY|O_CREAT|O_EXCL:O_RDONLY)|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC,0600);
    if(marker<0)goto fail;
    int ok=own_regular(marker);
    if(!ok){close(marker);goto fail;}
    if(created){if(write(marker,magic,sizeof magic-1)!=(ssize_t)(sizeof magic-1))ok=0;}
    else {char value[64]={0};ssize_t n=read(marker,value,sizeof value);if(n!=(ssize_t)(sizeof magic-1)||memcmp(value,magic,sizeof magic-1))ok=0;}
    close(marker);if(!ok)goto fail;
    l->lock=openat(l->directory,".lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
    if(l->lock<0||!own_regular(l->lock)||flock(l->lock,LOCK_EX|LOCK_NB)<0)goto fail;
    const char *names[]={"events.jsonl","events.1.jsonl","events.2.jsonl"};
    for(int i=0;i<3;i++)if(!safe_entry(l,names[i]))goto fail;
    l->file=openat(l->directory,"events.jsonl",O_WRONLY|O_APPEND|O_CREAT|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC,0600);
    if(l->file<0||!own_regular(l->file))goto fail;
    struct stat st;if(fstat(l->file,&st)<0)goto fail;l->bytes=(size_t)st.st_size;
    return 1;
fail:log_fail(l);log_close(l);return 0;
}
static int rotate(Log *l){
    const char *names[]={"events.jsonl","events.1.jsonl","events.2.jsonl"};
    for(int i=0;i<3;i++)if(!safe_entry(l,names[i]))return 0;
    close(l->file);l->file=-1;
    if(unlinkat(l->directory,names[2],0)<0&&errno!=ENOENT)return 0;
    for(int i=1;i>=0;i--)if(renameat(l->directory,names[i],l->directory,names[i+1])<0&&errno!=ENOENT)return 0;
    l->file=openat(l->directory,names[0],O_WRONLY|O_APPEND|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(l->file<0)return 0;
    l->bytes=0;return 1;
}
int log_record(Log *l,const char *line,size_t size){
    if(l->file<0)return 0;
    if(size>l->limit){log_fail(l);return 0;}
    if(l->bytes+size>l->limit&&!rotate(l)){log_fail(l);return 0;}
    size_t written=0;
    while(written<size){
        ssize_t n=write(l->file,line+written,size-written);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){/* Best effort: avoid retaining a partial JSON record on an I/O failure. */
            (void)ftruncate(l->file,(off_t)l->bytes);log_fail(l);return 0;
        }
        written+=(size_t)n;
    }
    l->bytes+=size;l->records++;return 1;
}
void json_quote(char *out,size_t cap,const char *s){
    if(cap<3){if(cap)*out=0;return;}size_t n=0;out[n++]='"';
    for(;*s;s++){
        unsigned char c=(unsigned char)*s;char token[8];size_t len;
        if(c=='"'||c=='\\'){token[0]='\\';token[1]=c;len=2;}
        else if(c<32||c>=127){snprintf(token,sizeof token,"\\u%04x",c);len=6;}
        else {token[0]=c;len=1;}
        if(n+len+2>cap)break;
        memcpy(out+n,token,len);n+=len;
    }
    out[n++]='"';out[n]=0;
}
void observer_init(Observer *o,const char *dir,size_t limit,int status,int telemetry,double interval){
    *o=(Observer){.status=status,.telemetry=telemetry,.start=monotonic_seconds(),.interval=interval};
    struct timespec run_time;clock_gettime(CLOCK_REALTIME,&run_time);
    snprintf(o->run_id,sizeof o->run_id,"%lld-%09ld-%ld",(long long)run_time.tv_sec,run_time.tv_nsec,(long)getpid());
    o->sample_start=o->start;o->next_sample=o->start+interval;o->cpu_start=process_cpu_seconds();
    log_open(&o->log,dir,limit);
    struct stat output;
    if(status&&fstat(STDOUT_FILENO,&output)==0&&S_ISFIFO(output.st_mode)){
        int flags=fcntl(STDOUT_FILENO,F_GETFL);
        if(flags>=0&&fcntl(STDOUT_FILENO,F_SETFL,flags|O_NONBLOCK)==0)o->status_pipe=1;
    }
    /* A closed status consumer must not bypass the orderly cleanup path. */
    struct sigaction sa={0};sa.sa_handler=SIG_IGN;sigaction(SIGPIPE,&sa,NULL);
}
void observer_emit(Observer *o,const char *severity,const char *event,const char *data,int status){
    if(o->log.file<0&&!(o->status&&status))return;
    char stamp[40],quoted[96],line[8192];struct timespec ts;clock_gettime(CLOCK_REALTIME,&ts);struct tm utc;gmtime_r(&ts.tv_sec,&utc);
    strftime(stamp,sizeof stamp,"%Y-%m-%dT%H:%M:%S",&utc);
    json_quote(quoted,sizeof quoted,event);
    int n=snprintf(line,sizeof line,"{\"schema\":1,\"program\":\"phosphor-field\",\"version\":\"" PHOSPHOR_VERSION "\",\"run_id\":\"%s\",\"time\":\"%s.%03ldZ\",\"elapsed_s\":%.6f,\"severity\":\"%s\",\"event\":%s,\"data\":%s}\n",o->run_id,stamp,ts.tv_nsec/1000000,monotonic_seconds()-o->start,severity,quoted,data?data:"{}");
    if(n<0||(size_t)n>=sizeof line){log_fail(&o->log);return;}
    if(o->log.file>=0)log_record(&o->log,line,(size_t)n);
    if(o->status&&status){
        int ok=1;
        if(o->status_pipe){
            /* Records <= PIPE_BUF are atomic: either one complete JSON line or no bytes. */
            ssize_t sent;
            do {sent=n<=PIPE_BUF?write(STDOUT_FILENO,line,(size_t)n):-1;}while(sent<0&&errno==EINTR&&n<=PIPE_BUF);
            if(n>PIPE_BUF||(sent<0&&(errno==EAGAIN||errno==EWOULDBLOCK))){
                if(!o->status_dropped)fputs("phosphor-field: slow status consumer; complete records dropped\n",stderr);
                o->status_dropped++;return;
            }
            ok=sent==n;
        }else ok=fwrite(line,1,(size_t)n,stdout)==(size_t)n&&fflush(stdout)==0;
        if(!ok){fputs("phosphor-field: status output unavailable; continuing\n",stderr);o->status=0;clearerr(stdout);}
    }
}
