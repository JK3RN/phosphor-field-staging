#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field.h"
#include "generator.h"
#include "theme.h"
#include "observe.h"
#include <signal.h>
#include <errno.h>
#define MAX_POINTS 50000
/* Semantic roles: defaults are the fixed phosphor palette; native palettes replace them together. */
static SDL_Color bg={2,9,5,255},fg={151,220,164,255},dim={93,139,105,255},warn={211,187,103,255};
static SDL_Color signal_color={104,193,127,96};
static SDL_Color grid_minor={12,26,18,255},grid_major={24,48,31,255},grid_axis={42,78,49,255};
static TTF_Font *font;
static int ui_scale=1;
static size_t active_generator;
static int observer_lab_active;
static char observer_creature[40]="lab-preview";
static uint64_t generator_generation;
#define TEXT_CAPACITY 64
typedef struct { SDL_Texture *texture; char value[256]; SDL_Color color; int w,h; Uint64 used; } TextEntry;
static TextEntry cache[TEXT_CAPACITY];
static Uint64 text_clock, text_builds;
static void text_clear(void){
    for(int i=0;i<TEXT_CAPACITY;i++){SDL_DestroyTexture(cache[i].texture);cache[i]=(TextEntry){0};}
}
static int text_live(void){int n=0;for(int i=0;i<TEXT_CAPACITY;i++)n+=cache[i].texture!=NULL;return n;}
static void color(SDL_Renderer *r,SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);}
static void text(SDL_Renderer *r,int x,int y,const char *s,SDL_Color c){
    TextEntry *entry=NULL,*oldest=&cache[0];
    for(int i=0;i<TEXT_CAPACITY;i++){
        TextEntry *e=&cache[i];
        if(e->texture&&!strcmp(e->value,s)&&!memcmp(&e->color,&c,sizeof c)){entry=e;break;}
        if(e->used<oldest->used)oldest=e;
    }
    if(!entry){
        SDL_Surface *surface=TTF_RenderUTF8_Blended(font,s,c);
        if(!surface)return;
        SDL_Texture *t=SDL_CreateTextureFromSurface(r,surface);
        if(!t){SDL_FreeSurface(surface);return;}
        entry=oldest;SDL_DestroyTexture(entry->texture);
        *entry=(TextEntry){.texture=t,.color=c,.w=surface->w,.h=surface->h};
        snprintf(entry->value,sizeof entry->value,"%s",s);
        SDL_FreeSurface(surface);text_builds++;
    }
    entry->used=++text_clock;
    SDL_Rect dest={x,y,entry->w,entry->h};
    /* Crop at the window edge rather than overlap a neighbouring bay. */
    SDL_RenderCopy(r,entry->texture,NULL,&dest);
}
static void apply_palette(Palette p){
    bg=p.background;fg=p.foreground;dim=p.dim;warn=p.warning;signal_color=p.signal;
    grid_minor=p.minor;grid_major=p.major;grid_axis=p.axis;text_clear();
}
static volatile sig_atomic_t stopped;
static void handle_signal(int sig){stopped=sig;}
typedef struct {
    Work all,warm,steady,interval;
    int calculated,drawn,w,h,hidden,invalid,clipped,calculated_lab;
    size_t calculated_generator;
    uint64_t calculated_generation;
    double calculated_phase;
    double elapsed_s,init_ms,sdl_ms,font_ms,renderer_ms,theme_ms;
    double first_ms,first_math_ms,first_submit_ms,first_face_ms,first_present_ms;
    char renderer[80],calculated_creature[40];
} RunStats;
static void work_json(char *out,size_t capacity,const Work *w){
    snprintf(out,capacity,"{\"frames\":%llu,\"mean_ms\":%.6f,\"max_ms\":%.6f,\"bins\":[%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu]}",
        (unsigned long long)w->frames,w->frames?w->sum_ms/w->frames:0,w->max_ms,
        (unsigned long long)w->bins[0],(unsigned long long)w->bins[1],(unsigned long long)w->bins[2],(unsigned long long)w->bins[3],
        (unsigned long long)w->bins[4],(unsigned long long)w->bins[5],(unsigned long long)w->bins[6],(unsigned long long)w->bins[7],
        (unsigned long long)w->bins[8],(unsigned long long)w->bins[9]);
}
static void aquarium_json(char *out,size_t cap);
static void snapshot_json(char *out,size_t cap,const RunStats *st,const Settings *s,const Theme *t){
    char renderer[180],name[180];json_quote(renderer,sizeof renderer,st->renderer);json_quote(name,sizeof name,t->name);
    long long rss=process_rss_bytes();char memory[40];if(rss<0)strcpy(memory,"null");else snprintf(memory,sizeof memory,"%lld",rss);
    char aq[160];aquarium_json(aq,sizeof aq);
    const Generator *active=generator_at(active_generator);
    const char *calculated=st->calculated?(st->calculated_lab?st->calculated_creature:generator_at(st->calculated_generator)->id):NULL;
    char calculated_id[64];json_quote(calculated_id,sizeof calculated_id,calculated?calculated:"");
    snprintf(out,cap,"{\"width_px\":%d,\"height_px\":%d,\"hidden\":%s,\"paused\":%s,\"calculated\":%d,\"drawn\":%d,\"requested_points\":%d,\"phase\":%.9f,\"zoom\":%.6f,\"speed\":%.6f,\"decay_s\":%.6f,\"grid\":%s,\"trail\":%s,\"help\":%s,\"controls\":%s,\"dpi_scale\":%d,\"renderer\":%s,\"theme\":%s,\"theme_valid\":%s,\"rss_bytes\":%s,\"text_live\":%d,\"text_builds\":%llu,\"generator\":\"%s\",\"generation\":%llu,\"calculated_generator\":%s,\"calculated_generation\":%llu,\"calculated_phase\":%.9f,\"invalid\":%d,\"clipped\":%d,\"aquarium\":%s}",
        st->w,st->h,st->hidden?"true":"false",s->paused?"true":"false",st->calculated,st->drawn,s->points,s->t,s->zoom,s->speed,s->tau,s->grid?"true":"false",s->trail?"true":"false",s->help?"true":"false",s->controls?"true":"false",ui_scale,renderer,name,t->valid?"true":"false",memory,text_live(),(unsigned long long)text_builds,observer_lab_active?observer_creature:active->id,(unsigned long long)generator_generation,calculated?calculated_id:"null",(unsigned long long)st->calculated_generation,st->calculated_phase,st->invalid,st->clipped,aq);
}
static void sample_tick(Observer *o,RunStats *st,const Settings *s,const Theme *t,double now){
    if(!o->telemetry||(o->log.file<0&&!o->status)||now<o->next_sample)return;
    double duration=now-o->sample_start,cpu=process_cpu_seconds();
    char work[512],state[1600],data[2600],cpu_json[64];
    if(cpu<0||o->cpu_start<0||duration<=0)strcpy(cpu_json,"null");else snprintf(cpu_json,sizeof cpu_json,"%.6f",100*(cpu-o->cpu_start)/duration);
    work_json(work,sizeof work,&st->interval);snapshot_json(state,sizeof state,st,s,t);
    snprintf(data,sizeof data,"{\"duration_s\":%.6f,\"fps\":%.6f,\"cpu_core_percent\":%s,\"status_dropped\":%llu,\"file_log_failed\":%s,\"work\":%s,\"state\":%s}",duration,duration>0?st->interval.frames/duration:0,cpu_json,(unsigned long long)o->status_dropped,o->log.failed?"true":"false",work,state);
    observer_emit(o,"info","sample",data,1);
    st->interval=(Work){0};o->sample_start=now;o->cpu_start=cpu;o->next_sample=now+o->interval;
}
static void theme_event(Observer *o,const Theme *t){
    char name[180],data[600];json_quote(name,sizeof name,t->name);
    snprintf(data,sizeof data,"{\"name\":%s,\"valid\":%s,\"source\":\"%s\",\"palette_id\":\"%016llx\",\"background\":\"#%02x%02x%02x\",\"signal\":\"#%02x%02x%02x\",\"foreground\":\"#%02x%02x%02x\"}",name,t->valid?"true":"false",t->fixed?"fixed":*t->name_file?"omarchy":"file",(unsigned long long)t->applied,t->palette.background.r,t->palette.background.g,t->palette.background.b,t->palette.signal.r,t->palette.signal.g,t->palette.signal.b,t->palette.foreground.r,t->palette.foreground.g,t->palette.foreground.b);
    observer_emit(o,t->failed?"warning":"info",t->event,data,0);
}
static void exit_event(Observer *o,const RunStats *st,const Settings *s,const Theme *t,int code,const char *reason){
    char all[512],warm[512],steady[512],state[1600],why[100],data[4200];
    work_json(all,sizeof all,&st->all);work_json(warm,sizeof warm,&st->warm);work_json(steady,sizeof steady,&st->steady);
    snapshot_json(state,sizeof state,st,s,t);json_quote(why,sizeof why,reason);
    snprintf(data,sizeof data,"{\"exit_code\":%d,\"reason\":%s,\"signal\":%d,\"status_dropped\":%llu,\"file_log_failed\":%s,\"render_elapsed_s\":%.9f,\"all_fps\":%.9f,\"init\":{\"total_ms\":%.6f,\"sdl_ms\":%.6f,\"font_ms\":%.6f,\"renderer_ms\":%.6f,\"theme_ms\":%.6f},\"first_frame\":{\"work_ms\":%.6f,\"math_ms\":%.6f,\"points_submit_ms\":%.6f,\"face_ms\":%.6f,\"present_ms\":%.6f},\"all\":%s,\"warmup\":%s,\"steady\":%s,\"state\":%s}",
        code,why,(int)stopped,(unsigned long long)o->status_dropped,o->log.failed?"true":"false",st->elapsed_s,st->elapsed_s>0?st->all.frames/st->elapsed_s:0,st->init_ms,st->sdl_ms,st->font_ms,st->renderer_ms,st->theme_ms,st->first_ms,st->first_math_ms,st->first_submit_ms,st->first_face_ms,st->first_present_ms,all,warm,steady,state);
    observer_emit(o,code?"error":"info","exit",data,1);
}
static int number(const char *value,double *result,double min,double max){
    char *end;errno=0;double n=strtod(value,&end);
    if(errno||!*value||*end||!isfinite(n)||n<min||n>max)return 0;
    *result=n;return 1;
}
typedef struct { SDL_Rect plot; int compact,beacon,panel; } Layout;
static Layout layout(int w,int h,Settings s){
    int u=ui_scale;w/=u;h/=u;
    int compact=w<600||h<400, beacon=w<280||h<180;
    int panel=s.help?153:s.controls?54:0;
    if(h-panel<(s.help?210:180))panel=0;
    return (Layout){{38*u,32*u,(w>50?w-48:1)*u,(h-32-(compact?58:80)-panel)*u},compact,beacon,panel*u};
}

