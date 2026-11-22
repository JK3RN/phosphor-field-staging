#define main instrument_main
#include "../src/main.c"
#undef main
#include <assert.h>
static void set_formula(const char *body){snprintf(lab.draft,sizeof lab.draft,"radius = 120\ntwist = 0.8\nwaves = 3\n%s",body);}
static void press(Settings *s,SDL_Keycode key,int mods){SDL_KeyboardEvent e={.type=SDL_KEYDOWN,.keysym={.sym=key,.mod=mods}};assert(lab_key(s,&e));}
int main(void){
    Settings s=defaults();lab_init();assert(!lab.error.message[0]);
    Formula initial=lab.formula;FormulaError e;double x,y;
    assert(formula_eval(&initial,25,100,0,&x,&y,&e));assert(fabs(x)<1e-10&&fabs(y+138)<1e-10);
    set_formula("x = 2 + 3 * 4 - -2\ny = abs(-5) / (1 + 1)");assert(lab_apply(&s));
    assert(formula_eval(&lab.formula,0,1,0,&x,&y,&e)&&x==16&&y==2.5);
    Formula saved=lab.formula;
    const char *bad[]={"x = sin(\ny = 0","x = tan(u)\ny = 0","x = 1/0\ny = 0","x = 1e308 * 1e308\ny = 0","x = 0\ny = 1 / (i - 7777)","x = sin u\ny = 0","x = 1\ny = 0 garbage","x = nan\ny = 0"};
    for(size_t j=0;j<sizeof bad/sizeof bad[0];j++){set_formula(bad[j]);assert(!lab_apply(&s));assert(lab.error.message[0]);assert(!memcmp(&saved,&lab.formula,sizeof saved));}
    set_formula("x = 1 / (t - 1)\ny = u");assert(lab_apply(&s));assert(!formula_eval(&lab.formula,0,100,1,&x,&y,&e));assert(!strcmp(e.message,"division by zero"));
    Point scratch[100],output[100],before[100];
    assert(lab_generate(100,0,scratch,output)==1);memcpy(before,output,sizeof before);
    assert(lab_generate(100,1,scratch,output)==-1&&lab.runtime_error);
    assert(!memcmp(before,output,sizeof before)&&lab.preview_count==100&&lab.preview_phase==0);
    assert(lab_generate(100,2,scratch,output)==0&&!memcmp(before,output,sizeof before));
    strcpy(lab.draft,lab_starter);assert(lab_apply(&s)&&!lab.runtime_error);
    char large[FORMULA_TEXT+1];memset(large,'1',sizeof large-1);large[sizeof large-1]=0;Formula f;assert(!formula_compile(large,&f,&e));
    set_formula("x = 0\ny = 0");int len=(int)strlen(lab.draft);for(int j=0;j<200;j++)strcat(lab.draft,"+1");assert(!lab_apply(&s));assert(strstr(lab.error.message,"budget"));lab.draft[len]=0;
    set_formula("x = ");for(int j=0;j<40;j++)strcat(lab.draft,"(");strcat(lab.draft,"1");for(int j=0;j<40;j++)strcat(lab.draft,")");strcat(lab.draft,"\ny = 0");assert(!lab_apply(&s));assert(strstr(lab.error.message,"nesting"));
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);assert(TTF_Init()==0);
    lab_init();active_generator=6;s.points=20700;s.zoom=2;s.t=12;s.paused=1;Settings normal=s;
    press(&s,SDLK_l,0);assert(lab.open&&active_generator==6&&s.points==10000);press(&s,SDLK_TAB,0);assert(lab.focus==2);press(&s,SDLK_TAB,0);assert(lab.focus==1);
    press(&s,SDLK_h,0);press(&s,SDLK_l,0);assert(lab.open&&!lab.help);lab_insert("hl");assert(!strncmp(lab.draft,"hl",2));
    press(&s,SDLK_BACKSPACE,0);press(&s,SDLK_DELETE,0);press(&s,SDLK_HOME,KMOD_CTRL);press(&s,SDLK_END,KMOD_CTRL);assert(lab.cursor==(int)strlen(lab.draft));
    press(&s,SDLK_F6,0);assert(!strcmp(lab.draft,lab.applied));press(&s,SDLK_r,KMOD_CTRL);assert(!strcmp(lab.draft,lab_starter));
    for(int j=0;j<100;j++){press(&s,SDLK_s,KMOD_CTRL);assert(!lab.error.message[0]);}
    press(&s,SDLK_ESCAPE,0);assert(!lab.focus&&lab.open);press(&s,SDLK_h,0);assert(lab.help);press(&s,SDLK_SPACE,0);assert(s.paused);
    press(&s,SDLK_l,0);assert(!lab.open&&!memcmp(&s,&normal,sizeof s));assert(active_generator==6);
    for(int j=0;j<100;j++){press(&s,SDLK_l,0);press(&s,SDLK_l,0);}assert(!memcmp(&s,&normal,sizeof s));
    const char *paths[]={"/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"};
    for(int j=0;j<2&&!font;j++)font=TTF_OpenFont(paths[j],12);
    assert(font);
    const int sizes[][3]={{1920,1080,1},{1100,640,1},{760,600,1},{2200,1280,2}};
    for(int j=0;j<4;j++){
        ui_scale=sizes[j][2];assert(TTF_SetFontSize(font,12*ui_scale)==0);text_clear();
        int w=sizes[j][0],h=sizes[j][1];SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
        SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface);assert(r);color(r,bg);SDL_RenderClear(r);lab.open=1;lab.focus=1;
        Layout l=lab_layout(w,h);assert(l.plot.w>0&&l.plot.h>0);if(!l.beacon){double scale=fmin(l.plot.w,l.plot.h)/400.;grid(r,l,scale);color(r,signal_color);
            for(int k=0;k<10000;k++){assert(formula_eval(&lab.formula,k,10000,.7,&x,&y,&e));SDL_RenderDrawPointF(r,l.plot.x+l.plot.w/2.+x*scale,l.plot.y+l.plot.h/2.+y*scale);}}
        lab_draw(r,w,h,s);char path[128];snprintf(path,sizeof path,"build/qa/layout-%dx%d-dpi%d.bmp",w,h,ui_scale);assert(screenshot(r,path,w,h));text_clear();SDL_DestroyRenderer(r);SDL_FreeSurface(surface);
    }
    double start=monotonic_seconds(),checksum=0;
    for(int frame=0;frame<120;frame++)for(int k=0;k<10000;k++){assert(formula_eval(&lab.formula,k,10000,frame/60.,&x,&y,&e));checksum+=x+y;}
    printf("PASS parser, atomic apply, limits, runtime error, editor/focus, 100 applies/toggles, layout/DPI\n120 x 10000 point evaluation: %.3f ms/frame (checksum %.9g)\n",(monotonic_seconds()-start)*1000/120,checksum);
    text_clear();TTF_CloseFont(font);font=NULL;TTF_Quit();SDL_Quit();return 0;
}
