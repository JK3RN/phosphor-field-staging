#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#define main instrument_main
#include "../src/main.c"
#undef main
int main(void){
    Settings s=defaults();s.points=2500;s.zoom=4;s.speed=.25;s.tau=.6;s.paused=1;s.help=1;s.grid=0;s.trail=0;
    active_generator=0;generator_generation=0;
    SDL_KeyboardEvent e={.type=SDL_KEYDOWN};int dirty=0;
    /* One native creature: 1 is a no-op, 2..9 address no native index and
       must neither switch, reset phase nor touch other settings. */
    for(int slot=1;slot<=9;slot++){
        s.t=123;e.keysym.sym=SDLK_1+slot-1;dirty=0;
        assert(key_event(&s,&e,&dirty)&&!dirty);
        assert(active_generator==0&&generator_generation==0&&s.t==123);
        assert(s.points==2500&&s.zoom==4&&s.speed==.25&&s.tau==.6&&s.paused&&s.help&&!s.grid&&!s.trail);
    }
    select_generator(&s,NATIVE_COUNT);select_generator(&s,(size_t)-1);
    assert(active_generator==0&&generator_generation==0&&s.t==123);
    for(int mode=0;mode<3;mode++){
        dirty=0;e.keysym.sym=SDLK_2;e.repeat=mode==0;e.type=mode==1?SDL_KEYUP:SDL_KEYDOWN;e.keysym.mod=mode==2?KMOD_SHIFT:0;
        key_event(&s,&e,&dirty);assert(!dirty&&active_generator==0);
    }
    e.repeat=0;e.type=SDL_KEYDOWN;e.keysym.mod=0;e.keysym.sym=SDLK_0;dirty=0;
    key_event(&s,&e,&dirty);assert(!dirty&&active_generator==0);
    e.keysym.mod=KMOD_RALT|KMOD_LCTRL;e.keysym.sym=SDLK_9;e.keysym.scancode=SDL_SCANCODE_9;
    double old=s.tau;key_event(&s,&e,&dirty);assert(active_generator==0&&s.tau==old);
    s.tau=.14;e.keysym.sym=SDLK_8;e.keysym.scancode=SDL_SCANCODE_8;dirty=0;
    key_event(&s,&e,&dirty);assert(dirty&&active_generator==0&&fabs(s.tau-.1)<1e-12);
    e.keysym.sym=SDLK_9;e.keysym.scancode=SDL_SCANCODE_9;dirty=0;
    key_event(&s,&e,&dirty);assert(dirty&&active_generator==0&&fabs(s.tau-.14)<1e-12);
    e.keysym.mod=0;e.keysym.sym=SDLK_r;dirty=0;key_event(&s,&e,&dirty);
    Settings def=defaults();assert(dirty&&active_generator==0&&generator_generation==0);
    assert(s.points==def.points&&s.zoom==def.zoom&&s.speed==def.speed&&s.tau==def.tau&&s.t==0&&!s.paused&&!s.help&&!s.controls&&s.grid&&s.trail);
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO)==0&&TTF_Init()==0);
    const char *fonts[]={"/usr/share/fonts/TTF/DejaVuSansMono.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf","/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf","/usr/share/fonts/liberation/LiberationMono-Regular.ttf"};
    for(int i=0;i<4&&!font;i++)font=TTF_OpenFont(fonts[i],12);
    assert(font);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,320,240,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
    SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface);assert(r);text(r,1,1,"TEST",fg);assert(text_live()==1);
    /* No-op selections keep the text cache. */
    e.keysym.sym=SDLK_1;dirty=0;key_event(&s,&e,&dirty);assert(!dirty&&text_live()==1);
    e.keysym.sym=SDLK_2;dirty=0;key_event(&s,&e,&dirty);assert(!dirty&&text_live()==1);
    RunStats st={.calculated=10000,.drawn=10000,.calculated_generator=0,.calculated_generation=0};Theme theme={0};char state[1600];
    snapshot_json(state,sizeof state,&st,&s,&theme);
    assert(strstr(state,"\"generator\":\"pf-medusa\""));assert(strstr(state,"\"calculated_generator\":\"pf-medusa\""));
    text_clear();SDL_DestroyRenderer(r);SDL_FreeSurface(surface);TTF_CloseFont(font);font=NULL;TTF_Quit();SDL_Quit();
    puts("PASS: slots 1-0 with single native, out-of-range select, 0/repeat/KEYUP/modifiers, AltGr, R defaults, cache no-op, snapshot identity");
}
