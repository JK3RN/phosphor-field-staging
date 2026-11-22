#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#define main instrument_main
#include "../src/main.c"
#undef main
int main(void){
    Settings s=defaults();advance(&s,1./60);assert(fabs(s.t-3.141592653589793/80)<1e-12);
    s.paused=1;double old=s.t;advance(&s,10);assert(s.t==old);
    assert(input(&s,SDLK_r));assert(s.points==10000&&s.t==0&&!s.paused);
    input(&s,SDLK_p);assert(!s.trail);input(&s,SDLK_g);assert(!s.grid);
    for(int i=0;i<50;i++){input(&s,SDLK_UP);}
    assert(s.points==50000);
    for(int i=0;i<50;i++){input(&s,SDLK_DOWN);}
    assert(s.points==2500);
    input(&s,SDLK_h);assert(s.help&&!s.controls);input(&s,SDLK_k);assert(s.controls&&!s.help);
    Layout l=layout(320,240,s);assert(l.plot.h>0&&l.plot.w>0&&l.compact);
    assert(!input(&s,SDLK_q)&&!input(&s,SDLK_ESCAPE));
    input(&s,SDLK_SPACE);assert(s.paused);old=s.t;advance(&s,1);assert(s.t==old);
    input(&s,SDLK_SPACE);advance(&s,1./60);assert(s.t>old);
    for(int i=0;i<50;i++){input(&s,SDLK_n);input(&s,SDLK_LEFT);input(&s,SDLK_MINUS);}
    assert(s.tau==.035&&s.speed==.25&&s.zoom==.25);
    for(int i=0;i<50;i++){input(&s,SDLK_m);input(&s,SDLK_RIGHT);input(&s,SDLK_PLUS);}
    assert(s.tau==.6&&s.speed==4&&s.zoom==4);
    s=defaults();int dirty=0;
    SDL_KeyboardEvent e={.type=SDL_KEYDOWN};e.keysym.sym=SDLK_n;
    assert(key_event(&s,&e,&dirty));assert(dirty&&fabs(s.tau-.1)<1e-12);
    old=s.tau;dirty=0;e.repeat=1;key_event(&s,&e,&dirty);assert(s.tau==old&&!dirty);
    e.repeat=0;e.type=SDL_KEYUP;key_event(&s,&e,&dirty);assert(s.tau==old&&!dirty);
    e.type=SDL_KEYDOWN;e.keysym.sym=SDLK_9;e.keysym.scancode=SDL_SCANCODE_9;e.keysym.mod=KMOD_RALT|KMOD_LCTRL;
    key_event(&s,&e,&dirty);assert(fabs(s.tau-.14)<1e-12);
    e.keysym.sym=SDLK_LEFTBRACKET;e.keysym.scancode=SDL_SCANCODE_8;e.keysym.mod=KMOD_MODE;
    key_event(&s,&e,&dirty);assert(fabs(s.tau-.1)<1e-12); /* exactly once, not twice */
    e.keysym.mod=0;e.keysym.sym=SDLK_x;e.keysym.scancode=SDL_SCANCODE_X;dirty=0;
    key_event(&s,&e,&dirty);assert(!dirty&&s.tau==old);
    e.keysym.sym=SDLK_y;key_event(&s,&e,&dirty);assert(!dirty);
    int sizes[][2]={{320,240},{400,400},{760,600},{280,600},{640,220},{160,120}};
    for(int dpi=1;dpi<=2;dpi++){
        ui_scale=dpi;
        for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++)for(int panel=0;panel<3;panel++){
            s=defaults();s.help=panel==1;s.controls=panel==2;
            l=layout(sizes[i][0]*dpi,sizes[i][1]*dpi,s);
            assert(l.plot.w>0&&l.plot.h>0);
            assert(l.plot.y+l.plot.h+25*dpi<=sizes[i][1]*dpi-l.panel);
        }
    }
    ui_scale=1;
    assert(SDL_setenv("SDL_VIDEODRIVER","dummy",1)==0);
    assert(SDL_Init(SDL_INIT_VIDEO)==0&&TTF_Init()==0);
    const char *paths[]={"/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf","/usr/share/fonts/TTF/DejaVuSansMono.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf","/usr/share/fonts/liberation/LiberationMono-Regular.ttf"};
    const char *custom=getenv("PHOSPHOR_FONT");if(custom)font=TTF_OpenFont(custom,12);
    for(unsigned i=0;i<sizeof paths/sizeof paths[0]&&!font;i++)font=TTF_OpenFont(paths[i],12);
    assert(font);
    SDL_Window *win=SDL_CreateWindow("cache test",0,0,320,240,0);assert(win);
    SDL_Renderer *r=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);assert(r);
    text(r,1,1,"STATIC",fg);old=(double)text_builds;
    for(int i=0;i<120;i++)text(r,1,1,"STATIC",fg);
    assert(text_builds==(Uint64)old&&text_live()==1);
    for(int i=0;i<256;i++){char label[32];snprintf(label,sizeof label,"DYNAMIC %d",i);text(r,1,1,label,fg);}
    assert(text_live()==TEXT_CAPACITY);
    Palette alternate;assert(palette_parse("background = \"#020905\"\nforeground = \"#97dca4\"\naccent = \"#81a1c1\"\n",&alternate));
    apply_palette(alternate);assert(text_live()==0);text(r,1,1,"STATIC",fg);assert(text_live()==1);
    apply_palette(palette_fallback());assert(text_live()==0);
    for(int cycle=0;cycle<20;cycle++){
        SDL_SetWindowSize(win,cycle%2?400:320,cycle%2?400:240);
        text_clear();assert(text_live()==0);
        assert(TTF_SetFontSize(font,cycle%2?24:12)==0);
        text(r,1,1,"HELP",fg);text(r,1,20,"CONTROL",warn);assert(text_live()==2);
    }
    text_clear();assert(text_live()==0);
    SDL_DestroyRenderer(r);SDL_DestroyWindow(win);TTF_CloseFont(font);font=NULL;TTF_Quit();SDL_Quit();
    puts("PASS: events, repeat, AltGr, N/M, all parameter bounds, pause/resume, X/Y, DPI layouts, cache reuse/eviction/resize/clear; input bounds, reset, panels, exit; phase rate, frozen pause");
}
