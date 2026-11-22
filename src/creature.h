#ifndef PHOSPHOR_CREATURE_H
#define PHOSPHOR_CREATURE_H
/* PF-MEDUSA — original Phosphor Field creature (RetroTools, 2026).
   An explicit 3D model, not a 2D formula warp: a Fibonacci-sampled bell,
   an asymmetric swim beat, delay-coupled tentacles, ruffled oral arms and
   a bioluminescent density packet, tumbled in 3D and projected with mild
   perspective. Pure function of (N, phase): no state, no allocation.
   Body frame: X right, Y down, Z towards the viewer; view centre (0,0). */
#include <math.h>
#include "field.h"

#define MEDUSA_TAU 6.283185307179586
#define MEDUSA_GOLDEN 2.399963229728653   /* pi*(3-sqrt(5)) */
#define MEDUSA_BELL_ANGLE 1.25            /* polar extent of the bell [rad] */
#define MEDUSA_TENTACLES 28
#define MEDUSA_ARMS 4

/* Deterministic per-index jitter in [0,1). */
static double medusa_hash(unsigned n){
    n*=2654435761u;n^=n>>15;n*=2246822519u;n^=n>>13;
    return (double)(n&0xffffffu)/16777216.0;
}
/* Swim beat in [0,1]: fast contraction (sqrt-warped onset), slow release. */
static double medusa_beat(double t){
    double u=t/MEDUSA_TAU;u-=floor(u);
    double s=sin(3.141592653589793*sqrt(u));
    return s*s;
}
/* Bioluminescent packet: sample position for quantile u in [0,1] of the
   mixture density (1-g)*uniform + g*gauss(front,width), truncated to [0,1].
   Exact inverse CDF via bracketed Newton: monotone in u, continuous in
   time, and the packet fades out naturally as it leaves [0,1]. */
static double medusa_phi(double x){return .5*erfc(-x*.7071067811865476);}
typedef struct { double front,width,g,base,z; } MedusaGlow;
static MedusaGlow medusa_glow_init(double front,double width,double g){
    MedusaGlow q={front,width,g,medusa_phi(-front/width),0};
    q.z=(1-g)+g*(medusa_phi((1-front)/width)-q.base);
    return q;
}
/* Solve F(s)=u on [lo,1]; callers pass the previous (smaller) quantile's
   solution as lo, which keeps the bracket tight for sequential samples. */
static double medusa_glow(const MedusaGlow *q,double u,double lo){
    double hi=1,s=u>lo?u:lo;
    for(int it=0;it<24;it++){
        double x=(s-q->front)/q->width;
        double F=((1-q->g)*s+q->g*(medusa_phi(x)-q->base))/q->z-u;
        if(fabs(F)<1e-7)break;
        double dF=((1-q->g)+q->g*exp(-.5*x*x)*.3989422804014327/q->width)/q->z;
        if(F>0)hi=s;else lo=s;
        double next=s-F/dF;
        s=(next>lo&&next<hi)?next:.5*(lo+hi);
    }
    return s;
}
typedef struct { double x,y,z; } MedusaV;
typedef struct {
    double cy,sy,cp,sp,cr,sr;        /* yaw / pitch / roll */
    double bob,drift,beat,front,t;
} MedusaPose;

