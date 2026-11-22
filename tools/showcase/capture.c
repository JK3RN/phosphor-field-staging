/* Showcase capture driver — reproduces docs/media from the real render path.
   Not part of the product. It compiles src/main.c unchanged, replaces only
   three things and is driven by tools/showcase/capture.sh:
     - SDL_PollEvent: injects a scripted key sequence (SHOWCASE_SCENE);
     - the monotonic clock: virtual time advancing exactly 1/fps per frame,
       so captures are deterministic and independent of machine load;
     - SDL_RenderPresent: streams each frame as raw RGB to ffmpeg.
   env: SHOWCASE_SCENE=hero|tour|lab|library  SHOWCASE_FFMPEG_OUT=<ffmpeg output args>
        SHOWCASE_FPS (default 30)  SHOWCASE_SKIP (frames, default 0)
   The library lives in XDG_DATA_HOME, which capture.sh points at a temp dir. */
#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int showcase_poll(SDL_Event *event);
static int showcase_present(SDL_Renderer *r);
#define SDL_PollEvent showcase_poll
#define SDL_RenderPresent showcase_present
#define main application_main
#include "../../src/main.c"
#undef main
#undef SDL_RenderPresent
#undef SDL_PollEvent

static double virtual_time=1000,virtual_start=1000;
static unsigned long frame_index;
static FILE *pipe_out;
static double fps(void){const char *v=getenv("SHOWCASE_FPS");return v?atof(v):30;}

/* observe.c is compiled with -Dclock_gettime=showcase_clock_gettime. */
int showcase_clock_gettime(clockid_t id,struct timespec *t){
    if(id!=CLOCK_MONOTONIC)return clock_gettime(id,t);
    t->tv_sec=(time_t)virtual_time;t->tv_nsec=(long)((virtual_time-(double)t->tv_sec)*1e9);return 0;
}

/* Two small formula creatures written for this showcase (original work). */
static const char orbit_weave[]=
    "param petals = 5 [1,12,1]\nparam drift = 0.6 [0,3,0.05]\n"
    "x = 150 * sin(petals * u + t * drift) * cos(u + t / 3)\n"
    "y = 150 * cos((petals + 1) * u - t) * sin(u * 2 + t / 5)";
static const char tide_ring[]=
    "param rings = 7 [1,20,1]\n"
    "x = (110 + 40 * sin(rings * u + t)) * cos(u)\n"
    "y = (110 + 40 * sin(rings * u + t)) * sin(u) * 0.8 + 20 * sin(3 * u - t)";
static void seed_library(int tour_order){
    library.count=2;
    strcpy(library.entries[0].id,"c-5e0f1a2b3c4d5e6f708192a3b4c5d6e7");
    strcpy(library.entries[0].name,"Orbit Weave");strcpy(library.entries[0].formula,orbit_weave);
    strcpy(library.entries[1].id,"c-7a6b5c4d3e2f10f9e8d7c6b5a4938271");
    strcpy(library.entries[1].name,"Tide Ring");strcpy(library.entries[1].formula,tide_ring);
    memset(library.favorites,0,sizeof library.favorites);
    /* Library view: Medusa on 1. Tour: Aquarium opens with a fade to a formula. */
    strcpy(library.favorites[tour_order?2:0],generator_at(0)->id);
    strcpy(library.favorites[tour_order?0:1],library.entries[0].id);
    strcpy(library.favorites[tour_order?1:2],library.entries[1].id);
}

