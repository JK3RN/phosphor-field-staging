#ifndef PHOSPHOR_OBSERVE_H
#define PHOSPHOR_OBSERVE_H
#include <stdint.h>
#include <stddef.h>
#define PHOSPHOR_VERSION "0.6.0"
#define LOG_BACKUPS 2
typedef struct { int directory,lock,file,failed; size_t bytes,limit; uint64_t records; } Log;
typedef struct { char run_id[64]; Log log; int status,telemetry,status_pipe; uint64_t status_dropped; double start,interval,next_sample,sample_start,cpu_start; } Observer;
typedef struct { uint64_t frames; double sum_ms,max_ms; uint64_t bins[10]; } Work;
double monotonic_seconds(void);
double process_cpu_seconds(void);
long long process_rss_bytes(void);
void work_add(Work *work,double ms);
int log_open(Log *log,const char *base,size_t limit);
void log_close(Log *log);
int log_record(Log *log,const char *line,size_t size);
void json_quote(char *out,size_t capacity,const char *value);
void observer_init(Observer *o,const char *log_dir,size_t limit,int status,int telemetry,double interval);
void observer_emit(Observer *o,const char *severity,const char *event,const char *data,int status);
#endif
