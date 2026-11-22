#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#define main instrument_main
#include "../src/main.c"
#undef main
static SDL_KeyboardEvent key(SDL_Keycode k){return (SDL_KeyboardEvent){.type=SDL_KEYDOWN,.keysym={.sym=k}};}
static void press(Settings *s,SDL_Keycode k){SDL_KeyboardEvent e=key(k);assert(aquarium_key(s,&e));}
int main(void){
    char directory[]="/tmp/phosphor-aquarium-XXXXXX";assert(mkdtemp(directory));
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO));
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,320,240,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
    SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface);assert(r);
    SDL_Texture *trail=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,320,240);assert(trail);SDL_SetTextureBlendMode(trail,SDL_BLENDMODE_ADD);
    SDL_SetRenderTarget(r,trail);SDL_SetRenderDrawColor(r,100,80,60,255);SDL_RenderClear(r);SDL_SetRenderTarget(r,NULL);
    Point scratch[MAX_POINTS];Settings s=defaults();s.t=17;s.zoom=1.7;s.paused=1;active_generator=0;lab_init();library_init(&library,directory);aquarium_init();
    Settings saved=s;
    memset(library.favorites,0,sizeof library.favorites);press(&s,SDLK_a);aquarium_tick(&s,10,trail,r,scratch);assert(!aquarium_count()&&strstr(aquarium.message,"NO FAVORITES"));press(&s,SDLK_ESCAPE);assert(!memcmp(&saved,&s,sizeof s)&&active_generator==0);
    strcpy(library.favorites[9],generator_at(0)->id);press(&s,SDLK_a);aquarium.last=10;aquarium_tick(&s,10,trail,r,scratch);assert(aquarium.slot==9&&active_generator==0&&!aquarium.formula_active);s.paused=0;aquarium_tick(&s,100,trail,r,scratch);assert(aquarium.slot==9);press(&s,SDLK_a);
    strcpy(library.favorites[0],generator_at(0)->id);strcpy(library.favorites[4],generator_at(0)->id);
    press(&s,SDLK_a);s.paused=0;aquarium.last=0;aquarium_tick(&s,0,trail,r,scratch);assert(aquarium.slot==0&&aquarium.source);
    aquarium_tick(&s,.5,trail,r,scratch);assert(fabs(aquarium_alpha()-.5)<1e-9);
    /* Source is a frozen rendered layer; weighted copies retain light at midpoint. */
    SDL_SetRenderTarget(r,trail);SDL_SetRenderDrawColor(r,20,120,40,255);SDL_RenderClear(r);SDL_SetRenderTarget(r,NULL);
    for(int j=0;j<3;j++){
        aquarium.fade=j*.5;double a=aquarium_alpha();SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
        SDL_SetTextureAlphaMod(trail,(Uint8)lround(255*a));SDL_RenderCopy(r,trail,NULL,NULL);
        SDL_SetTextureAlphaMod(aquarium.source,(Uint8)lround(255*(1-a)));SDL_RenderCopy(r,aquarium.source,NULL,NULL);
        Uint32 pixel;SDL_Rect rect={10,10,1,1};assert(!SDL_RenderReadPixels(r,&rect,SDL_PIXELFORMAT_ARGB8888,&pixel,4));
        SDL_PixelFormat *format=SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);Uint8 red,green,blue,alpha;SDL_GetRGBA(pixel,format,&red,&green,&blue,&alpha);SDL_FreeFormat(format);
        assert(abs(red-(int)lround(100*(1-a)+20*a))<=2&&green>=79&&blue>=39);
    }
    SDL_SetTextureAlphaMod(trail,255);aquarium.fade=.5;

    aquarium_tick(&s,1,trail,r,scratch);assert(aquarium_alpha()==1&&!aquarium.source);
    aquarium_tick(&s,16,trail,r,scratch);assert(aquarium.slot==4);aquarium_tick(&s,17,trail,r,scratch);aquarium_tick(&s,32,trail,r,scratch);assert(aquarium.slot==9);
    s.paused=1;double remaining=aquarium.remaining,fade=aquarium.fade;aquarium_tick(&s,100,trail,r,scratch);assert(aquarium.remaining==remaining&&aquarium.fade==fade);
    /* Minimized time is excluded by eventloop resetting last before tick. */
    aquarium.last=1000;aquarium_tick(&s,1000,trail,r,scratch);assert(aquarium.fade==fade);
    aquarium.interval=120;press(&s,SDLK_PERIOD);assert(aquarium.interval==120);aquarium.interval=5;press(&s,SDLK_COMMA);assert(aquarium.interval==5);
    press(&s,SDLK_PERIOD);assert(aquarium.interval==6);press(&s,SDLK_ESCAPE);aquarium_init();assert(aquarium.interval==6);
    /* Text focus, repeat and modified keys never activate. */
    SDL_KeyboardEvent e=key(SDLK_a);lab.open=1;assert(!aquarium_key(&s,&e));lab.open=0;lab.dialog=1;assert(!aquarium_key(&s,&e));lab.dialog=0;e.repeat=1;assert(!aquarium_key(&s,&e));e.repeat=0;e.keysym.mod=KMOD_CTRL;assert(!aquarium_key(&s,&e));
    for(int j=0;j<4;j++){SDL_Keycode keys[]={SDLK_l,SDLK_b,SDLK_r,SDLK_0};press(&s,SDLK_a);e=key(keys[j]);assert(!aquarium_key(&s,&e)&&!aquarium.active&&!memcmp(&saved,&s,sizeof s)&&active_generator==0);}
    press(&s,SDLK_a);aquarium.fade=.2;press(&s,SDLK_ESCAPE);assert(aquarium_alpha()==1);
    press(&s,SDLK_a);e=key(SDLK_l);e.keysym.mod=KMOD_SHIFT;assert(!aquarium_key(&s,&e)&&!aquarium.active);
    /* Aquarium formulas use the same frame as the lab: (0,0) is the plot centre. */
    {Formula centre;FormulaError fe;assert(formula_compile("x = 0\ny = 0",&centre,&fe));aquarium.formula=centre;
     Point p[3];assert(aquarium_generate(3,0,p));const GeneratorView *v=&generator_at(0)->view;
     for(int k=0;k<3;k++)assert(p[k].x==v->center_x&&p[k].y==v->center_y);}
    /* Mixed native/formula order; malformed and runtime-invalid formulas skip. */
    memset(library.favorites,0,sizeof library.favorites);library.count=2;
    strcpy(library.entries[0].id,"c-00000000000000000000000000000001");strcpy(library.entries[0].name,"Ring");strcpy(library.entries[0].formula,"x=100*cos(u)\ny=100*sin(u)");
    strcpy(library.entries[1].id,"c-00000000000000000000000000000002");strcpy(library.entries[1].formula,"x=1/(1-t)\ny=0");
    strcpy(library.favorites[0],library.entries[0].id);strcpy(library.favorites[1],library.entries[1].id);strcpy(library.favorites[9],generator_at(0)->id);
    press(&s,SDLK_a);s.paused=0;aquarium.last=0;aquarium_tick(&s,0,trail,r,scratch);assert(aquarium.slot==0&&aquarium.formula_active&&aquarium_generate(10000,4,scratch));assert(!strcmp(aquarium_display_name(),"Ring"));
    aquarium_tick(&s,1,trail,r,scratch);aquarium_tick(&s,7,trail,r,scratch);assert(aquarium.slot==1);assert(!aquarium_generate(10000,1,scratch));aquarium.failed=1;aquarium_tick(&s,7.1,trail,r,scratch);assert(aquarium.slot==9&&!aquarium.formula_active&&(aquarium.rejected&2));assert(!aquarium_display_name());press(&s,SDLK_a);
    strcpy(library.entries[0].formula,"x=sin(");strcpy(library.entries[1].formula,"x=1/0\ny=0");press(&s,SDLK_a);s.paused=0;aquarium.last=0;aquarium_tick(&s,0,trail,r,scratch);assert(aquarium.slot==9&&(aquarium.rejected&3)==3);press(&s,SDLK_a);
    *library.favorites[9]=0;press(&s,SDLK_a);aquarium.last=0;aquarium_tick(&s,0,trail,r,scratch);assert(!aquarium_count()&&aquarium.failed);aquarium_tick(&s,999,trail,r,scratch);assert(strstr(aquarium.message,"NO VALID"));press(&s,SDLK_a);
    /* Invalid/unknown settings remain untouched, interval is explicitly volatile. */
    char path[4200];snprintf(path,sizeof path,"%s/settings.pfs",directory);FILE *f=fopen(path,"w");assert(f);fputs("PHOSPHOR_SETTINGS 99\ninterval 10\n",f);fclose(f);aquarium_init();assert(aquarium.interval==15&&aquarium.settings_blocked&&!aquarium_save());
    unlink(path);rmdir(directory);aquarium_destroy();SDL_DestroyTexture(trail);SDL_DestroyRenderer(r);SDL_FreeSurface(surface);SDL_Quit();
    puts("PASS: empty/single/mixed slots incl 0, easing/time/pause, interval persistence/bounds, text focus, restore/exits, parse/runtime recovery");
}
