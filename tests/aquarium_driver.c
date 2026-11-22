/* Test-only SDL event injection; never linked into the product. */
#include <SDL.h>
#include <assert.h>
static int test_poll(SDL_Event *event);
static SDL_Window *test_window_handle;
static SDL_Window *test_window(const char *,int,int,int,int,Uint32);
#define SDL_CreateWindow test_window
#define SDL_PollEvent test_poll
#define main application_main
#include "../src/main.c"
#undef main
#undef SDL_PollEvent
#undef SDL_CreateWindow
static SDL_Window *test_window(const char *title,int x,int y,int w,int h,Uint32 flags){return test_window_handle=SDL_CreateWindow(title,x,y,w,h,flags);}
static int test_poll(SDL_Event *event){
    if(SDL_PollEvent(event))return 1;
    static double start;static unsigned step;
    if(!start)start=monotonic_seconds();
    double elapsed=monotonic_seconds()-start;
    if(!step){
        memset(library.favorites,0,sizeof library.favorites);
        strcpy(library.favorites[0],generator_at(0)->id);
        strcpy(library.favorites[4],generator_at(0)->id);
        library.count=1;strcpy(library.entries[0].id,"c-00000000000000000000000000000001");
        strcpy(library.entries[0].formula,"x=120*cos(3*u+t)*cos(u)\ny=120*sin(3*u+t)*sin(u)");
        strcpy(library.favorites[9],library.entries[0].id);
        aquarium.interval=5;
        SDL_zero(*event);event->type=SDL_KEYDOWN;event->key.keysym.sym=SDLK_a;step++;return 1;
    }
    static const double at[]={0,2,2.3,2.6,2.9,13.3};
    if(step<6&&elapsed>=at[step]){
        SDL_zero(*event);
        if(step==3||step==4){
            SDL_Window *window=test_window_handle;assert(window);
            SDL_SetWindowSize(window,step==3?1100:1920,step==3?640:1080);
            event->type=SDL_WINDOWEVENT;event->window.event=SDL_WINDOWEVENT_EXPOSED;
        }else {event->type=SDL_KEYDOWN;event->key.keysym.sym=step==5?SDLK_a:SDLK_SPACE;}
        if(step==5){assert(aquarium.slot==9&&aquarium.formula_active);assert(!aquarium.rejected);}
        step++;return 1;
    }return 0;
}
int main(int argc,char **argv){return application_main(argc,argv);}
