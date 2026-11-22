#ifndef PHOSPHOR_LIBRARY_H
#define PHOSPHOR_LIBRARY_H
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "formula.h"
#define LIB_CAP 64
#define LIB_NAME 80
#define LIB_ID 40
#define LIB_FILE_MAX 262144
/* Native references are not serialized as editable formulas. */
typedef struct {char id[LIB_ID],name[LIB_NAME],formula[FORMULA_TEXT];} Creature;
typedef struct {
    Creature entries[LIB_CAP];int count,blocked;
    char favorites[10][LIB_ID],directory[4096],status[160];
    uint64_t fingerprint;
} CreatureLibrary;
static CreatureLibrary library;
static int library_index(const CreatureLibrary *l,const char *id){for(int i=0;i<l->count;i++)if(!strcmp(l->entries[i].id,id))return i;return -1;}
static int library_native(const char *id){for(size_t i=0;i<generator_count();i++)if(!strcmp(generator_at(i)->id,id))return (int)i;return -1;}
static int library_id_valid(const char *id){
    if(strlen(id)!=34||strncmp(id,"c-",2))return 0;
    for(int i=2;i<34;i++)if(!isxdigit((unsigned char)id[i])||isupper((unsigned char)id[i]))return 0;
    return 1;
}
static int library_name_valid(const char *name){
    size_t n=strlen(name);if(!n||n>=LIB_NAME)return 0;
    int visible=0;for(size_t i=0;i<n;i++){if((unsigned char)name[i]<32||(unsigned char)name[i]>126)return 0;if(name[i]!=' ')visible=1;}return visible;
}
static int library_formula_valid(const char *text){
    Formula f;FormulaError error;double x,y;
    if(!formula_compile(text,&f,&error))return 0;
    /* Structural/numeric baseline on reload. Animation is still checked every frame. */
    for(int i=0;i<10000;i++)if(!formula_eval(&f,i,10000,0,&x,&y,&error))return 0;
    return 1;
}
static uint64_t library_hash(FILE *file){
    uint64_t h=1469598103934665603ULL;int c;size_t bytes=0;while((c=fgetc(file))!=EOF){if(++bytes>LIB_FILE_MAX)return 0;h=(h^(unsigned char)c)*1099511628211ULL;}
    return ferror(file)?0:h;
}
static FILE *library_open(const char *path){
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return NULL;
    struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size>LIB_FILE_MAX||st.st_size<1){close(fd);errno=EINVAL;return NULL;}
    FILE *f=fdopen(fd,"rb");if(!f)close(fd);return f;
}
static int library_read(CreatureLibrary *l,FILE *f){
    char line[512];int version=0,used=0;
    if(!fgets(line,sizeof line,f)||sscanf(line,"PHOSPHOR_LIBRARY %d %n",&version,&used)!=1||line[used]||version!=1)return 0;
    for(int i=0;i<10;i++){
        char id[LIB_ID];int slot=-1;
        if(!fgets(line,sizeof line,f)||sscanf(line,"FAVORITE %d %39s %n",&slot,id,&used)!=2||line[used]||slot!=i)return 0;
        if(strcmp(id,"-")&&library_native(id)<0&&!library_id_valid(id))return 0;
        snprintf(l->favorites[i],LIB_ID,"%s",strcmp(id,"-")?id:"");
    }
    if(!fgets(line,sizeof line,f)||sscanf(line,"COUNT %d %n",&l->count,&used)!=1||line[used]||l->count<0||l->count>LIB_CAP)return 0;
    for(int i=0;i<l->count;i++){
        Creature *c=&l->entries[i];int namesize=0,textsize=0;
        if(!fgets(line,sizeof line,f)||sscanf(line,"ENTRY %39s %d %d %n",c->id,&namesize,&textsize,&used)!=3||line[used]||!library_id_valid(c->id)||namesize<1||namesize>=LIB_NAME||textsize<1||textsize>=FORMULA_TEXT)return 0;
        for(int j=0;j<i;j++)if(!strcmp(c->id,l->entries[j].id))return 0;
        if(fread(c->name,1,namesize,f)!=(size_t)namesize||fgetc(f)!='\n')return 0;
        c->name[namesize]=0;
        if(fread(c->formula,1,textsize,f)!=(size_t)textsize||fgetc(f)!='\n')return 0;
        c->formula[textsize]=0;
        if((int)strlen(c->name)!=namesize||(int)strlen(c->formula)!=textsize||!library_name_valid(c->name)||!library_formula_valid(c->formula))return 0;
    }
    for(int i=0;i<10;i++)if(*l->favorites[i]&&library_native(l->favorites[i])<0&&library_index(l,l->favorites[i])<0)return 0;
    return fgetc(f)==EOF&&!ferror(f);
}
static void library_init(CreatureLibrary *l,const char *override){
    *l=(CreatureLibrary){0};for(int i=0;i<NATIVE_COUNT;i++)snprintf(l->favorites[i],LIB_ID,"%s",generator_at(i)->id);
    const char *xdg=getenv("XDG_DATA_HOME"),*home=getenv("HOME");int n;
    if(override)n=snprintf(l->directory,sizeof l->directory,"%s",override);
    else if(xdg&&*xdg=='/')n=snprintf(l->directory,sizeof l->directory,"%s/phosphor-field",xdg);
    else n=snprintf(l->directory,sizeof l->directory,"%s/.local/share/phosphor-field",home?home:"");
    if(n<0||(size_t)n>=sizeof l->directory||l->directory[0]!='/'){l->blocked=1;snprintf(l->status,sizeof l->status,"LIBRARY PATH UNAVAILABLE");return;}
    char path[4200];snprintf(path,sizeof path,"%s/library.pfl",l->directory);FILE *f=library_open(path);
    if(!f){if(errno!=ENOENT)l->blocked=1;snprintf(l->status,sizeof l->status,l->blocked?"LIBRARY READ ERROR / writes blocked":"NEW LIBRARY / slot 1 Medusa, 2-0 free");return;}
    CreatureLibrary *candidate=calloc(1,sizeof *candidate);
    int valid=candidate&&library_read(candidate,f);rewind(f);uint64_t fingerprint=library_hash(f);fclose(f);
    if(valid&&fingerprint){memcpy(l->entries,candidate->entries,sizeof l->entries);memcpy(l->favorites,candidate->favorites,sizeof l->favorites);l->count=candidate->count;l->fingerprint=fingerprint;snprintf(l->status,sizeof l->status,"LIBRARY LOADED / %d creations",l->count);}
    else {l->blocked=1;snprintf(l->status,sizeof l->status,"INVALID / incompatible library; writes blocked");}free(candidate);
}
static int library_mkdir(const char *path){
    char work[4096];snprintf(work,sizeof work,"%s",path);
    for(char *p=work+1;;p++)if(*p=='/'||!*p){char c=*p;*p=0;
        if(mkdir(work,0700)&&errno!=EEXIST)return 0;
        struct stat st;if(stat(work,&st)||!S_ISDIR(st.st_mode))return 0;
        *p=c;if(!c)break;
    }
    struct stat st;return !lstat(path,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==geteuid()&&!(st.st_mode&0022);
}
static int library_write_file(const CreatureLibrary *l,FILE *f){
    if(fprintf(f,"PHOSPHOR_LIBRARY 1\n")<0)return 0;
    for(int i=0;i<10;i++)if(fprintf(f,"FAVORITE %d %s\n",i,*l->favorites[i]?l->favorites[i]:"-")<0)return 0;
    if(fprintf(f,"COUNT %d\n",l->count)<0)return 0;
    for(int i=0;i<l->count;i++){
        const Creature *c=&l->entries[i];size_t a=strlen(c->name),b=strlen(c->formula);
        if(fprintf(f,"ENTRY %s %zu %zu\n",c->id,a,b)<0||fwrite(c->name,1,a,f)!=a||fputc('\n',f)==EOF||fwrite(c->formula,1,b,f)!=b||fputc('\n',f)==EOF)return 0;
    }return !ferror(f);
}
/* The caller mutates a candidate; memory changes commit only after durable file replacement. */
static int library_commit(CreatureLibrary *l,CreatureLibrary *candidate){
    if(l->blocked){snprintf(l->status,sizeof l->status,"SAVE BLOCKED / repair library and restart");return 0;}
    char path[4200],lockpath[4200],temp[4200],backup[4200],backup_temp[4200];int ok=0,lock=-1,fd=-1;FILE *f=NULL;
    if(!library_mkdir(l->directory))goto done;
    snprintf(path,sizeof path,"%s/library.pfl",l->directory);snprintf(lockpath,sizeof lockpath,"%s/library.lock",l->directory);
    lock=open(lockpath,O_CREAT|O_RDWR|O_NOFOLLOW|O_NONBLOCK,0600);struct stat st;
    if(lock<0||fstat(lock,&st)||!S_ISREG(st.st_mode)||flock(lock,LOCK_EX|LOCK_NB))goto done;
    f=library_open(path);uint64_t current=0;
    if(f){current=library_hash(f);fclose(f);f=NULL;if(!current)goto done;}else if(errno!=ENOENT)goto done;
    if(current!=l->fingerprint){snprintf(l->status,sizeof l->status,"EXTERNAL CHANGE / restart before saving");close(lock);return 0;}
    snprintf(temp,sizeof temp,"%s/.library-XXXXXX",l->directory);fd=mkstemp(temp);if(fd<0)goto done;
    f=fdopen(fd,"w+b");if(!f){close(fd);unlink(temp);goto done;}fd=-1;
    ok=library_write_file(candidate,f)&&!fflush(f)&&!fsync(fileno(f));
    rewind(f);uint64_t fingerprint=library_hash(f);if(fclose(f))ok=0;f=NULL;
    if(!ok||!fingerprint){unlink(temp);ok=0;goto done;}
    if(current){
        snprintf(backup,sizeof backup,"%s/library.pfl.bak",l->directory);
        snprintf(backup_temp,sizeof backup_temp,"%s/.backup-XXXXXX",l->directory);
        int b=mkstemp(backup_temp);if(b<0){unlink(temp);ok=0;goto done;}close(b);unlink(backup_temp);
        if(link(path,backup_temp)||rename(backup_temp,backup)){unlink(backup_temp);unlink(temp);ok=0;goto done;}
    }
    if(rename(temp,path)){unlink(temp);ok=0;goto done;}
    /* Rename is the commit point. A failed directory fsync is reported, never rolled back in memory. */
    int directory=open(l->directory,O_RDONLY|O_DIRECTORY);int synced=directory>=0&&!fsync(directory);if(directory>=0)close(directory);
    candidate->fingerprint=fingerprint;*l=*candidate;
    snprintf(l->status,sizeof l->status,synced?"SAVED / atomic local commit":"SAVED / directory sync unavailable");ok=1;
done:
    if(f)fclose(f);
    if(fd>=0)close(fd);
    if(lock>=0)close(lock);
    if(!ok)snprintf(l->status,sizeof l->status,"SAVE FAILED / previous library retained");
    return ok;
}
static int library_new_id(const CreatureLibrary *l,char *id){
    unsigned char bytes[16];int fd=open("/dev/urandom",O_RDONLY);if(fd<0)return 0;int ok=read(fd,bytes,sizeof bytes)==sizeof bytes;close(fd);if(!ok)return 0;
    strcpy(id,"c-");for(int i=0;i<16;i++)snprintf(id+2+i*2,3,"%02x",bytes[i]);for(int i=0;i<l->count;i++)if(l->entries[i].id!=id&&!strcmp(l->entries[i].id,id))return 0;return 1;
}
static int library_save(CreatureLibrary *l,char *id,const char *name,const char *formula,int as_new){
    if(!library_name_valid(name)||!library_formula_valid(formula)){snprintf(l->status,sizeof l->status,"INVALID NAME / FORMULA (phase 0 required)");return 0;}
    CreatureLibrary *c=malloc(sizeof *c);if(!c){snprintf(l->status,sizeof l->status,"SAVE FAILED / allocation");return 0;}*c=*l;
    int index=as_new?-1:library_index(c,id);
    if(index<0){if(c->count==LIB_CAP){free(c);snprintf(l->status,sizeof l->status,"LIBRARY FULL / 64 creations");return 0;}index=c->count++;
        if(!library_new_id(c,c->entries[index].id)){free(c);snprintf(l->status,sizeof l->status,"SAVE FAILED / unique ID unavailable");return 0;}}
    Creature *e=&c->entries[index];snprintf(e->name,sizeof e->name,"%s",name);snprintf(e->formula,sizeof e->formula,"%s",formula);
    char saved[LIB_ID];strcpy(saved,e->id);int ok=library_commit(l,c);if(ok)strcpy(id,saved);free(c);return ok;
}
static int library_favorite(CreatureLibrary *l,int slot,const char *id){
    if(slot<0||slot>=10||(*id&&library_native(id)<0&&library_index(l,id)<0))return 0;
    CreatureLibrary *c=malloc(sizeof *c);if(!c){snprintf(l->status,sizeof l->status,"LIBRARY FAILED / allocation");return 0;}*c=*l;snprintf(c->favorites[slot],LIB_ID,"%s",id);int ok=library_commit(l,c);free(c);if(ok&&!strstr(l->status,"sync unavailable"))snprintf(l->status,sizeof l->status,"FAVORITE %c %s",slot==9?'0':'1'+slot,*id?"ASSIGNED":"CLEARED");return ok;
}
static int library_delete(CreatureLibrary *l,const char *id){
    int index=library_index(l,id);if(index<0)return 0;
    CreatureLibrary *c=malloc(sizeof *c);if(!c){snprintf(l->status,sizeof l->status,"LIBRARY FAILED / allocation");return 0;}*c=*l;
    memmove(c->entries+index,c->entries+index+1,(c->count-index-1)*sizeof *c->entries);c->count--;
    for(int i=0;i<10;i++)if(!strcmp(c->favorites[i],id))*c->favorites[i]=0;
    int ok=library_commit(l,c);free(c);if(ok&&!strstr(l->status,"sync unavailable"))snprintf(l->status,sizeof l->status,"DELETED / favorite links removed");return ok;
}
#endif