static MedusaPose medusa_pose(double t){
    MedusaPose m;
    /* Rare deep tilt (sin^5): about once a minute the body leans far enough
       to reveal the tentacle star from above, half a cycle later from below. */
    double lean=sin(.041*t),lean5=lean*lean*lean*lean*lean;
    double yaw=.19*t,pitch=.34+.10*sin(.13*t)+.62*lean5,roll=.14*sin(.29*t+1);
    m.cy=cos(yaw);m.sy=sin(yaw);m.cp=cos(pitch);m.sp=sin(pitch);
    m.cr=cos(roll);m.sr=sin(roll);
    m.beat=medusa_beat(t);
    m.bob=-16*medusa_beat(t-.9)-20;  /* lift lags the contraction */
    m.drift=9*sin(.17*t);
    double u=t/MEDUSA_TAU;u-=floor(u);
    /* One light wave per beat: apex -> rim on [0,1], rim -> tips on [1,2]. */
    m.front=-.3+2.6*u;  /* starts/ends >4 sigma outside: seamless wrap */
    m.t=t;
    return m;
}
static Point medusa_project(const MedusaPose *m,MedusaV v){
    double x=v.x*m->cy+v.z*m->sy,z=-v.x*m->sy+v.z*m->cy,y=v.y;     /* yaw */
    double y2=y*m->cp-z*m->sp,z2=y*m->sp+z*m->cp;y=y2;z=z2;         /* pitch */
    double x2=x*m->cr-y*m->sr;y=x*m->sr+y*m->cr;x=x2;               /* roll */
    double f=560/(560-z);
    return (Point){x*f+m->drift,y*f+m->bob};
}
/* Bell surface at polar fraction a in [0,1], azimuth b, beat p. */
static MedusaV medusa_bell(double a,double b,double p,double t){
    double ang=a*MEDUSA_BELL_ANGLE,a2=a*a,a6=a2*a2*a2;
    double r=82*sin(ang)*(1-.31*p*a2)
        *(1+.045*a6*cos(16*b)+.03*a2*a*sin(5*b-1.7*t));
    double y=-62*cos(ang)-9*p*(1-a2)+24*p*a2-14;
    return (MedusaV){r*cos(b),y,r*sin(b)};
}
static void medusa_generate(int N,double t,Point *out){
    MedusaPose m=medusa_pose(t);
    int nb=N*24/100,nr=N*8/100,na=N*18/100,nt=N-nb-nr-na,k=0;
    /* Bell: golden-angle phyllotaxis, area-balanced; the light ring is a
       packet in area fraction a^2, so it widens as it nears the rim. */
    double fa=m.front<0?0:(m.front>1?1:m.front),prev=0;
    MedusaGlow bell=medusa_glow_init(m.front*fabs(m.front),.02+.09*fa,.32);
    for(int j=0;j<nb;j++){
        prev=medusa_glow(&bell,(j+.5)/nb,prev);
        out[k++]=medusa_project(&m,medusa_bell(sqrt(prev),j*MEDUSA_GOLDEN,m.beat,t));
    }
    /* Margin: bright rim plus an inward velum shelf. */
    for(int j=0;j<nr;j++){
        double b=MEDUSA_TAU*(j>>1)/((nr+1)/2);
        MedusaV v=medusa_bell(1,b,m.beat,t);
        if(j&1){v.x*=.86;v.z*=.86;v.y+=3;}
        out[k++]=medusa_project(&m,v);
    }
    /* Oral arms: four twisting ruffled ribbons under the bell. */
    MedusaGlow arms=medusa_glow_init(m.front-1.05,.05,.35);
    for(int arm=0;arm<MEDUSA_ARMS;arm++){
        int count=(na+MEDUSA_ARMS-1-arm)/MEDUSA_ARMS;prev=0;
        double sway=.3*sin(.7*t+arm);
        for(int i=0;i<count;i++){
            double s=prev=medusa_glow(&arms,(i+.5)/count,prev);
            double v=2*fmod(i*.6180339887498949+.5*medusa_hash((unsigned)(i*MEDUSA_ARMS+arm)),1.0)-1;
            double lag=medusa_beat(t-1.4*s);
            double phi=arm*MEDUSA_TAU/MEDUSA_ARMS+.7*s+sway;
            double rc=5+16*s,len=118*(1-.12*lag);
            double w=13*pow(1-s,.6)*pow(s+.02,.35)+1.5;
            double across=phi+1.5707963267948966+2.6*s;
            double ruffle=6*v*v*sin(19*s+5*v-2.3*t+arm)*w/13;
            out[k++]=medusa_project(&m,(MedusaV){
                rc*cos(phi)+v*w*cos(across)+ruffle*cos(phi),
                -24+len*s+2*v*v,
                rc*sin(phi)+v*w*sin(across)+ruffle*sin(phi)});
        }
    }
    /* Tentacles: each sample follows the rim as it was 2.4*s rad ago,
       so the swim stroke travels down as a wave; tips coil in 3D. */
    for(int tn=0;tn<MEDUSA_TENTACLES;tn++){
        int count=(nt+MEDUSA_TENTACLES-1-tn)/MEDUSA_TENTACLES;prev=0;
        double h=medusa_hash((unsigned)tn+977u),g=medusa_hash((unsigned)tn+31u);
        MedusaGlow light=medusa_glow_init(m.front-1-.12*h,.045,.4);
        double b=MEDUSA_TAU*(tn+.5)/MEDUSA_TENTACLES+.05*sin(t+tn);
        double cb=cos(b),sb=sin(b),len=100+115*h;
        for(int i=0;i<count;i++){
            double s=prev=medusa_glow(&light,(i+.5)/count,prev);
            double past=medusa_beat(t-2.4*s);
            MedusaV rim=medusa_bell(1,b,past,t-2.4*s);
            double amp=3+21*pow(s,1.4);
            double w1=sin(4.5*s-1.3*t+MEDUSA_TAU*h),w2=cos(3.2*s-1.05*t+MEDUSA_TAU*g);
            double radial=-9*s+16*s*(past-.35)+.8*amp*w1,tangent=amp*w2;
            out[k++]=medusa_project(&m,(MedusaV){
                rim.x+radial*cb-tangent*sb+22*s*s*sin(.37*t+.5*s),
                rim.y+len*s*(1-.08*past)-.25*amp*(1+w1)*s,
                rim.z+radial*sb+tangent*cb});
        }
    }
}
#endif
