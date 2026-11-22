#define main instrument_main
#include "../src/main.c"
#undef main
#include <assert.h>
#include <sys/wait.h>
#include <sys/resource.h>
static void key(Settings *s,SDL_Keycode k,int mod){SDL_KeyboardEvent e={.type=SDL_KEYDOWN,.keysym={.sym=k,.mod=mod}};assert(lab_key(s,&e));}
static void replace(const char *value){lab.anchor=0;lab.cursor=(int)strlen(lab.draft);lab_insert(value);}
static void write_bad(const char *dir,const char *value){char p[4300];snprintf(p,sizeof p,"%s/library.pfl",dir);FILE *f=fopen(p,"wb");assert(f);assert(fputs(value,f)>=0);assert(!fclose(f));}
static void restart(const char *self,const char *dir,const char *id){pid_t pid=fork();assert(pid>=0);if(!pid){execl(self,self,"--reload",dir,id,(char*)NULL);_exit(99);}int status;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}
int main(int argc,char **argv){
    if(argc==4&&!strcmp(argv[1],"--reload")){
        CreatureLibrary *l=calloc(1,sizeof *l);assert(l);library_init(l,argv[2]);assert(!l->blocked&&l->count==2);
        assert(!strcmp(l->favorites[9],argv[3]));assert(library_index(l,argv[3])>=0);
        char id[LIB_ID];strcpy(id,argv[3]);assert(library_save(l,id,"Renamed after restart",lab_starter,0));assert(!strcmp(id,argv[3]));free(l);return 0;
    }
    Formula f;FormulaError e;double x,y;
    const char *formula="param size = 12 [1,100,0.5]\nx = min(pow(2,3),9) + sqrt(16)\ny = max(abs(-3),cos(pi)) * size";
    assert(formula_compile(formula,&f,&e)&&f.param_count==1);assert(formula_eval(&f,0,1,0,&x,&y,&e)&&x==12&&y==36);
    const char *invalid[]={"param a = 3 [4,10,1]\nx=0\ny=0","param a = 3 [0,10,0]\nx=0\ny=0","param i = 3 [0,10,1]\nx=0\ny=0","param a=1 [0,2,1]\nparam a=1 [0,2,1]\nx=0\ny=0","x=sqrt(-1)\ny=0","x=pow(-1,0.5)\ny=0","x=pow(1)\ny=0"};
    for(size_t i=0;i<sizeof invalid/sizeof invalid[0];i++)if(formula_compile(invalid[i],&f,&e))assert(!formula_eval(&f,0,100,0,&x,&y,&e));
    unsigned random=987654321;char fuzz[256];
    for(int trial=0;trial<3000;trial++){int n=trial%255;for(int j=0;j<n;j++){random=random*1664525u+1013904223u;fuzz[j]="xyparam=0123456789+-*/(),[] sincostu\n"[random%37];}fuzz[n]=0;if(formula_compile(fuzz,&f,&e))formula_eval(&f,0,100,0,&x,&y,&e);}
    assert(!formula_compile("param a=1 [0,2,1]\nparam b=1 [0,2,1]\nparam c=1 [0,2,1]\nparam d=1 [0,2,1]\nparam e=1 [0,2,1]\nparam f=1 [0,2,1]\nparam g=1 [0,2,1]\nparam h=1 [0,2,1]\nparam j=1 [0,2,1]\nx=0\ny=0",&f,&e));
    char directory[]="/tmp/phosphor-creation-XXXXXX";assert(mkdtemp(directory));CreatureLibrary *l=calloc(1,sizeof *l),*stale=calloc(1,sizeof *l);assert(l&&stale);library_init(l,directory);assert(!l->blocked&&l->count==0);
    assert(!strcmp(l->favorites[0],"pf-medusa"));for(int j=NATIVE_COUNT;j<10;j++)assert(!*l->favorites[j]);
    assert(!*l->favorites[9]);
    char first[LIB_ID]="",second[LIB_ID]="";assert(library_save(l,first,"Same name",formula,0));assert(library_id_valid(first));assert(library_save(l,second,"Same name",lab_starter,1));assert(strcmp(first,second)&&l->count==2);
    for(int j=0;j<10;j++)assert(library_favorite(l,j,j%2?first:second));
    assert(library_favorite(l,9,first));
    restart(argv[0],directory,first);library_init(l,directory);assert(!strcmp(l->entries[library_index(l,first)].name,"Renamed after restart"));assert(l->count==2);
    char backup[4200];snprintf(backup,sizeof backup,"%s/library.pfl.bak",directory);assert(access(backup,R_OK)==0);
    *stale=*l;assert(library_favorite(l,0,"pf-medusa"));assert(!library_favorite(stale,1,"pf-medusa"));assert(strstr(stale->status,"EXTERNAL CHANGE"));
    char lockpath[4200];snprintf(lockpath,sizeof lockpath,"%s/library.lock",directory);int lock=open(lockpath,O_RDWR);assert(lock>=0&&!flock(lock,LOCK_EX|LOCK_NB));assert(!library_favorite(l,2,first));close(lock);
    assert(library_delete(l,first));for(int j=0;j<10;j++)assert(strcmp(l->favorites[j],first));assert(l->count==1&&!strcmp(l->entries[0].id,second));
    uint64_t before_hash=l->fingerprint;pid_t limited=fork();assert(limited>=0);
    if(!limited){struct rlimit limit={32,32};signal(SIGXFSZ,SIG_IGN);assert(!setrlimit(RLIMIT_FSIZE,&limit));assert(!library_favorite(l,4,second));_exit(0);}
    int limited_status;assert(waitpid(limited,&limited_status,0)==limited&&WIFEXITED(limited_status)&&WEXITSTATUS(limited_status)==0);library_init(l,directory);assert(l->fingerprint==before_hash);
    char orphan[4200];snprintf(orphan,sizeof orphan,"%s/.library-interrupted",directory);FILE *partial=fopen(orphan,"wb");assert(partial);fputs("partial uncommitted",partial);fclose(partial);library_init(l,directory);assert(l->fingerprint==before_hash&&!l->blocked);
    /* Libraries from builds with other native IDs are refused, never rewritten. */
    char legacy_dir[4200];snprintf(legacy_dir,sizeof legacy_dir,"%s/legacy",directory);assert(!mkdir(legacy_dir,0700));
    {char src[4300],dst[4300];snprintf(src,sizeof src,"%s/library.pfl",directory);snprintf(dst,sizeof dst,"%s/library.pfl",legacy_dir);
     FILE *in=fopen(src,"rb");assert(in);static char text[LIB_FILE_MAX+1];size_t size=fread(text,1,sizeof text-1,in);fclose(in);text[size]=0;
     assert(library_favorite(l,0,"pf-medusa"));in=fopen(src,"rb");assert(in);size=fread(text,1,sizeof text-1,in);fclose(in);text[size]=0;
     char *hit=strstr(text,"pf-medusa");assert(hit);char legacy_text[LIB_FILE_MAX+16];
     snprintf(legacy_text,sizeof legacy_text,"%.*sretired-native-01%s",(int)(hit-text),text,hit+9);write_bad(legacy_dir,legacy_text);
     library_init(stale,legacy_dir);assert(stale->blocked);assert(!library_favorite(stale,2,"pf-medusa"));
     FILE *check=fopen(dst,"rb");assert(check);static char after[LIB_FILE_MAX+16];size_t n=fread(after,1,sizeof after-1,check);fclose(check);after[n]=0;assert(!strcmp(after,legacy_text));}
    char bad_dir[4200];snprintf(bad_dir,sizeof bad_dir,"%s/bad",directory);assert(!mkdir(bad_dir,0700));write_bad(bad_dir,"PHOSPHOR_LIBRARY 999\n");library_init(stale,bad_dir);assert(stale->blocked);assert(!library_save(stale,first,"Do not overwrite",lab_starter,1));
    char bad_path[4300];snprintf(bad_path,sizeof bad_path,"%s/library.pfl",bad_dir);FILE *bad=fopen(bad_path,"rb");assert(bad);char contents[80];assert(fgets(contents,sizeof contents,bad)&&!strcmp(contents,"PHOSPHOR_LIBRARY 999\n"));fclose(bad);
    bad=fopen(bad_path,"wb");assert(bad);for(int j=0;j<=LIB_FILE_MAX;j++)assert(fputc('x',bad)!=EOF);fclose(bad);library_init(stale,bad_dir);assert(stale->blocked);
    assert(!unlink(bad_path));assert(!symlink("/dev/null",bad_path));library_init(stale,bad_dir);assert(stale->blocked);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);lab_init();library_init(&library,directory);Settings s=defaults();key(&s,SDLK_l,0);key(&s,SDLK_TAB,0);assert(lab.focus==2&&lab.panel_valid);
    double old=lab.panel.params[0];key(&s,SDLK_LEFT,KMOD_SHIFT);assert(lab.panel.params[0]==old-.1);assert(strcmp(lab.draft,lab.applied));lab_parameter(999);assert(lab.panel.params[0]==old-.1);
    key(&s,SDLK_RETURN,0);assert(lab.dialog==5);key(&s,SDLK_a,KMOD_CTRL);lab_text("85.25");key(&s,SDLK_RETURN,0);assert(!lab.dialog&&lab.panel.params[0]==85.25);key(&s,SDLK_s,KMOD_CTRL);assert(!lab.error.message[0]);
    for(int j=0;j<6;j++){char expression[40];snprintf(expression,sizeof expression,"x = %d\ny = 0",j);replace(expression);assert(lab_apply(&s));}
    assert(lab.revision_count==4);for(int j=4;j>=1;j--){key(&s,SDLK_F7,0);assert(formula_eval(&lab.formula,0,1,0,&x,&y,&e)&&x==j);}assert(lab.revision_count==0);
    key(&s,SDLK_r,KMOD_CTRL);
    key(&s,SDLK_TAB,0);assert(lab.focus==1);key(&s,SDLK_a,KMOD_CTRL);key(&s,SDLK_c,KMOD_CTRL);char *copy=SDL_GetClipboardText();assert(copy&&!strcmp(copy,lab.draft));SDL_free(copy);key(&s,SDLK_x,KMOD_CTRL);assert(!*lab.draft);key(&s,SDLK_z,KMOD_CTRL);assert(*lab.draft);key(&s,SDLK_y,KMOD_CTRL);assert(!*lab.draft);key(&s,SDLK_v,KMOD_CTRL);assert(*lab.draft);
    key(&s,SDLK_d,KMOD_CTRL);assert(lab.dialog==1);key(&s,SDLK_a,KMOD_CTRL);lab_text("Editable saved creature");key(&s,SDLK_RETURN,0);assert(!lab.dialog&&*lab.id&&!lab_unsaved());char saved[LIB_ID];strcpy(saved,lab.id);
    key(&s,SDLK_s,KMOD_CTRL|KMOD_SHIFT);assert(lab.dialog==2);key(&s,SDLK_RETURN,0);assert(strcmp(saved,lab.id));
    key(&s,SDLK_ESCAPE,0);key(&s,SDLK_b,0);assert(lab.browser);key(&s,SDLK_SLASH,0);lab_text("Editable saved");key(&s,SDLK_RETURN,0);int indices[LIB_CAP+NATIVE_COUNT];assert(lab_list(indices)==2);key(&s,SDLK_0,KMOD_CTRL);assert(!strcmp(library.favorites[9],lab_selected()));
    key(&s,SDLK_F4,0);assert(lab.dialog==3);key(&s,SDLK_a,KMOD_CTRL);lab_text("Renamed in UI");key(&s,SDLK_RETURN,0);assert(!lab.dialog);
    *lab.filter=0;lab.selection=library.count+NATIVE_COUNT-1;key(&s,SDLK_F8,0);assert(lab.dialog==4);int count=library.count;key(&s,SDLK_y,KMOD_CTRL);assert(library.count==count&&lab.dialog==4);key(&s,SDLK_ESCAPE,0);assert(library.count==count);key(&s,SDLK_F8,0);key(&s,SDLK_y,0);assert(library.count==count-1);
    key(&s,SDLK_ESCAPE,0);replace("x = sin(\ny = 0");key(&s,SDLK_s,KMOD_CTRL);assert(lab.error.message[0]);key(&s,SDLK_F6,0);assert(!strcmp(lab.draft,lab.applied));
    /* Formula space shares the native view frame: (0,0) is the plot centre. */
    {replace("x = 0\ny = 0");assert(lab_apply(&s));Point a[4],b[4];assert(lab_generate(4,0,a,b)==1);
     const GeneratorView *v=&generator_at(0)->view;
     for(int k=0;k<4;k++)assert(b[k].x==v->center_x&&b[k].y==v->center_y);}
    replace("x = 1/(t-1)\ny = 0");s.t=0;assert(lab_apply(&s));Point scratch[10],out[10];assert(lab_generate(10,0,scratch,out)==1&&lab_generate(10,1,scratch,out)==-1);key(&s,SDLK_r,KMOD_CTRL);assert(!lab.runtime_error&&s.t==0);
    replace("x=2\ny=3");assert(!lab_confirm_quit(&s)&&lab.dialog==6);key(&s,SDLK_d,KMOD_CTRL);assert(lab.dialog==6&&!lab.quit_confirmed);key(&s,SDLK_ESCAPE,0);assert(!lab.quit_confirmed);assert(!lab_confirm_quit(&s));key(&s,SDLK_d,0);assert(lab.quit_confirmed);
    SDL_Quit();free(l);free(stale);puts("PASS dynamic parameters/functions/domains, atomic persistence/restart/IDs/duplicate names/backups/favorites/conflict/lock/corrupt/oversize/symlink, editor clipboard/selection/undo/redo, parameter input, save/save-as/search/rename, runtime recovery/unsaved exit");return 0;
}
