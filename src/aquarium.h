#ifndef PHOSPHOR_AQUARIUM_H
#define PHOSPHOR_AQUARIUM_H
/* Aquarium owns display state only; editor drafts and library bytes are untouched. */
typedef struct {
    int active,slot,formula_active,failed,changed,interval,settings_blocked;
    unsigned rejected;
    Settings saved;size_t saved_generator;
    Formula formula;
    double last,remaining,fade;
    SDL_Texture *source;
    char message[96];
} Aquarium;
static Aquarium aquarium;
static double aquarium_alpha(void){if(!aquarium.active)return 1;double x=fmin(1,fmax(0,aquarium.fade));return x*x*(3-2*x);}
static void aquarium_destroy(void){SDL_DestroyTexture(aquarium.source);aquarium.source=NULL;}
static void aquarium_init(void){
    aquarium_destroy();aquarium=(Aquarium){.interval=15,.slot=-1,.fade=1};
    char path[4200];snprintf(path,sizeof path,"%s/settings.pfs",library.directory);
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0){aquarium.settings_blocked=errno!=ENOENT;return;}
    struct stat st;char data[80]={0};ssize_t n=read(fd,data,sizeof data-1);
    const char prefix[]="PHOSPHOR_SETTINGS 1\ninterval ";char *end=NULL;errno=0;
    long interval=n>(ssize_t)strlen(prefix)&&!strncmp(data,prefix,strlen(prefix))?strtol(data+strlen(prefix),&end,10):0;
    int valid=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size<79&&n>0&&!errno&&end&&
        end!=data+strlen(prefix)&&*end=='\n'&&!end[1]&&interval>=5&&interval<=120;
    close(fd);if(valid)aquarium.interval=interval;else aquarium.settings_blocked=1;
}
static int aquarium_save(void){
    if(aquarium.settings_blocked||!library_mkdir(library.directory))return 0;
    char path[4200],temp[4200];snprintf(path,sizeof path,"%s/settings.pfs",library.directory);
    snprintf(temp,sizeof temp,"%s/.settings-XXXXXX",library.directory);
    int fd=mkstemp(temp);if(fd<0)return 0;
    char data[80];int n=snprintf(data,sizeof data,"PHOSPHOR_SETTINGS 1\ninterval %d\n",aquarium.interval);
    int ok=write(fd,data,n)==n&&!fsync(fd);if(close(fd))ok=0;
    if(ok)ok=!rename(temp,path);
    if(!ok)unlink(temp);
    int directory=open(library.directory,O_RDONLY|O_DIRECTORY);
    if(directory>=0){if(fsync(directory))ok=0;close(directory);}else ok=0;
    return ok;
}
static int aquarium_generate(int n,double phase,Point *out){
    FormulaError error={0};
    for(int k=0;k<n;k++){
        double x,y;if(!formula_eval(&aquarium.formula,n-1-k,n,phase,&x,&y,&error)||!isfinite(x)||!isfinite(y))return 0;
        out[k]=(Point){x,y}; /* same frame as lab_generate */
    }return 1;
}
/* Name of the formula creature the Aquarium is showing; NULL for natives. */
static const char *aquarium_display_name(void){
    if(!aquarium.active||!aquarium.formula_active||aquarium.slot<0||aquarium.slot>9)return NULL;
    int index=library_index(&library,library.favorites[aquarium.slot]);
    return index>=0?library.entries[index].name:NULL;
}
static void aquarium_stop(Settings *s){
    if(!aquarium.active)return;
    *s=aquarium.saved;active_generator=aquarium.saved_generator;generator_generation++;
    aquarium.active=0;aquarium.fade=1;observer_lab_active=0;aquarium.formula_active=0;aquarium.failed=0;aquarium_destroy();text_clear();
}
static int aquarium_key(Settings *s,const SDL_KeyboardEvent *e){
    if(e->repeat||e->type!=SDL_KEYDOWN||lab.open||lab.dialog||lab.searching)return 0;
    SDL_Keycode k=e->keysym.sym;int plain=!(e->keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI|KMOD_SHIFT|KMOD_MODE));
    /* Match the existing field dispatch, including Shift+L/B and modified Q/R. */
    if(aquarium.active){
        if(k==SDLK_ESCAPE){aquarium_stop(s);return 1;}
        if(k==SDLK_q||k==SDLK_r||((k==SDLK_l||k==SDLK_b)&&!(e->keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI)))||
           (plain&&k>=SDLK_0&&k<=SDLK_9)){aquarium_stop(s);return 0;}
    }
    if(!plain)return 0;
    if(k==SDLK_a){
        if(aquarium.active)aquarium_stop(s);
        else {aquarium.saved=*s;aquarium.saved_generator=active_generator;aquarium.active=1;
            aquarium.slot=-1;aquarium.rejected=0;aquarium.failed=0;aquarium.formula_active=0;
            aquarium.last=monotonic_seconds();aquarium.remaining=0;aquarium.fade=1;*aquarium.message=0;}
        return 1;
    }
    if(!aquarium.active)return 0;
    if(k==SDLK_ESCAPE){aquarium_stop(s);return 1;}
    if(k==SDLK_COMMA||k==SDLK_PERIOD){
        aquarium.interval+=(k==SDLK_PERIOD?1:-1);if(aquarium.interval<5)aquarium.interval=5;if(aquarium.interval>120)aquarium.interval=120;
        aquarium.remaining=aquarium.interval;
        snprintf(aquarium.message,sizeof aquarium.message,aquarium_save()?"INTERVAL SAVED":"INTERVAL VOLATILE / settings write unavailable");return 1;
    }
    if(k==SDLK_q||k==SDLK_l||k==SDLK_b||k==SDLK_r||(k>=SDLK_0&&k<=SDLK_9)){aquarium_stop(s);return 0;}
    return 0;
}
static int aquarium_count(void){int n=0;for(int i=0;i<10;i++)n+=*library.favorites[i]!=0&&!(aquarium.rejected&(1u<<i));return n;}
static int aquarium_snapshot(SDL_Renderer *r,SDL_Texture *trail){
    if(!trail){aquarium_destroy();aquarium.fade=1;return 1;}
    int w,h;if(SDL_QueryTexture(trail,NULL,NULL,&w,&h)){aquarium_destroy();aquarium.fade=1;return 0;}
    SDL_Texture *source=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,w,h);
    int ok=source&&SDL_SetRenderTarget(r,source)==0;
    double alpha=aquarium_alpha();
    if(ok){
        SDL_SetRenderDrawColor(r,0,0,0,255);ok=SDL_RenderClear(r)==0;
        SDL_SetTextureAlphaMod(trail,(Uint8)lround(255*alpha));
        if(SDL_RenderCopy(r,trail,NULL,NULL))ok=0;
        SDL_SetTextureAlphaMod(trail,255);
        if(aquarium.source&&alpha<1){
            SDL_SetTextureAlphaMod(aquarium.source,(Uint8)lround(255*(1-alpha)));
            if(SDL_RenderCopy(r,aquarium.source,NULL,NULL))ok=0;
        }
    }
    if(SDL_SetRenderTarget(r,NULL))ok=0;
    aquarium_destroy();
    if(!ok){SDL_DestroyTexture(source);aquarium.fade=1;return 0;}
    aquarium.source=source;SDL_SetTextureBlendMode(source,SDL_BLENDMODE_ADD);aquarium.fade=0;return 1;
}
static void aquarium_tick(Settings *s,double now,SDL_Texture *trail,SDL_Renderer *r,Point *scratch){
    if(!aquarium.active)return;
    double dt=fmax(0,now-aquarium.last);aquarium.last=now;
    if(!s->paused){
        if(aquarium.fade<1){aquarium.fade=fmin(1,aquarium.fade+dt);if(aquarium.fade>=1)aquarium_destroy();}
        else aquarium.remaining-=dt;
    }
    if(aquarium.failed&&aquarium.slot>=0){aquarium.rejected|=1u<<aquarium.slot;aquarium.remaining=0;}
    int count=aquarium_count();
    if(!count){snprintf(aquarium.message,sizeof aquarium.message,aquarium.rejected?"NO VALID FAVORITES / A BACK":"NO FAVORITES / A BACK");return;}
    if(aquarium.slot>=0&&!aquarium.failed&&(count==1||aquarium.remaining>0||aquarium.fade<1))return;
    int previous=aquarium.slot;
    for(int step=1;step<=10;step++){
        int slot=(previous+step)%10;const char *id=library.favorites[slot];
        if(!*id||(aquarium.rejected&(1u<<slot)))continue;
        int native=library_native(id),index=library_index(&library,id);Formula candidate;FormulaError error={0};int valid=1;
        if(native<0){
            valid=index>=0&&formula_compile(library.entries[index].formula,&candidate,&error)&&lab_validate(&candidate,s->points,0,&error);
            /* Reject offset overflow too, before touching any render layer. */
            if(valid){Formula old=aquarium.formula;aquarium.formula=candidate;valid=aquarium_generate(s->points,0,scratch);aquarium.formula=old;}
        }else {
            generator_at(native)->generate(s->points,0,scratch);
            for(int i=0;i<s->points;i++)if(!isfinite(scratch[i].x)||!isfinite(scratch[i].y)){valid=0;break;}
        }
        if(!valid){aquarium.rejected|=1u<<slot;continue;}
        int fade_ok=aquarium_snapshot(r,trail);
        aquarium.slot=slot;aquarium.formula_active=native<0;if(native<0)aquarium.formula=candidate;
        active_generator=native<0?0:(size_t)native;generator_generation++;s->t=0;
        aquarium.remaining=aquarium.interval;aquarium.failed=0;aquarium.changed=1;
        snprintf(aquarium.message,sizeof aquarium.message,"%s",!fade_ok?"FADE UNAVAILABLE / renderer snapshot":aquarium.rejected?"INVALID FAVORITE SKIPPED":"");return;
    }
    aquarium.failed=1;snprintf(aquarium.message,sizeof aquarium.message,"NO VALID FAVORITES / A BACK");
}
static void aquarium_status(char *out,size_t cap){
    if(!aquarium_count()){snprintf(out,cap,"AQUARIUM / %s",aquarium.message);return;}
    snprintf(out,cap,"AQUARIUM %c / %s %.0fs / %ds / A BACK / %s",aquarium.slot==9?'0':aquarium.slot<0?'-':'1'+aquarium.slot,
        aquarium_count()==1?"STATIC":"NEXT",aquarium.remaining,aquarium.interval,aquarium.message);
}
static void aquarium_json(char *out,size_t cap){
    snprintf(out,cap,"{\"active\":%s,\"slot\":%d,\"interval_s\":%d,\"fade\":%.6f,\"rejected\":%u}",aquarium.active?"true":"false",aquarium.slot,aquarium.interval,aquarium_alpha(),aquarium.rejected);
}
#endif
