#ifndef PHOSPHOR_LAB_H
#define PHOSPHOR_LAB_H
#include "formula.h"
#include "library.h"
static const char lab_starter[]=
    "param radius = 120 [1,180,1]\nparam twist = 0.8 [-4,4,0.05]\nparam waves = 3 [1,24,1]\n"
    "x = cos(u * waves + t * twist) * (radius + 18 * sin(u * 5 + t))\n"
    "y = sin(u * waves + t * twist) * (radius + 18 * sin(u * 5 + t))";
typedef struct {
    int open,focus,help,cursor,runtime_error,preview_count;
    double preview_phase;
    Settings settings,saved;
    char draft[FORMULA_TEXT],applied[FORMULA_TEXT],status[160];
    Formula formula,panel;FormulaError error;
    int panel_valid,anchor,browser,searching,selection,param,dialog,quit_confirmed,pending_quit;
    char filter[80],field[80],id[LIB_ID],name[LIB_NAME],persisted[FORMULA_TEXT],pending_id[LIB_ID];
    char undo[16][FORMULA_TEXT];int undo_cursor[16],undo_anchor[16],undo_count,undo_at;
    char revisions[4][FORMULA_TEXT];int revision_count;
    unsigned revision;int suppress_search_text,restoring_revision;double notice_until;

} CreationLab;
static CreationLab lab;
static void lab_error(FormulaError error,const char *source){
    int line=1,col=1;for(int j=0;j<error.pos&&source[j];j++){if(source[j]=='\n'){line++;col=1;}else col++;}
    lab.error=error;snprintf(lab.status,sizeof lab.status,"INVALID L%d C%d: %s",line,col,error.message);
}
static int lab_validate(const Formula *f,int n,double phase,FormulaError *error){
    double x,y;for(int i=n-1;i>=0;i--)if(!formula_eval(f,i,n,phase,&x,&y,error))return 0;
    return 1;
}
static int lab_apply(Settings *s){
    Formula candidate;FormulaError error;
    if(!formula_compile(lab.draft,&candidate,&error)||!lab_validate(&candidate,s->points,s->t,&error)){
        lab_error(error,lab.draft);return 0;
    }
    if(*lab.applied&&strcmp(lab.applied,lab.draft)&&!lab.restoring_revision){
        if(lab.revision_count==4){memmove(lab.revisions,lab.revisions+1,3*sizeof lab.revisions[0]);lab.revision_count--;}
        strcpy(lab.revisions[lab.revision_count++],lab.applied);
    }
    lab.formula=candidate;strcpy(lab.applied,lab.draft);lab.revision++;lab.error=(FormulaError){0};lab.runtime_error=0;
    snprintf(lab.status,sizeof lab.status,"VALID / APPLIED (volatile)");return 1;
}
/* Commit a whole frame, never a partial result. Runtime failure freezes last valid frame. */
static int lab_generate(int n,double phase,Point *scratch,Point *output){
    if(lab.runtime_error)return 0;
    FormulaError error={0};
    for(int k=0;k<n;k++){
        double x,y;
        if(!formula_eval(&lab.formula,n-1-k,n,phase,&x,&y,&error)){
            lab_error(error,lab.applied);lab.runtime_error=1;return -1;
        }
        /* Formula space is the native view frame: centre (0,0), +-200. */
        scratch[k]=(Point){x,y};
        if(!isfinite(scratch[k].x)||!isfinite(scratch[k].y)){
            formula_fail(&error,0,"coordinate overflow");lab_error(error,lab.applied);lab.runtime_error=1;return -1;
        }
    }
    memcpy(output,scratch,n*sizeof *output);lab.preview_count=n;lab.preview_phase=phase;return 1;
}
static void lab_refresh(void){FormulaError error;lab.panel_valid=formula_compile(lab.draft,&lab.panel,&error);}
static void lab_history(void){
    if(lab.undo_count&& !strcmp(lab.undo[lab.undo_at],lab.draft))return;
    lab.undo_count=lab.undo_count?lab.undo_at+1:0;
    if(lab.undo_count==16){memmove(lab.undo,lab.undo+1,15*sizeof lab.undo[0]);memmove(lab.undo_cursor,lab.undo_cursor+1,15*sizeof(int));memmove(lab.undo_anchor,lab.undo_anchor+1,15*sizeof(int));lab.undo_count--;}
    lab.undo_at=lab.undo_count++;strcpy(lab.undo[lab.undo_at],lab.draft);lab.undo_cursor[lab.undo_at]=lab.cursor;lab.undo_anchor[lab.undo_at]=lab.anchor;lab_refresh();
}
static void lab_init(void){
    observer_lab_active=0;strcpy(observer_creature,"lab-preview");lab=(CreationLab){.settings=defaults()};strcpy(lab.draft,lab_starter);lab_apply(&lab.settings);
    strcpy(lab.persisted,lab_starter);strcpy(lab.name,"Untitled rosette");lab_history();
}
static int lab_unsaved(void){return strcmp(lab.draft,lab.applied)||strcmp(lab.applied,lab.persisted);}
static void lab_focus(int focus){lab.focus=focus;if(focus==1)lab.help=0;if(focus==1)SDL_StartTextInput();else SDL_StopTextInput();lab.anchor=lab.cursor;}
static void lab_toggle(Settings *s){
    if(lab.open){lab.settings=*s;*s=lab.saved;lab.open=0;lab.browser=lab.dialog=0;lab_focus(0);}
    else {lab.saved=*s;*s=lab.settings;lab.open=1;lab_focus(0);}
    observer_lab_active=lab.open;snprintf(observer_creature,sizeof observer_creature,"%s",*lab.id?lab.id:"lab-preview");text_clear();
}
static int lab_line_start(int pos){while(pos&&lab.draft[pos-1]!='\n')pos--;return pos;}
static int lab_line_end(int pos){while(lab.draft[pos]&&lab.draft[pos]!='\n')pos++;return pos;}
static void lab_insert(const char *value){
    size_t length=strlen(lab.draft),n=strlen(value);int low=lab.cursor<lab.anchor?lab.cursor:lab.anchor,high=lab.cursor>lab.anchor?lab.cursor:lab.anchor;
    if(length-(high-low)+n>=FORMULA_TEXT){snprintf(lab.status,sizeof lab.status,"EDITOR FULL / 2047 bytes maximum");return;}
    for(size_t j=0;j<n;j++)if((unsigned char)value[j]>126||((unsigned char)value[j]<32&&value[j]!='\n')){snprintf(lab.status,sizeof lab.status,"ASCII formula text required");return;}
    memmove(lab.draft+low+n,lab.draft+high,length-high+1);memcpy(lab.draft+low,value,n);lab.cursor=lab.anchor=low+(int)n;
    snprintf(lab.status,sizeof lab.status,"DRAFT / Ctrl+S to apply");lab_history();
}
static void lab_undo(int redo){
    int next=lab.undo_at+(redo?1:-1);if(next<0||next>=lab.undo_count)return;
    lab.undo_at=next;strcpy(lab.draft,lab.undo[next]);lab.cursor=lab.undo_cursor[next];lab.anchor=lab.undo_anchor[next];lab_refresh();
    snprintf(lab.status,sizeof lab.status,"%s / draft only",redo?"REDO":"UNDO");
}
static int lab_matches(const char *name,const char *filter){
    for(const char *a=name;*a;a++){const char *b=a,*c=filter;while(*b&&*c&&tolower((unsigned char)*b)==tolower((unsigned char)*c)){b++;c++;}if(!*c)return 1;}return !*filter;
}
static int lab_list(int *indices){
    int n=0;for(int i=0;i<NATIVE_COUNT+library.count;i++){const char *name=i<NATIVE_COUNT?generator_at(i)->name:library.entries[i-NATIVE_COUNT].name;
        if(lab_matches(name,lab.filter))indices[n++]=i;}return n;
}
static const char *lab_selected(void){
    int indices[NATIVE_COUNT+LIB_CAP],n=lab_list(indices);if(!n)return "";
    if(lab.selection>=n)lab.selection=n-1;
    if(lab.selection<0)lab.selection=0;
    int index=indices[lab.selection];return index<NATIVE_COUNT?generator_at(index)->id:library.entries[index-NATIVE_COUNT].id;
}
static void lab_dialog(int type,const char *value){lab.dialog=type;snprintf(lab.field,sizeof lab.field,"%s",value);SDL_StartTextInput();}
static int lab_load(Settings *s,const char *id){
    int native=library_native(id),index=library_index(&library,id);
    if(native>=0){
        if(lab.open)lab_toggle(s);
        select_generator(s,(size_t)native);
        strcpy(lab.draft,lab_starter);strcpy(lab.applied,lab_starter);strcpy(lab.persisted,lab_starter);*lab.id=0;strcpy(lab.name,"Untitled rosette");
        FormulaError e;formula_compile(lab_starter,&lab.formula,&e);lab.error=(FormulaError){0};lab.runtime_error=0;lab.settings.t=0;lab.preview_count=0;lab.cursor=lab.anchor=0;lab.revision_count=0;lab_refresh();lab.undo_count=0;lab_history();return 1;
    }
    if(index<0){snprintf(lab.status,sizeof lab.status,"MISSING CREATURE / favorite is empty");return 0;}
    const Creature *c=&library.entries[index];Formula f;FormulaError e;
    if(!formula_compile(c->formula,&f,&e)||!lab_validate(&f,lab.open?s->points:lab.settings.points,0,&e)){lab_error(e,c->formula);return 0;}
    if(!lab.open)lab_toggle(s);
    s->t=0;lab.formula=f;strcpy(lab.draft,c->formula);strcpy(lab.applied,c->formula);strcpy(lab.persisted,c->formula);strcpy(lab.id,c->id);strcpy(lab.name,c->name);
    lab.error=(FormulaError){0};lab.runtime_error=0;lab.browser=lab.searching=0;lab.cursor=lab.anchor=0;lab.revision_count=lab.undo_count=0;lab.revision++;lab_focus(0);lab_history();snprintf(lab.status,sizeof lab.status,"LOADED / persisted creature");return 1;
}
static void lab_request_load(Settings *s,const char *id){
    if(!*id){snprintf(lab.status,sizeof lab.status,"EMPTY FAVORITE / B opens Library");lab.notice_until=monotonic_seconds()+3;return;}
    if(lab_unsaved()){snprintf(lab.pending_id,sizeof lab.pending_id,"%s",id);lab.pending_quit=0;lab_dialog(6,"");}
    else lab_load(s,id);
}
static int lab_confirm_quit(Settings *s){
    if(!lab_unsaved())return 1;
    if(!lab.open)lab_toggle(s);
    lab.browser=lab.searching=0;lab.pending_quit=1;lab_dialog(6,"");return 0;
}
static void lab_save(int as_new){
    if(strcmp(lab.draft,lab.applied)){snprintf(lab.status,sizeof lab.status,"SAVE REFUSED / Apply draft first (Ctrl+S)");return;}
    if(!as_new&&*lab.id){
        if(library_save(&library,lab.id,lab.name,lab.applied,0)){strcpy(lab.persisted,lab.applied);snprintf(lab.status,sizeof lab.status,"SAVED / %s",lab.name);}
        else snprintf(lab.status,sizeof lab.status,"%s",library.status);
    }else lab_dialog(as_new?2:1,lab.name);
}
static int lab_parameter(double value){
    if(!lab.panel_valid||lab.param>=lab.panel.param_count)return 0;
    int j=lab.param;if(!isfinite(value)||value<lab.panel.min[j]||value>lab.panel.max[j]){snprintf(lab.status,sizeof lab.status,"PARAMETER OUT OF RANGE / no clamp");return 0;}
    lab.cursor=lab.panel.value_end[j];lab.anchor=lab.panel.value_start[j];char number[40];snprintf(number,sizeof number,"%.17g",value);lab_insert(number);lab_focus(2);return 1;
}
static void lab_text(const char *value){
    if(lab.suppress_search_text){lab.suppress_search_text=0;if(!strcmp(value,"/"))return;}
    if(lab.dialog||lab.searching){
        char *out=lab.dialog?lab.field:lab.filter;size_t cap=lab.dialog?sizeof lab.field:sizeof lab.filter,n=strlen(out),add=strlen(value);
        for(size_t j=0;j<add;j++)if((unsigned char)value[j]<32||(unsigned char)value[j]>126)return;
        if(n+add<cap){memcpy(out+n,value,add+1);if(lab.searching)lab.selection=0;}return;
    }
    if(lab.focus==1)lab_insert(value);
}
static int lab_key(Settings *s,const SDL_KeyboardEvent *e){
    SDL_Keycode k=e->keysym.sym;int ctrl=e->keysym.mod&KMOD_CTRL,shift=e->keysym.mod&KMOD_SHIFT;
    if(lab.dialog){
        if(e->repeat)return 1;
        if(k==SDLK_ESCAPE){lab.dialog=0;if(lab.focus!=1&&!lab.searching)SDL_StopTextInput();return 1;}
        if(lab.dialog==6){if(k==SDLK_d&&!(e->keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI|KMOD_MODE))){lab.dialog=0;if(lab.pending_quit)lab.quit_confirmed=1;else lab_load(s,lab.pending_id);}return 1;}
        if(lab.dialog==4){if(k==SDLK_y&&!(e->keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI|KMOD_MODE))){char id[LIB_ID];snprintf(id,sizeof id,"%s",lab_selected());if(library_delete(&library,id)){if(!strcmp(id,lab.id)){*lab.id=0;*lab.persisted=0;}lab.dialog=0;}snprintf(lab.status,sizeof lab.status,"%s",library.status);}return 1;}
        if(ctrl&&k==SDLK_a){*lab.field=0;return 1;}
        if(k==SDLK_BACKSPACE){size_t n=strlen(lab.field);if(n)lab.field[n-1]=0;return 1;}
        if(k==SDLK_RETURN){
            int ok=0,type=lab.dialog;
            if(lab.dialog==5){double v;if(number(lab.field,&v,-DBL_MAX,DBL_MAX)){ok=lab_parameter(v);}else snprintf(lab.status,sizeof lab.status,"INVALID FINITE PARAMETER VALUE");}
            else if(lab.dialog==3){char id[LIB_ID];snprintf(id,sizeof id,"%s",lab_selected());int index=library_index(&library,id);
                if(index>=0&&library_save(&library,id,lab.field,library.entries[index].formula,0)){if(!strcmp(lab.id,id))snprintf(lab.name,sizeof lab.name,"%s",lab.field);ok=1;}}
            else if(library_save(&library,lab.id,lab.field,lab.applied,lab.dialog==2)){snprintf(lab.name,sizeof lab.name,"%s",lab.field);strcpy(lab.persisted,lab.applied);ok=1;}
            if(ok){lab.dialog=0;if(type!=5)snprintf(lab.status,sizeof lab.status,"%s",type==3&&!strstr(library.status,"sync unavailable")?"RENAMED / stable ID unchanged":library.status);if(lab.focus!=1&&!lab.searching)SDL_StopTextInput();}
            else if(lab.dialog!=5)snprintf(lab.status,sizeof lab.status,"%s",library.status);
        }return 1;
    }
    if(lab.browser){
        if(lab.help&&k!=SDLK_h&&k!=SDLK_ESCAPE)return 1;
        if(lab.help&&k==SDLK_ESCAPE){lab.help=0;return 1;}
        if(k==SDLK_ESCAPE&&!e->repeat){if(lab.searching){lab.searching=0;SDL_StopTextInput();}else lab.browser=0;return 1;}
        if(lab.searching){
            if(ctrl&&k==SDLK_a){*lab.filter=0;lab.selection=0;return 1;}
            if(k==SDLK_BACKSPACE){size_t n=strlen(lab.filter);if(n)lab.filter[n-1]=0;lab.selection=0;}
            if(k==SDLK_RETURN){lab.searching=0;SDL_StopTextInput();}return 1;
        }
        if(e->repeat&&k!=SDLK_UP&&k!=SDLK_DOWN)return 1;
        if(k==SDLK_UP)lab.selection--;else if(k==SDLK_DOWN)lab.selection++;
        else if(k==SDLK_SLASH){lab.searching=1;lab.suppress_search_text=1;SDL_StartTextInput();}
        else if(k==SDLK_b)lab.browser=0;
        else if(k==SDLK_RETURN)lab_request_load(s,lab_selected());
        else if(k==SDLK_F4){int index=library_index(&library,lab_selected());if(index>=0)lab_dialog(3,library.entries[index].name);else snprintf(lab.status,sizeof lab.status,"NATIVE REFERENCE / read-only");}
        else if(k==SDLK_F8){if(library_index(&library,lab_selected())>=0)lab_dialog(4,"");else snprintf(lab.status,sizeof lab.status,"NATIVE REFERENCE / cannot delete");}
        else if(ctrl&&((k>=SDLK_0&&k<=SDLK_9))){int slot=k==SDLK_0?9:(int)(k-SDLK_1);library_favorite(&library,slot,shift?"":lab_selected());snprintf(lab.status,sizeof lab.status,"%s",library.status);}
        else if(k==SDLK_h)lab.help=!lab.help;
        return 1;
    }
    if(!lab.open){
        if(k==SDLK_l&&!e->repeat&&!(e->keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI))){lab_toggle(s);return 1;}
        if(k==SDLK_b&&!e->repeat&&!(e->keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI))){lab_toggle(s);lab.browser=1;return 1;}
        if(k>=SDLK_0&&k<=SDLK_9&&!e->repeat&&!(e->keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI|KMOD_SHIFT|KMOD_MODE))){
            int slot=k==SDLK_0?9:(int)(k-SDLK_1);const char *id=library.favorites[slot];
            /* Keep the original event/telemetry path for default native slots. */
            if(library_native(id)==slot&&slot<NATIVE_COUNT)return 0;
            lab_request_load(s,id);return 1;
        }return 0;
    }
    if(k==SDLK_ESCAPE&&!e->repeat){if(lab.focus)lab_focus(0);else lab_toggle(s);return 1;}
    if(k==SDLK_TAB&&!e->repeat){lab_focus(shift?(lab.focus==0?1:lab.focus==1?2:0):(lab.focus==0?2:lab.focus==2?1:0));return 1;}
    if(ctrl&&!e->repeat){
        if(k==SDLK_s){if(shift)lab_save(1);else {lab_apply(s);lab_refresh();}return 1;}
        if(k==SDLK_d){lab_save(0);return 1;}
        if(k==SDLK_z){lab_undo(shift);return 1;}if(k==SDLK_y){lab_undo(1);return 1;}
        if(k==SDLK_r){lab.anchor=0;lab.cursor=(int)strlen(lab.draft);lab_insert(lab_starter);s->t=0;lab_apply(s);return 1;}
    }
    if(k==SDLK_F6&&!e->repeat){lab.anchor=0;lab.cursor=(int)strlen(lab.draft);lab_insert(lab.applied);lab.error=(FormulaError){0};snprintf(lab.status,sizeof lab.status,"RESTORED LAST APPLIED / runtime: Ctrl+R");return 1;}
    if(k==SDLK_F7&&!e->repeat&&lab.revision_count){int count=lab.revision_count-1;char previous[FORMULA_TEXT];strcpy(previous,lab.revisions[count]);lab.anchor=0;lab.cursor=(int)strlen(lab.draft);lab_insert(previous);s->t=0;lab.restoring_revision=1;int ok=lab_apply(s);lab.restoring_revision=0;if(ok)lab.revision_count=count;return 1;}
    if(lab.focus==2){
        if(k==SDLK_UP&&lab.param>0)lab.param--;
        if(k==SDLK_DOWN&&lab.param+1<lab.panel.param_count)lab.param++;
        if(lab.panel_valid&&lab.param<lab.panel.param_count){int j=lab.param;
            if(k==SDLK_LEFT||k==SDLK_RIGHT)lab_parameter(lab.panel.params[j]+(k==SDLK_LEFT?-1:1)*lab.panel.step[j]*(shift?.1:1));
            if(k==SDLK_RETURN&&!e->repeat){char value[40];snprintf(value,sizeof value,"%.17g",lab.panel.params[j]);lab_dialog(5,value);}}
        return 1;
    }
    if(lab.focus==1){
        int length=(int)strlen(lab.draft),start=lab_line_start(lab.cursor),end=lab_line_end(lab.cursor),move=0;
        if(ctrl){
            if(k==SDLK_a){lab.anchor=0;lab.cursor=length;return 1;}
            if(k==SDLK_c||k==SDLK_x){int low=lab.cursor<lab.anchor?lab.cursor:lab.anchor,high=lab.cursor>lab.anchor?lab.cursor:lab.anchor;char copy[FORMULA_TEXT];memcpy(copy,lab.draft+low,high-low);copy[high-low]=0;SDL_SetClipboardText(copy);if(k==SDLK_x)lab_insert("");return 1;}
            if(k==SDLK_v){char *value=SDL_GetClipboardText();if(value){lab_insert(value);SDL_free(value);}return 1;}
        }
        switch(k){
        case SDLK_LEFT:if(lab.cursor)lab.cursor--;move=1;break;case SDLK_RIGHT:if(lab.cursor<length)lab.cursor++;move=1;break;
        case SDLK_HOME:lab.cursor=ctrl?0:start;move=1;break;case SDLK_END:lab.cursor=ctrl?length:end;move=1;break;
        case SDLK_UP:if(start){int previous=lab_line_start(start-1);lab.cursor=previous+(int)fmin(lab.cursor-start,start-1-previous);}move=1;break;
        case SDLK_DOWN:if(end<length){int next=end+1;lab.cursor=next+(int)fmin(lab.cursor-start,lab_line_end(next)-next);}move=1;break;
        case SDLK_PAGEUP:case SDLK_PAGEDOWN:{int column=lab.cursor-start;for(int j=0;j<10;j++){int a=lab_line_start(lab.cursor),b=lab_line_end(lab.cursor);if(k==SDLK_PAGEUP&&a){int prev=lab_line_start(a-1);lab.cursor=prev+(int)fmin(column,a-1-prev);}else if(k==SDLK_PAGEDOWN&&b<length){lab.cursor=b+1+(int)fmin(column,lab_line_end(b+1)-b-1);}else break;}move=1;break;}
        case SDLK_BACKSPACE:if(lab.cursor==lab.anchor&&lab.cursor)lab.anchor--;lab_insert("");break;
        case SDLK_DELETE:if(lab.cursor==lab.anchor&&lab.cursor<length)lab.anchor++;lab_insert("");break;
        case SDLK_RETURN:if(!ctrl)lab_insert("\n");break;default:break;
        }if(move&&!shift)lab.anchor=lab.cursor;return 1;
    }
    if(!e->repeat){
        if(k==SDLK_l)lab_toggle(s);else if(k==SDLK_b){lab.browser=1;lab.searching=0;}
        else if(k==SDLK_h)lab.help=!lab.help;
        else if(k==SDLK_q){if(lab_confirm_quit(s))lab.quit_confirmed=1;}
        else if(k==SDLK_SPACE)s->paused=!s->paused;
        else if(k==SDLK_UP||k==SDLK_DOWN||k==SDLK_PLUS||k==SDLK_EQUALS||k==SDLK_MINUS||k==SDLK_KP_PLUS||k==SDLK_KP_MINUS)input(s,k);
    }return 1;
}
static Layout lab_layout(int w,int h){
    int u=ui_scale,wide=w/u>=1100&&h/u>=640;
    if(!wide)return (Layout){{38*u,32*u,1,1},1,1,0};
    int right=(w/u*52/100)*u;return (Layout){{38*u,32*u,right-54*u,h-110*u},0,0,0};
}
static void lab_draw(SDL_Renderer *r,int w,int h,Settings s){
    int u=ui_scale;
    if(w/u<1100||h/u<640){text(r,8*u,7*u,"CREATION LAB / EXPAND TO 1100 x 640",warn);text(r,8*u,32*u,"Esc back / draft retained",dim);
        if(lab.dialog)text(r,8*u,56*u,lab.dialog==6?"UNSAVED / D discard / Esc cancel":"DIALOG / Esc cancel / expand",warn);
        return;}
    int left=(w/u*52/100)*u,width=w-left-12*u,ch=0,chh=0;TTF_SizeText(font,"M",&ch,&chh);if(ch<1)ch=8*u;
    int step=chh>18*u?chh:18*u,cols=(width-12*u)/ch;
    SDL_Rect bay={left,7*u,width,h-14*u};SDL_RenderSetClipRect(r,&bay);
    char label[256];snprintf(label,sizeof label,"%s / %s",lab.browser?"CREATURE LIBRARY":"CREATION LAB",lab.name);text(r,left,7*u,label,fg);
    snprintf(label,sizeof label,"DRAFT %s / APPLIED r%u / %s",strcmp(lab.draft,lab.applied)?"MODIFIED":"=APPLIED",lab.revision,!*lab.id?"NEW / NOT STORED":strcmp(lab.applied,lab.persisted)?"UNSAVED":"PERSISTED");text(r,left,29*u,label,lab_unsaved()?warn:dim);
    text(r,left,51*u,lab.browser?(lab.searching?"SEARCH INPUT / Enter done / Esc back":"LIBRARY / Enter load / B back"):lab.focus==1?"EDITOR / Tab next / Ctrl+S apply":lab.focus==2?"PARAMETERS / arrows adjust / Enter value":"FIELD / Tab focus / B library / H help",fg);
    int editor_y=82*u;
    if(lab.browser){
        snprintf(label,sizeof label,"FILTER: %s%s",lab.filter,lab.searching?"_":"");text(r,left,77*u,label,lab.searching?warn:dim);
        int indices[NATIVE_COUNT+LIB_CAP],count=lab_list(indices);lab_selected();int rows=(h-250*u)/step,first=lab.selection>=rows?lab.selection-rows+1:0;
        for(int j=first;j<count&&j<first+rows;j++){
            int index=indices[j];const char *name=index<NATIVE_COUNT?generator_at(index)->name:library.entries[index-NATIVE_COUNT].name;
            const char *id=index<NATIVE_COUNT?generator_at(index)->id:library.entries[index-NATIVE_COUNT].id;char slots[11];int slotcount=0;
            for(int k=0;k<10;k++)if(!strcmp(library.favorites[k],id))slots[slotcount++]=k==9?'0':'1'+k;
            slots[slotcount]=0;
            snprintf(label,sizeof label,"%c %.*s [%s] %s %s",j==lab.selection?'>':' ',cols-25,name,index<NATIVE_COUNT?"NATIVE":"FORMULA",slots,index<NATIVE_COUNT?"":id+26);
            text(r,left,102*u+(j-first)*step,label,j==lab.selection?fg:dim);
        }
        if(!count)text(r,left,102*u,"NO MATCHES / Esc then / to change search",warn);
        int y=h-126*u;
        text(r,left,y,"/ search / F4 rename / F8 delete",dim);
        text(r,left,y+20*u,"Ctrl+1..0 assign / +Shift clear slot",dim);
        text(r,left,y+40*u,library_native(lab_selected())>=0?"NATIVE REFERENCE / not formula editable":"Enter loads editable formula",dim);
        text(r,left,y+60*u,library.status,dim);
        text(r,left,y+80*u,lab.status,lab.error.message[0]?warn:fg);
    }else{
        Formula *panel=lab.panel_valid?&lab.panel:&lab.formula;
        text(r,left,editor_y,lab.panel_valid?"FORMULA PARAMETERS / edit then Ctrl+S":"INVALID DRAFT / parameters disabled",lab.panel_valid?dim:warn);editor_y+=22*u;
        for(int j=0;j<panel->param_count;j++){
            snprintf(label,sizeof label,"%c %-12s % .8g  [%.5g,%.5g]",lab.focus==2&&j==lab.param?'>':' ',panel->names[j],panel->params[j],panel->min[j],panel->max[j]);
            text(r,left,editor_y,label,lab.focus==2&&j==lab.param?fg:dim);editor_y+=step;
        }
        editor_y+=8*u;int rows=(h-editor_y-150*u)/step;if(rows<3)rows=3;
        SDL_Rect editor={left,editor_y,width,rows*step};color(r,lab.focus==1?fg:dim);SDL_RenderDrawRect(r,&editor);SDL_RenderSetClipRect(r,&editor);
        if(lab.help){
            const char *help[]={"H HELP / Esc unfocus / Tab next panel","Ctrl+S APPLY / Ctrl+D SAVE to library","Ctrl+Shift+S SAVE AS (new stable ID)","F6 last applied / F7 previous valid","Ctrl+R starter / Ctrl+Z,Y undo,redo","Shift+arrows select / Ctrl+A all","Ctrl+C,X,V clipboard / Home,End,Page","param size = 100 [1,180,0.5]","x = size*cos(u) / y = size*sin(u)","i=0..N-1 / u=2*pi*i/N / t=phase","+ - * / sin cos abs sqrt pow min max","Try waves=7; sin(3*u+t) for lobes","pow/sqrt/division: domain may fail"};
            for(int j=0;j<13&&j<rows;j++)text(r,left+3*u,editor_y+2*u+j*step,help[j],dim);
        }else{
            int cursor_line=0,col=0;for(int j=0;j<lab.cursor;j++){if(lab.draft[j]=='\n'){cursor_line++;col=0;}else col++;}
            int first=cursor_line>=rows?cursor_line-rows+1:0,offset=col>=cols?col-cols+1:0;
            int pos=0,line=0,low=lab.cursor<lab.anchor?lab.cursor:lab.anchor,high=lab.cursor>lab.anchor?lab.cursor:lab.anchor;
            do{
                int end=lab_line_end(pos);
                if(line>=first&&line<first+rows){
                    int len=end-pos,skip=offset<len?offset:len,count=len-skip;if(count>cols)count=cols;if(count>254)count=254;
                    int a=low>pos+skip?low:pos+skip,b=high<pos+skip+count?high:pos+skip+count;
                    if(b>a){SDL_Rect selected={left+3*u+(a-pos-skip)*ch,editor_y+2*u+(line-first)*step,(b-a)*ch,step};color(r,grid_major);SDL_RenderFillRect(r,&selected);}
                    char row[256];memcpy(row,lab.draft+pos+skip,count);row[count]=0;if(count)text(r,left+3*u,editor_y+2*u+(line-first)*step,row,fg);
                }
                if(!lab.draft[end])break;
                pos=end+1;line++;
            }while(1);
            if(lab.focus==1){int x=left+3*u+(col-offset)*ch,y=editor_y+2*u+(cursor_line-first)*step;color(r,warn);SDL_RenderDrawLine(r,x,y,x,y+step-2*u);}
        }
        SDL_RenderSetClipRect(r,&bay);int y=h-128*u;
        /* Fixed status zone, wrapped to avoid losing the error cause at minimum width. */
        int length=(int)strlen(lab.status);for(int j=0;j<3&&j*cols<length;j++){char row[256];int n=length-j*cols;if(n>cols)n=cols;if(n>254)n=254;memcpy(row,lab.status+j*cols,n);row[n]=0;text(r,left,y+j*18*u,row,lab.error.message[0]||lab.runtime_error?warn:fg);}
        text(r,left,y+57*u,lab.runtime_error?"PREVIEW FROZEN / LAST VALID FRAME":lab.error.message[0]?"PREVIEW: LAST VALID VERSION":"PREVIEW: APPLIED (not draft)",lab.runtime_error||lab.error.message[0]?warn:dim);
        text(r,left,y+78*u,"Ctrl+S Apply / Ctrl+D Save / B Library",dim);
        text(r,left,y+99*u,"F6 Rollback / Ctrl+R Starter / H Help",dim);
    }
    if(lab.browser&&lab.help){
        SDL_Rect help={left,76*u,width,230*u};color(r,bg);SDL_RenderFillRect(r,&help);color(r,dim);SDL_RenderDrawRect(r,&help);
        const char *lines[]={"LIBRARY HELP / H or Esc closes help","Up/Down select / Enter load","/ search names / Ctrl+A clear filter","F4 rename / same ID, duplicate names OK","F8 delete / Y confirm / native protected","Ctrl+1..0 assign selected to favorite","Ctrl+Shift+1..0 clear favorite slot","B back to Lab / then Ctrl+D Save","Native references cannot be formula edited","Formula saves require valid phase 0","B back / selected does not mean loaded"};
        for(int j=0;j<11;j++)text(r,left+3*u,82*u+j*18*u,lines[j],dim);
    }
    if(lab.dialog){
        SDL_Rect dialog={left,90*u,width,172*u};color(r,bg);SDL_RenderFillRect(r,&dialog);color(r,warn);SDL_RenderDrawRect(r,&dialog);
        const char *title=lab.dialog==1?"SAVE CREATURE / name":lab.dialog==2?"SAVE AS NEW / name":lab.dialog==3?"RENAME / same stable ID":lab.dialog==4?"DELETE CREATURE + favorite links?":lab.dialog==5?"PARAMETER / exact numeric value":"UNSAVED CHANGES / discard?";
        text(r,left+8*u,101*u,title,warn);
        text(r,left+8*u,129*u,lab.dialog==4?"Y confirm / Esc cancel":lab.dialog==6?"D discard / Esc cancel (save first)":lab.field,fg);
        text(r,left+8*u,157*u,lab.dialog==4||lab.dialog==6?"Esc cancel / plain confirmation key only":"Enter confirm / Ctrl+A replace / Esc cancel",dim);
        text(r,left+8*u,187*u,lab.status,warn);
    }
    SDL_RenderSetClipRect(r,NULL);
    static char state[128];static double next;double now=monotonic_seconds();
    if(now>=next||s.paused){snprintf(state,sizeof state,"%s / N %d / Z %.2f / PHASE %.3f",s.paused?"PAUSED":"RUN",s.points,s.zoom,s.t);next=now+.125;}
    SDL_Rect footer={8*u,h-60*u,left-16*u,60*u};SDL_RenderSetClipRect(r,&footer);text(r,8*u,h-55*u,state,fg);text(r,8*u,h-34*u,"Tab FOCUS / L CLOSE / B LIBRARY / H HELP",dim);SDL_RenderSetClipRect(r,NULL);
}
#endif