typedef struct { double at; SDL_Keycode key; Uint16 mod; int action; } Step;
enum { KEY, SEED, SEED_TOUR, AQUARIUM_5S };
static const Step tour[]={
    {0.0,0,0,SEED_TOUR},
    {2.5,SDLK_l,0,KEY},{3.2,SDLK_TAB,0,KEY},{3.5,SDLK_DOWN,0,KEY},{3.7,SDLK_DOWN,0,KEY},
    {4.2,SDLK_RIGHT,0,KEY},{4.3,SDLK_s,KMOD_LCTRL,KEY},
    {5.0,SDLK_RIGHT,0,KEY},{5.1,SDLK_s,KMOD_LCTRL,KEY},
    {5.8,SDLK_RIGHT,0,KEY},{5.9,SDLK_s,KMOD_LCTRL,KEY},
    {6.6,SDLK_RIGHT,0,KEY},{6.7,SDLK_s,KMOD_LCTRL,KEY},
    {8.0,SDLK_ESCAPE,0,KEY},{8.2,SDLK_ESCAPE,0,KEY},
    {8.5,0,0,AQUARIUM_5S},{8.6,SDLK_a,0,KEY},
};
static const Step lab_scene[]={
    {0.5,SDLK_l,0,KEY},{1.0,SDLK_TAB,0,KEY},{1.2,SDLK_DOWN,0,KEY},{1.4,SDLK_DOWN,0,KEY},
    {1.6,SDLK_RIGHT,0,KEY},{1.8,SDLK_RIGHT,0,KEY},{2.0,SDLK_s,KMOD_LCTRL,KEY},
};
static const Step library_scene[]={{0.0,0,0,SEED},{0.5,SDLK_b,0,KEY},{1.0,SDLK_DOWN,0,KEY}};

static int showcase_poll(SDL_Event *event){
    if(SDL_PollEvent(event))return 1;
    const char *scene=getenv("SHOWCASE_SCENE");const Step *steps=NULL;size_t count=0;
    if(scene&&!strcmp(scene,"tour")){steps=tour;count=sizeof tour/sizeof *tour;}
    else if(scene&&!strcmp(scene,"lab")){steps=lab_scene;count=sizeof lab_scene/sizeof *lab_scene;}
    else if(scene&&!strcmp(scene,"library")){steps=library_scene;count=sizeof library_scene/sizeof *library_scene;}
    static size_t next;
    double elapsed=virtual_time-virtual_start;
    while(next<count&&elapsed>=steps[next].at){
        const Step *s=&steps[next++];
        if(s->action==SEED||s->action==SEED_TOUR){seed_library(s->action==SEED_TOUR);continue;}
        if(s->action==AQUARIUM_5S){aquarium.interval=5;continue;}
        SDL_zero(*event);event->type=SDL_KEYDOWN;event->key.keysym.sym=s->key;event->key.keysym.mod=s->mod;
        SDL_SetModState((SDL_Keymod)s->mod);
        return 1;
    }
    return 0;
}
static int showcase_present(SDL_Renderer *r){
    int w,h;SDL_GetRendererOutputSize(r,&w,&h);
    const char *skip_text=getenv("SHOWCASE_SKIP");unsigned long skip=skip_text?strtoul(skip_text,NULL,10):0;
    if(frame_index>=skip){
        if(!pipe_out){
            const char *out=getenv("SHOWCASE_FFMPEG_OUT");if(!out){fputs("SHOWCASE_FFMPEG_OUT unset\n",stderr);exit(2);}
            char cmd[2048];snprintf(cmd,sizeof cmd,"ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %g -i - %s",w,h,fps(),out);
            pipe_out=popen(cmd,"w");if(!pipe_out){perror("ffmpeg");exit(2);}
        }
        static unsigned char *buffer;static size_t size;
        if(size!=(size_t)w*h*3){free(buffer);size=(size_t)w*h*3;buffer=malloc(size);if(!buffer)exit(2);}
        if(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGB24,buffer,w*3)){fputs(SDL_GetError(),stderr);exit(2);}
        fwrite(buffer,1,size,pipe_out);
    }
    frame_index++;virtual_time+=1/fps();
    SDL_RenderPresent(r);return 0;
}
int main(int argc,char **argv){
    int code=application_main(argc,argv);
    if(pipe_out&&pclose(pipe_out)!=0)code=code?code:3;
    return code;
}