static void select_generator(Settings *s,size_t index){
    if(index==active_generator||!generator_at(index))return;
    active_generator=index;generator_generation++;s->t=0;text_clear();
}
static int input(Settings *s,SDL_Keycode k){
    switch(k){
    case SDLK_q:case SDLK_ESCAPE:return 0;
    case SDLK_SPACE:s->paused=!s->paused;break;
    case SDLK_h:s->help=!s->help;s->controls=0;break;
    case SDLK_k:s->controls=!s->controls;s->help=0;break;
    case SDLK_r:*s=defaults();select_generator(s,0);break;
    case SDLK_1:case SDLK_2:case SDLK_3:case SDLK_4:case SDLK_5:
    case SDLK_6:case SDLK_7:case SDLK_8:case SDLK_9:
        select_generator(s,(size_t)(k-SDLK_1));break;
    case SDLK_g:s->grid=!s->grid;break;
    case SDLK_p:s->trail=!s->trail;break;
    case SDLK_UP:s->points+=2500;if(s->points>MAX_POINTS)s->points=MAX_POINTS;break;
    case SDLK_DOWN:s->points-=2500;if(s->points<2500)s->points=2500;break;
    case SDLK_RIGHT:s->speed=fmin(4,s->speed+.25);break;
    case SDLK_LEFT:s->speed=fmax(.25,s->speed-.25);break;
    case SDLK_EQUALS:case SDLK_PLUS:case SDLK_KP_PLUS:s->zoom=fmin(4,s->zoom*1.1);break;
    case SDLK_MINUS:case SDLK_KP_MINUS:s->zoom=fmax(.25,s->zoom/1.1);break;
    case SDLK_n:case SDLK_LEFTBRACKET:s->tau=fmax(.035,s->tau/1.4);break;
    case SDLK_m:case SDLK_RIGHTBRACKET:s->tau=fmin(.6,s->tau*1.4);break;
    default:break;
    }return 1;
}
/* One KEYDOWN, one action. TEXTINPUT is deliberately not a shortcut source. */
static int key_event(Settings *s,const SDL_KeyboardEvent *e,int *dirty){
    if(e->type!=SDL_KEYDOWN||e->repeat)return 1;
    SDL_Keycode key=e->keysym.sym;
    if(e->keysym.mod&(KMOD_RALT|KMOD_MODE)){
        if(e->keysym.scancode==SDL_SCANCODE_8)key=SDLK_LEFTBRACKET;
        else if(e->keysym.scancode==SDL_SCANCODE_9)key=SDLK_RIGHTBRACKET;
    }
    /* Modified number keys are not figure selectors; AltGr 8/9 were mapped above. */
    if(key>=SDLK_1&&key<=SDLK_9&&(e->keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI|KMOD_SHIFT|KMOD_MODE)))return 1;
    Settings before=*s;size_t previous_generator=active_generator;
    int run=input(s,key);
    if(before.points!=s->points||before.grid!=s->grid||before.trail!=s->trail||before.paused!=s->paused||
       previous_generator!=active_generator||before.help!=s->help||before.controls!=s->controls||before.speed!=s->speed||before.zoom!=s->zoom||before.t!=s->t||before.tau!=s->tau)*dirty=1;
    return run;
}
#include "lab.h"
#include "aquarium.h"
static void grid(SDL_Renderer *r,Layout l,double scale){
    SDL_Rect p=l.plot; double cx=p.x+p.w/2.,cy=p.y+p.h/2.;
    SDL_RenderSetClipRect(r,&p);
    for(int axis=0;axis<2;axis++)for(int n=-100;n<=100;n++){
        double v=(axis?cy:cx)+n*25*scale;
        color(r,n==0?grid_axis:n%4==0?grid_major:grid_minor);
        if(axis)SDL_RenderDrawLineF(r,p.x,v,p.x+p.w,v);else SDL_RenderDrawLineF(r,v,p.y,v,p.y+p.h);
    }
    SDL_RenderSetClipRect(r,NULL);
    int x_stride=(int)ceil(48*ui_scale/(100*scale));
    int y_stride=(int)ceil(18*ui_scale/(100*scale));
    for(int n=-4;n<=4;n++){
        char b[24];snprintf(b,sizeof b,"%d",n*100);
        int x=(int)(cx+n*100*scale),y=(int)(cy+n*100*scale);
        color(r,dim);
        if(n%x_stride==0&&x>=p.x+12*ui_scale&&x<p.x+p.w-16*ui_scale){SDL_RenderDrawLine(r,x,p.y+p.h,x,p.y+p.h+4);text(r,x-9*ui_scale,p.y+p.h+5*ui_scale,b,dim);}
        if(n%y_stride==0&&y>=p.y&&y<p.y+p.h-12*ui_scale){SDL_RenderDrawLine(r,p.x-4*ui_scale,y,p.x,y);text(r,ui_scale,y-7*ui_scale,b,dim);}
    }
}
static int screenshot(SDL_Renderer *r,const char *path,int w,int h){
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    int ok=s&&SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch)==0&&SDL_SaveBMP(s,path)==0;
    if(!ok)fprintf(stderr,"Screenshot: %s\n",SDL_GetError());
    SDL_FreeSurface(s);return ok;
}
int main(int argc,char **argv){
    int qa=0,input_debug=0,status_json=0,telemetry=1,fixed_theme=0;
    int w=760,h=600;double seconds=0,sample_seconds=5;size_t log_limit=1048576;
    const char *shot=NULL,*log_dir=NULL,*theme_file=NULL,*theme_current_dir=NULL;Settings s=defaults();
    for(int i=1;i<argc;i++){
        double n;
        if(!strcmp(argv[i],"--help")){
            puts("phosphor-field " PHOSPHOR_VERSION " [--size W H] [--points N] [--seconds N] [--screenshot file.bmp] [--qa] [--input-debug]\n"
                "[--log-dir DIR] [--log-max-bytes N] [--status-json] [--sample-seconds N] [--no-telemetry]\n"
                "[--theme auto|phosphor] [--theme-file colors.toml | --theme-current DIR] [--version]\n"
                "A Aquarium / comma,period interval 5..120s / A Esc restore / 1s fade / Space pause\n"
                "L Creation Lab / Tab focus / Ctrl+S apply / F6 rollback / Ctrl+D save / B library / Ctrl+R starter / Esc back\n"
                "1-0 favorites (initial: 1 Medusa / 2-0 free) / H help / K controls / Q Esc quit / Space pause / R reset\n"
                "Up Down points / Left Right speed / + - zoom / G grid / P persistence / N M decay ([ ] secondary) / X Y are axis labels");return 0;
        }
        else if(!strcmp(argv[i],"--version")){puts("phosphor-field " PHOSPHOR_VERSION " (status schema 1)");return 0;}
        else if(!strcmp(argv[i],"--qa"))qa=1;
        else if(!strcmp(argv[i],"--input-debug"))input_debug=1;
        else if(!strcmp(argv[i],"--status-json"))status_json=1;
        else if(!strcmp(argv[i],"--no-telemetry"))telemetry=0;
        else if(!strcmp(argv[i],"--log-dir")&&i+1<argc)log_dir=argv[++i];
        else if(!strcmp(argv[i],"--theme-file")&&i+1<argc)theme_file=argv[++i];
        else if(!strcmp(argv[i],"--theme-current")&&i+1<argc)theme_current_dir=argv[++i];
        else if(!strcmp(argv[i],"--theme")&&i+1<argc){
            const char *mode=argv[++i];if(!strcmp(mode,"phosphor"))fixed_theme=1;else if(!strcmp(mode,"auto"))fixed_theme=0;else goto invalid;
        }
        else if(!strcmp(argv[i],"--size")&&i+2<argc){
            if(!number(argv[++i],&n,160,4096)||floor(n)!=n)goto invalid;
            w=(int)n;
            if(!number(argv[++i],&n,120,4096)||floor(n)!=n)goto invalid;
            h=(int)n;
        }
        else if(!strcmp(argv[i],"--points")&&i+1<argc){if(!number(argv[++i],&n,1,MAX_POINTS)||floor(n)!=n)goto invalid;s.points=(int)n;}
        else if(!strcmp(argv[i],"--seconds")&&i+1<argc){if(!number(argv[++i],&seconds,0,31536000))goto invalid;}
        else if(!strcmp(argv[i],"--sample-seconds")&&i+1<argc){if(!number(argv[++i],&sample_seconds,.1,3600))goto invalid;}
        else if(!strcmp(argv[i],"--log-max-bytes")&&i+1<argc){if(!number(argv[++i],&n,4096,67108864)||floor(n)!=n)goto invalid;log_limit=(size_t)n;}
        else if(!strcmp(argv[i],"--screenshot")&&i+1<argc)shot=argv[++i];
        else goto invalid;
    }
    if((fixed_theme&&(theme_file||theme_current_dir))||(theme_file&&theme_current_dir))goto invalid;
    active_generator=0;generator_generation=0;
    stopped=0;struct sigaction sa={0};sa.sa_handler=handle_signal;sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM,&sa,NULL);sigaction(SIGINT,&sa,NULL);
    Observer observer;observer_init(&observer,log_dir,log_limit,status_json,telemetry,sample_seconds);
    observer_emit(&observer,"info","start","{}",0);
    Theme theme;RunStats stats={.w=w,.h=h};const char *reason="error",*error_stage="startup";
    double init_step=monotonic_seconds();
    int theme_path_ok=theme_init(&theme,fixed_theme,theme_file);
    if(theme_current_dir)theme_path_ok=theme_set_current(&theme,theme_current_dir);
    if(!theme_path_ok){theme_init(&theme,1,NULL);observer_emit(&observer,"warning","theme_path_unavailable","{}",0);}
    int theme_result=theme_poll(&theme,monotonic_seconds());apply_palette(theme.palette);
    if(theme_result&THEME_EVENT)theme_event(&observer,&theme);
    stats.theme_ms=(monotonic_seconds()-init_step)*1000;
    SDL_Window *win=NULL;SDL_Renderer *r=NULL;SDL_Texture *trail=NULL;int code=1;
    Point *generated=NULL,*lab_scratch=NULL;
    lab_init();library_init(&library,NULL);aquarium_init();
    observer_emit(&observer,library.blocked?"warning":"info",library.blocked?"library_load_failed":"library_ready","{}",0);
    init_step=monotonic_seconds();
    error_stage="sdl_ttf_init";
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0||TTF_Init()<0)goto error;
    stats.sdl_ms=(monotonic_seconds()-init_step)*1000;init_step=monotonic_seconds();
    const char *paths[]={getenv("PHOSPHOR_FONT"),"/usr/share/fonts/TTF/DejaVuSansMono.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf","/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf","/usr/share/fonts/liberation/LiberationMono-Regular.ttf"};
    for(int i=0;i<5&&!font;i++)if(paths[i])font=TTF_OpenFont(paths[i],12);
    if(!font){fprintf(stderr,"Font unavailable: set PHOSPHOR_FONT to a local monospace TTF\n");observer_emit(&observer,"error","font_unavailable","{}",1);goto done;}
    stats.font_ms=(monotonic_seconds()-init_step)*1000;init_step=monotonic_seconds();
    error_stage="window_create";
    win=SDL_CreateWindow("PHOSPHOR FIELD // LAB-01",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,w,h,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!win)goto error;
    error_stage="renderer_create";
    r=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_TARGETTEXTURE);
    if(!r)r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE|SDL_RENDERER_TARGETTEXTURE);
    if(!r)goto error;
    stats.renderer_ms=(monotonic_seconds()-init_step)*1000;
    SDL_RendererInfo info;if(SDL_GetRendererInfo(r,&info)==0)snprintf(stats.renderer,sizeof stats.renderer,"%s",info.name);
    fprintf(stderr,"renderer=%s\n",stats.renderer);
    char renderer_json[180],renderer_data[220];json_quote(renderer_json,sizeof renderer_json,stats.renderer);
    snprintf(renderer_data,sizeof renderer_data,"{\"name\":%s}",renderer_json);observer_emit(&observer,"info","renderer",renderer_data,0);
    stats.init_ms=(monotonic_seconds()-observer.start)*1000;
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    generated=malloc(MAX_POINTS*sizeof *generated);
    lab_scratch=malloc(MAX_POINTS*sizeof *lab_scratch);
    if(!generated||!lab_scratch){
        fputs("Generator point buffer unavailable\n",stderr);
        observer_emit(&observer,"error","allocation_error","{}",1);goto done;
    }
    SDL_FPoint points[MAX_POINTS];int tw=0,th=0,run=1,dirty=1,drawn=0;
    double start=monotonic_seconds(),last=start,window_start=start;
    double frame_ms=0,sum_ms=0,max_ms=0,fps=0,steady_sum=0,steady_max=0,first_ms=0;
    unsigned steady_frames=0,qa_step=0;
    double display_next=0;
    char status[256]="",samples[256]="",view[256]="",controls[256]="";unsigned frames=0,interval_frames=0;
    int previous_hidden=-1,previous_w=0,previous_h=0,previous_layout=-1;
    while(run&&!stopped){
        double now=monotonic_seconds(),dt=now-last;last=now;
        uint64_t frame_generation=generator_generation;
        if(qa){
            static const SDL_Keycode keys[]={SDLK_h,SDLK_k,SDLK_SPACE,SDLK_g,SDLK_p,SDLK_UP,SDLK_DOWN,SDLK_RIGHT,SDLK_LEFT,SDLK_PLUS,SDLK_MINUS,SDLK_n,SDLK_m,SDLK_LEFTBRACKET,SDLK_RIGHTBRACKET,SDLK_SPACE,SDLK_r};
            if(qa_step<sizeof keys/sizeof keys[0]){SDL_Event test;SDL_zero(test);test.type=SDL_KEYDOWN;test.key.keysym.sym=keys[qa_step];SDL_PushEvent(&test);}
            if(qa_step==5)SDL_SetWindowSize(win,320,240);
            if(qa_step==10)SDL_SetWindowSize(win,760,600);
            qa_step++;
        }
        theme_result=theme_poll(&theme,now);
        if(theme_result&THEME_EVENT)theme_event(&observer,&theme);
        if(theme_result&THEME_COLORS){apply_palette(theme.palette);dirty=1;}
        sample_tick(&observer,&stats,&s,&theme,now);
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_TEXTINPUT&&lab.open){lab_text(e.text.text);dirty=1;}
            if(e.type==SDL_QUIT){aquarium_stop(&s);if(lab_confirm_quit(&s)){run=0;reason="window_close";}else dirty=1;}
            if(e.type==SDL_KEYDOWN){
                if(aquarium_key(&s,&e.key)){dirty=1;continue;}
                if(input_debug)fprintf(stderr,"keycode=%d scancode=%d mod=0x%x repeat=%u\n",e.key.keysym.sym,e.key.keysym.scancode,e.key.keysym.mod,e.key.repeat);
                size_t before_lab_generator=active_generator;double before_lab_phase=s.t;
                if(lab_key(&s,&e.key)){
                    dirty=1;
                    if(before_lab_generator!=active_generator){
                        char data[300];snprintf(data,sizeof data,"{\"from\":\"%s\",\"to\":\"%s\",\"generation\":%llu,\"phase_before\":%.9f,\"phase_after\":0,\"trail_cleared\":true}",generator_at(before_lab_generator)->id,generator_at(active_generator)->id,(unsigned long long)generator_generation,before_lab_phase);
                        observer_emit(&observer,"info","generator_changed",data,1);
                    }
                    if((e.key.keysym.mod&KMOD_CTRL)&&!e.key.repeat&&e.key.keysym.sym==SDLK_s){
                        char message[340],data[400];json_quote(message,sizeof message,lab.status);
                        snprintf(data,sizeof data,"{\"valid\":%s,\"message\":%s}",lab.error.message[0]?"false":"true",message);
                        observer_emit(&observer,"info","lab_apply",data,0);
                    }
                    continue;
                }
                Settings before=s;size_t previous_generator=active_generator;int changed=0;
                if(!key_event(&s,&e.key,&changed)){if(lab_confirm_quit(&s)){run=0;reason="quit_key";}else dirty=1;}
                if(changed){
                    dirty=1;
                    if(previous_generator!=active_generator){
                        char data[300];
                        snprintf(data,sizeof data,"{\"from\":\"%s\",\"to\":\"%s\",\"generation\":%llu,\"phase_before\":%.9f,\"phase_after\":0,\"trail_cleared\":true}",generator_at(previous_generator)->id,generator_at(active_generator)->id,(unsigned long long)generator_generation,before.t);
                        observer_emit(&observer,"info","generator_changed",data,1);
                    }
                    char state[1600];snapshot_json(state,sizeof state,&stats,&s,&theme);
                    const char *event=e.key.keysym.sym==SDLK_r?"reset":before.paused!=s.paused?(s.paused?"pause":"resume"):"parameters";
                    if(previous_generator==active_generator||e.key.keysym.sym==SDLK_r)observer_emit(&observer,"info",event,state,0);
                }
            }
            if(e.type==SDL_WINDOWEVENT)dirty=1;
        }
        if(lab.quit_confirmed){run=0;reason="quit_key";}
        if(!run)break;
        int hidden=(SDL_GetWindowFlags(win)&(SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN))!=0;
        stats.hidden=hidden;
        if(hidden!=previous_hidden){observer_emit(&observer,"info","visibility",hidden?"{\"hidden\":true}":"{\"hidden\":false}",0);previous_hidden=hidden;}
        if(hidden){aquarium.last=now;if(seconds>0&&now-start>=seconds){reason="time_limit";break;}SDL_Delay(100);continue;}
        SDL_GetRendererOutputSize(r,&w,&h);
        stats.w=w;stats.h=h;
        if(w!=previous_w||h!=previous_h){char size[100];snprintf(size,sizeof size,"{\"width_px\":%d,\"height_px\":%d}",w,h);observer_emit(&observer,"info","resize",size,0);previous_w=w;previous_h=h;}
        int ww,wh;SDL_GetWindowSize(win,&ww,&wh);
        int dpi=ww>0?(int)lround((double)w/ww):1;
        if(dpi<1)dpi=1;
        if(dpi>4)dpi=4;
        if(dpi!=ui_scale){text_clear();ui_scale=dpi;error_stage="font_resize";if(TTF_SetFontSize(font,12*dpi)<0)goto error;dirty=1;char dpi_json[60];snprintf(dpi_json,sizeof dpi_json,"{\"scale\":%d}",dpi);observer_emit(&observer,"info","dpi",dpi_json,0);}
        aquarium_tick(&s,now,trail,r,generated);
        if(aquarium.changed){dirty=1;aquarium.changed=0;}
        Layout l=lab.open?lab_layout(w,h):layout(w,h,s);
        if(aquarium.active&&!lab.open&&!l.beacon){l.plot.y+=20*ui_scale;l.plot.h-=20*ui_scale;}
        if(l.plot.h<1)l.plot.h=1;
        int layout_id=l.beacon?0:l.compact?1:2;layout_id+=l.panel?4:0;
        if(layout_id!=previous_layout){char data[80];snprintf(data,sizeof data,"{\"mode\":%d,\"panel_px\":%d}",layout_id,l.panel);observer_emit(&observer,"info","layout",data,0);previous_layout=layout_id;}
        if(l.plot.w!=tw||l.plot.h!=th){
            SDL_DestroyTexture(trail);tw=l.plot.w;th=l.plot.h;
            error_stage="trail_create";
            trail=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,tw,th);
            if(!trail)goto error;
            SDL_SetTextureBlendMode(trail,SDL_BLENDMODE_ADD);dirty=1;
        }
        if(s.paused&&!dirty){if(seconds>0&&now-start>=seconds){
            if(!shot){reason="time_limit";break;}dirty=1;
        }else {SDL_Delay(50);continue;}}
        const Generator *generator=generator_at(active_generator);
        double work_start=monotonic_seconds();
        if(frame_generation==generator_generation&&!(lab.open&&lab.runtime_error)&&!aquarium.failed)advance(&s,fmin(dt,.1));
        double scale=fmin(tw,th)/(2*generator->view.half_span)*s.zoom;
        error_stage="trail_target";if(SDL_SetRenderTarget(r,trail)<0)goto error;
        drawn=0;double math_start=0,submit_start=0,face_start=0,present_start=0;
        if(!frames)math_start=monotonic_seconds();
        int render_count=s.points;
        if(aquarium.active&&aquarium.formula_active){
            if(aquarium.failed||!aquarium_generate(s.points,s.t,generated)){
                aquarium.failed=1;snprintf(aquarium.message,sizeof aquarium.message,"RUNTIME ERROR / skipping slot");
                /* Keep the complete last valid layer. Recovery occurs next tick. */
                render_count=0;
            }
        }else if(lab.open){
            if(lab_generate(s.points,s.t,generated,lab_scratch)<0)
                observer_emit(&observer,"warning","lab_runtime_error","{}",0);
            render_count=lab.preview_count;
        }else generator->generate(s.points,s.t,generated);
        stats.invalid=stats.clipped=0;
        for(int k=0;k<render_count;k++){
            Point p=(lab.open?lab_scratch:generated)[k];
            if(!isfinite(p.x)||!isfinite(p.y)){stats.invalid++;continue;}
            double x=tw/2.+(p.x-generator->view.center_x)*scale,y=th/2.+(p.y-generator->view.center_y)*scale;
            if(!isfinite(x)||!isfinite(y))stats.invalid++;
            else if(x>=0&&y>=0&&x<tw&&y<th)points[drawn++]=(SDL_FPoint){x,y};
            else stats.clipped++;
        }
        stats.calculated=render_count;stats.drawn=drawn;
        stats.calculated_lab=lab.open||(aquarium.active&&aquarium.formula_active);
        observer_lab_active=lab.open||(aquarium.active&&aquarium.formula_active);
        snprintf(observer_creature,sizeof observer_creature,"%s",aquarium.active&&aquarium.slot>=0?library.favorites[aquarium.slot]:*lab.id?lab.id:"lab-preview");
        snprintf(stats.calculated_creature,sizeof stats.calculated_creature,"%s",observer_creature);
        stats.calculated_generator=active_generator;stats.calculated_generation=generator_generation;stats.calculated_phase=lab.open?lab.preview_phase:s.t;
        if(!frames){stats.first_math_ms=(monotonic_seconds()-math_start)*1000;submit_start=monotonic_seconds();}
        if(!(aquarium.active&&aquarium.failed)){
            if(dirty||!s.trail){SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);}
            else {SDL_SetRenderDrawColor(r,0,0,0,(Uint8)fmax(1,255*-expm1(-dt/s.tau)));SDL_RenderFillRect(r,NULL);}
        }
        color(r,signal_color);
        error_stage="points_submit";if(SDL_RenderDrawPointsF(r,points,drawn)<0)goto error;
        if(!frames){stats.first_submit_ms=(monotonic_seconds()-submit_start)*1000;face_start=monotonic_seconds();}
        error_stage="compose";if(SDL_SetRenderTarget(r,NULL)<0)goto error;
        color(r,bg);if(SDL_RenderClear(r)<0)goto error;
        if(s.grid&&!l.beacon)grid(r,l,scale);
        if(!l.beacon){
            double alpha=aquarium_alpha();
            SDL_SetTextureAlphaMod(trail,(Uint8)lround(255*alpha));
            if(SDL_RenderCopy(r,trail,NULL,&l.plot)<0)goto error;
            SDL_SetTextureAlphaMod(trail,255);
            if(aquarium.source&&alpha<1){
                SDL_SetTextureAlphaMod(aquarium.source,(Uint8)lround(255*(1-alpha)));
                if(SDL_RenderCopy(r,aquarium.source,NULL,&l.plot)<0)goto error;
            }
        }
        if(!l.beacon){color(r,dim);SDL_RenderDrawRect(r,&l.plot);}
        if(lab.open){lab_draw(r,w,h,s);}
        else {
        int u=ui_scale;
        char title[128];const char *shown=aquarium_display_name();if(!shown)shown=generator->name;
        if(l.beacon)snprintf(title,sizeof title,"%s / EXPAND",shown);
        else if(!l.panel&&(s.help||s.controls))snprintf(title,sizeof title,"%s / EXPAND FOR HELP",shown);
        else snprintf(title,sizeof title,l.compact?"PHOSPHOR FIELD / %s":"PHOSPHOR FIELD // LAB-01 / %s",shown);
        if(now<lab.notice_until)snprintf(title,sizeof title,"%.120s",lab.status);
        text(r,8*u,7*u,title,fg);
        if(aquarium.active){char aq[180];aquarium_status(aq,sizeof aq);text(r,8*u,27*u,aq,aquarium.failed?warn:dim);}
        int y=h-(l.compact?30:52)*u-l.panel;
        /* Measurement runs each frame; these displayed snapshots refresh at 8 Hz. */
        if(dirty||now>=display_next){
            snprintf(status,sizeof status,l.beacon?"%s %.0f FPS":"%s FPS %.1f/60 %.2f ms",s.paused?"PAUSED":"RUN",s.paused?0:fps,frame_ms);
            snprintf(samples,sizeof samples,"CALC %d  DRAW %d  PHASE %.3f [1]",s.points,drawn,s.t);
            if(l.compact)snprintf(view,sizeof view,"N %d Z %.2f D %.3fs",s.points,s.zoom,s.tau);
            else snprintf(view,sizeof view,"ZOOM %.2f  SCALE %.3f px/u  X Y [1]  H HELP  K CONTROL",s.zoom,scale);
            snprintf(controls,sizeof controls,"SPEED %.2fx P %s G %s",s.speed,s.trail?"ON":"OFF",s.grid?"ON":"OFF");
            display_next=now+.125;
        }
        text(r,8*u,y,status,s.paused?warn:fg);
        if(!l.compact)text(r,8*u,y+17*u,samples,dim);
        if(!l.beacon)text(r,8*u,y+(l.compact?16:34)*u,view,dim);
        if(l.beacon)text(r,8*u,40*u,"STATUS / EXPAND",warn);
        if(l.panel){
            int py=h-l.panel+3*u;color(r,dim);SDL_RenderDrawLine(r,8*u,py-4*u,w-8*u,py-4*u);
            if(s.help){const char *lines[]={"H HELP / K CONTROL / Q ESC QUIT","1-0 FAVORITES / B LIBRARY / L LAB","UP DOWN POINTS","LEFT RIGHT SPEED / + - ZOOM","G GRID / P TRAIL","N SHORTER / M LONGER DECAY","A AQUARIUM / A ESC BACK / SPACE PAUSE","COMMA/PERIOD INTERVAL 5..120s (15s)","FADE 1s + INTERVAL / 1-0 L B R EXIT"};
                for(int i=0;i<9;i++)text(r,8*u,py+i*17*u,lines[i],dim);
            }else{
                char decay[128];snprintf(decay,sizeof decay,"N/M DECAY %.3f s",s.tau);
                text(r,8*u,py,controls,fg);text(r,8*u,py+17*u,decay,warn);
            }
        }
        }
        if(shot&&seconds>0&&now-start>=seconds&&!screenshot(r,shot,w,h))observer_emit(&observer,"warning","screenshot_error","{}",0);
        if(!frames){stats.first_face_ms=(monotonic_seconds()-face_start)*1000;present_start=monotonic_seconds();}
        SDL_RenderPresent(r);dirty=0;
        if(!frames)stats.first_present_ms=(monotonic_seconds()-present_start)*1000;
        double end=monotonic_seconds();frame_ms=(end-work_start)*1000;
        work_add(&stats.all,frame_ms);work_add(&stats.interval,frame_ms);
        if(frames){if(now-start>=.5)work_add(&stats.steady,frame_ms);else work_add(&stats.warm,frame_ms);}
        if(!frames){
            first_ms=frame_ms;stats.first_ms=frame_ms;
            char ready[1600];snapshot_json(ready,sizeof ready,&stats,&s,&theme);observer_emit(&observer,"info","ready",ready,1);
            fprintf(stderr,"phosphor-field " PHOSPHOR_VERSION " ready / %s\n",theme.name);
            /* Periodic samples start after the separately reported first successful frame. */
            observer.sample_start=monotonic_seconds();observer.cpu_start=process_cpu_seconds();observer.next_sample=observer.sample_start+observer.interval;stats.interval=(Work){0};
        }
        if(now-start>=.5){steady_sum+=frame_ms;steady_frames++;if(frame_ms>steady_max)steady_max=frame_ms;}
        frames++;interval_frames++;sum_ms+=frame_ms;if(frame_ms>max_ms)max_ms=frame_ms;
        if(now-window_start>=.5){fps=interval_frames/(now-window_start);interval_frames=0;window_start=now;}
        if(seconds>0&&now-start>=seconds){reason="time_limit";break;}
        double wait=1./60-(end-now);if(wait>0)SDL_Delay((Uint32)ceil(wait*1000));
    }
    {double elapsed=monotonic_seconds()-start;stats.elapsed_s=elapsed;
    if(!status_json)printf("size=%dx%d points=%d drawn=%d frames=%u elapsed=%.3f fps=%.2f work_mean_ms=%.3f work_max_ms=%.3f phase=%.6f first_ms=%.3f steady_mean_ms=%.3f steady_max_ms=%.3f text_builds=%llu text_live=%d\n",w,h,s.points,drawn,frames,elapsed,frames/elapsed,frames?sum_ms/frames:0,max_ms,s.t,first_ms,steady_frames?steady_sum/steady_frames:0,steady_max,(unsigned long long)text_builds,text_live());}
    code=0;if(stopped)reason="signal";goto done;
error:{char stage[100],data[140];json_quote(stage,sizeof stage,error_stage);snprintf(data,sizeof data,"{\"stage\":%s}",stage);observer_emit(&observer,"error","sdl_error",data,1);}
    fprintf(stderr,"SDL: %s / TTF: %s\n",SDL_GetError(),TTF_GetError());
done:
    if(!strcmp(reason,"error")&&stopped&&code==0)reason="signal";
    free(generated);free(lab_scratch);SDL_StopTextInput();
    aquarium_destroy();text_clear();SDL_DestroyTexture(trail);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);if(font)TTF_CloseFont(font);font=NULL;TTF_Quit();SDL_Quit();
    exit_event(&observer,&stats,&s,&theme,code,reason);log_close(&observer.log);return code;
invalid:fputs("Invalid or incomplete argument (see --help)\n",stderr);return 1;
}
