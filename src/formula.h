#ifndef PHOSPHOR_FORMULA_H
#define PHOSPHOR_FORMULA_H
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FORMULA_TEXT 2048
#define FORMULA_OPS 384
#define FORMULA_DEPTH 32
#define FORMULA_PARAMS 8
/* Postfix bytecode: fixed storage and at most FORMULA_OPS operations per point. */
typedef enum { F_NUM,F_I,F_U,F_T,F_RADIUS,F_TWIST,F_WAVES,F_ADD,F_SUB,F_MUL,F_DIV,F_NEG,F_SIN,F_COS,F_ABS,F_PARAM,F_SQRT,F_POW,F_MIN,F_MAX } FormulaOp;
typedef struct { FormulaOp op; double value; int pos; } FormulaInstruction;
typedef struct { FormulaInstruction code[FORMULA_OPS]; int count,split; double params[FORMULA_PARAMS],min[FORMULA_PARAMS],max[FORMULA_PARAMS],step[FORMULA_PARAMS];
    char names[FORMULA_PARAMS][24]; int param_count,value_start[FORMULA_PARAMS],value_end[FORMULA_PARAMS]; } Formula;
typedef struct { int pos; char message[96]; } FormulaError;
typedef struct { const char *text; int pos,depth; Formula *out; FormulaError *error; } FormulaParser;
static int formula_fail(FormulaError *e,int pos,const char *message){
    if(!e->message[0]){e->pos=pos;snprintf(e->message,sizeof e->message,"%s",message);}return 0;
}
static void formula_space(FormulaParser *p){while(isspace((unsigned char)p->text[p->pos]))p->pos++;}
static int formula_emit(FormulaParser *p,FormulaOp op,double value,int pos){
    if(p->out->count==FORMULA_OPS)return formula_fail(p->error,pos,"operation budget exceeded");
    p->out->code[p->out->count++]=(FormulaInstruction){op,value,pos};return 1;
}
static int formula_expression(FormulaParser *p,int level);
static int formula_atom(FormulaParser *p){
    formula_space(p);int start=p->pos;char c=p->text[p->pos];
    if(++p->depth>FORMULA_DEPTH){p->depth--;return formula_fail(p->error,start,"nesting limit exceeded");}
    int ok=0;
    if(c=='-'){p->pos++;ok=formula_atom(p)&&formula_emit(p,F_NEG,0,start);}
    else if(c=='('){p->pos++;ok=formula_expression(p,0);formula_space(p);
        if(ok&&p->text[p->pos]!=')')ok=formula_fail(p->error,p->pos,"expected )");
        else if(ok)p->pos++;
    }else if(isdigit((unsigned char)c)||c=='.'){
        char *end;errno=0;double value=strtod(p->text+p->pos,&end);
        if(end==p->text+p->pos||errno||!isfinite(value))ok=formula_fail(p->error,start,"invalid finite number");
        else {p->pos=(int)(end-p->text);ok=formula_emit(p,F_NUM,value,start);}
    }else if(isalpha((unsigned char)c)){
        while(isalnum((unsigned char)p->text[p->pos])||p->text[p->pos]=='_')p->pos++;
        int n=p->pos-start;const char *names[]={"i","u","t","pi","sin","cos","abs","sqrt","pow","min","max"};
        const FormulaOp ops[]={F_I,F_U,F_T,F_NUM,F_SIN,F_COS,F_ABS,F_SQRT,F_POW,F_MIN,F_MAX};int found=-1,param=-1;
        for(int j=0;j<p->out->param_count;j++)if((int)strlen(p->out->names[j])==n&&!strncmp(p->text+start,p->out->names[j],n))param=j;
        for(int j=0;j<11;j++)if((int)strlen(names[j])==n&&!strncmp(p->text+start,names[j],n))found=j;
        if(param>=0)ok=formula_emit(p,F_PARAM,param,start);
        else if(found<0)ok=formula_fail(p->error,start,"unknown variable or function");
        else if(found<4)ok=formula_emit(p,ops[found],found==3?3.14159265358979323846:0,start);
        else {formula_space(p);if(p->text[p->pos]!='(')ok=formula_fail(p->error,p->pos,"expected function (");
            else {p->pos++;ok=formula_expression(p,0);formula_space(p);
                if(ok&&found>=8){
                    if(p->text[p->pos]!=',')ok=formula_fail(p->error,p->pos,"expected second argument after comma");
                    else {p->pos++;ok=formula_expression(p,0);formula_space(p);}
                }
                if(ok&&p->text[p->pos]!=')')ok=formula_fail(p->error,p->pos,"expected )");
                else if(ok){p->pos++;ok=formula_emit(p,ops[found],0,start);}}
        }
    }else ok=formula_fail(p->error,start,"expected expression");
    p->depth--;return ok;
}
static int formula_expression(FormulaParser *p,int level){
    if(level==2)return formula_atom(p);
    if(!formula_expression(p,level+1))return 0;
    for(;;){formula_space(p);int pos=p->pos;char c=p->text[pos];
        if(!(level==0?(c=='+'||c=='-'):(c=='*'||c=='/')))return 1;
        p->pos++;if(!formula_expression(p,level+1))return 0;
        if(!formula_emit(p,c=='+'?F_ADD:c=='-'?F_SUB:c=='*'?F_MUL:F_DIV,0,pos))return 0;
    }
}
static int formula_assignment(FormulaParser *p,const char *name){
    formula_space(p);size_t n=strlen(name);
    if(strncmp(p->text+p->pos,name,n))return formula_fail(p->error,p->pos,"expected assignment in fixed order");
    p->pos+=(int)n;formula_space(p);
    if(p->text[p->pos]!='=')return formula_fail(p->error,p->pos,"expected =");
    p->pos++;return 1;
}
static int formula_compile(const char *text,Formula *out,FormulaError *error){
    *error=(FormulaError){0};*out=(Formula){0};
    if(strlen(text)>=FORMULA_TEXT)return formula_fail(error,FORMULA_TEXT-1,"text budget exceeded");
    FormulaParser p={.text=text,.out=out,.error=error};
    for(;;){
        formula_space(&p);int declared=!strncmp(text+p.pos,"param ",6);
        if(declared){p.pos+=6;formula_space(&p);}
        if(!declared&&text[p.pos]=='x'&&!isalnum((unsigned char)text[p.pos+1]))break;
        if(out->param_count==FORMULA_PARAMS)return formula_fail(error,p.pos,"parameter limit: 8");
        int j=out->param_count,start=p.pos;
        while(isalnum((unsigned char)text[p.pos])||text[p.pos]=='_')p.pos++;
        int len=p.pos-start;
        if(len<1||len>=24||!isalpha((unsigned char)text[start]))return formula_fail(error,start,"invalid parameter name");
        memcpy(out->names[j],text+start,len);out->names[j][len]=0;
        const char *reserved[]={"i","u","t","x","y","pi","sin","cos","abs","sqrt","pow","min","max","param"};
        for(int k=0;k<14;k++)if(!strcmp(out->names[j],reserved[k]))return formula_fail(error,start,"reserved parameter name");
        for(int k=0;k<j;k++)if(!strcmp(out->names[j],out->names[k]))return formula_fail(error,start,"duplicate parameter name");
        formula_space(&p);if(text[p.pos]!='=')return formula_fail(error,p.pos,"expected parameter =");p.pos++;formula_space(&p);
        out->value_start[j]=p.pos;char *end;errno=0;double v=strtod(text+p.pos,&end);
        if(end==text+p.pos||errno||!isfinite(v))return formula_fail(error,p.pos,"parameter needs finite literal");
        out->params[j]=v;p.pos=(int)(end-text);out->value_end[j]=p.pos;
        out->min[j]=-DBL_MAX;out->max[j]=DBL_MAX;out->step[j]=1;
        if(declared){
            formula_space(&p);if(text[p.pos]!='[')return formula_fail(error,p.pos,"expected [min,max,step]");p.pos++;
            double bounds[3];for(int k=0;k<3;k++){
                formula_space(&p);errno=0;bounds[k]=strtod(text+p.pos,&end);
                if(end==text+p.pos||errno||!isfinite(bounds[k]))return formula_fail(error,p.pos,"finite bound required");
                p.pos=(int)(end-text);formula_space(&p);
                if(text[p.pos]!=(k==2?']':','))return formula_fail(error,p.pos,"expected bound delimiter");
                p.pos++;
            }
            if(bounds[0]>bounds[1]||bounds[2]<=0||v<bounds[0]||v>bounds[1])return formula_fail(error,start,"invalid bounds / value outside range");
            out->min[j]=bounds[0];out->max[j]=bounds[1];out->step[j]=bounds[2];
        }
        out->param_count++;
    }
    if(!formula_assignment(&p,"x")||!formula_expression(&p,0))return 0;
    out->split=out->count;
    if(!formula_assignment(&p,"y")||!formula_expression(&p,0))return 0;
    formula_space(&p);if(text[p.pos])return formula_fail(error,p.pos,"unexpected trailing text");
    return 1;
}
static int formula_eval(const Formula *f,int i,int n,double t,double *x,double *y,FormulaError *error){
    double stack[FORMULA_OPS];int top=0;*error=(FormulaError){0};
    for(int j=0;j<f->count;j++){
        if(j==f->split){*x=stack[0];top=0;}
        FormulaInstruction in=f->code[j];double a=0,b=0,v=0;
        if((in.op>=F_ADD&&in.op<=F_DIV)||in.op==F_POW||in.op==F_MIN||in.op==F_MAX){b=stack[--top];a=stack[--top];}
        else if((in.op>=F_NEG&&in.op<=F_ABS)||in.op==F_SQRT)a=stack[--top];
        switch(in.op){
        case F_NUM:v=in.value;break;case F_I:v=i;break;case F_U:v=2*3.14159265358979323846*i/n;break;
        case F_T:v=t;break;case F_RADIUS:v=f->params[0];break;case F_TWIST:v=f->params[1];break;case F_WAVES:v=f->params[2];break;
        case F_ADD:v=a+b;break;case F_SUB:v=a-b;break;case F_MUL:v=a*b;break;
        case F_DIV:if(b==0)return formula_fail(error,in.pos,"division by zero");v=a/b;break;
        case F_PARAM:v=f->params[(int)in.value];break;
        case F_SQRT:if(a<0)return formula_fail(error,in.pos,"sqrt domain: argument < 0");v=sqrt(a);break;
        case F_POW:errno=0;v=pow(a,b);if(errno)return formula_fail(error,in.pos,"pow domain or range error");break;
        case F_MIN:v=fmin(a,b);break;case F_MAX:v=fmax(a,b);break;
        case F_NEG:v=-a;break;case F_SIN:v=sin(a);break;case F_COS:v=cos(a);break;case F_ABS:v=fabs(a);break;
        }
        if(!isfinite(v))return formula_fail(error,in.pos,"nonfinite result");
        stack[top++]=v;
    }
    *y=stack[0];return 1;
}
#endif
