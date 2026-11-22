#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/generator.h"

/* PF-MEDUSA contract: registry, guarded output, finiteness, view bounds,
   bit-exact determinism, temporal continuity (incl. beat-cycle wrap) and
   monotone light-packet sampling. */
int main(void){
    assert(generator_count()==NATIVE_COUNT&&NATIVE_COUNT==1);
    const Generator *g=generator_at(0);
    assert(g&&!generator_at(1)&&!generator_at((size_t)-1));
    assert(!strcmp(g->id,"pf-medusa")&&!strcmp(g->name,"Medusa"));
    assert(g->view.center_x==0&&g->view.center_y==0);
    assert(g->view.half_span==200&&g->view.grid_step==25&&g->reference_points==10000);
    const int counts[]={1,2,3,7,29,2500,10000,12345,25000,50000};
    const double phases[]={0,.91,2.73,6.283185307179586,1e3+.5,14064.184818246,1e7};
    Point *buffer=malloc(50002*sizeof *buffer),*again=malloc(50000*sizeof *again);assert(buffer&&again);
    long checked=0;
    for(size_t n=0;n<sizeof counts/sizeof counts[0];n++)
    for(size_t t=0;t<sizeof phases/sizeof phases[0];t++){
        int N=counts[n];
        for(int i=0;i<N+2;i++)buffer[i]=(Point){NAN,NAN};
        buffer[0]=(Point){123,456};buffer[N+1]=(Point){789,321};
        g->generate(N,phases[t],buffer+1);
        assert(buffer[0].x==123&&buffer[0].y==456);
        assert(buffer[N+1].x==789&&buffer[N+1].y==321);
        g->generate(N,phases[t],again);
        for(int k=1;k<=N;k++,checked++){
            /* every slot written, finite, inside the +-200 instrument view */
            assert(isfinite(buffer[k].x)&&isfinite(buffer[k].y));
            assert(fabs(buffer[k].x)<=200&&fabs(buffer[k].y)<=200);
            assert(!memcmp(&buffer[k],&again[k-1],sizeof(Point)));
        }
    }
    /* Continuity: tiny phase steps never teleport samples, also across the
       beat-cycle boundary where the light wave wraps. */
    double worst=0;
    for(int c=0;c<40;c++){
        double base=c<20?c*6.283185307179586:c*1.37;
        for(int side=-1;side<=1;side+=2){
            double t=base+side*2e-4;
            g->generate(12000,t,buffer);g->generate(12000,t+4e-4,again);
            for(int k=0;k<12000;k++){
                double d=hypot(buffer[k].x-again[k].x,buffer[k].y-again[k].y);
                if(d>worst)worst=d;
            }
        }
    }
    assert(worst<1.0);
    /* Light packet: inverse CDF stays in [0,1] and is strictly monotone. */
    const double fronts[]={-1.5,-.1,0,.3,.99,1.2,3};
    for(size_t f=0;f<sizeof fronts/sizeof fronts[0];f++){
        MedusaGlow q=medusa_glow_init(fronts[f],.045,.4);double prev=-1,lo=0;
        for(int i=0;i<4000;i++){
            double s=lo=medusa_glow(&q,(i+.5)/4000,lo);
            assert(s>=0&&s<=1&&s>prev);prev=s;
        }
    }
    MedusaGlow none=medusa_glow_init(.5,.05,0);
    for(int i=0;i<100;i++){double u=(i+.5)/100;assert(fabs(medusa_glow(&none,u,0)-u)<1e-6);}
    /* Generation must not mutate the caller's settings or invent phase updates. */
    Settings s=defaults();advance(&s,1./60);double phase=s.t;
    g->generate(s.points,s.t,buffer+1);assert(s.t==phase&&s.points==10000);
    free(buffer);free(again);
    printf("PASS: single-entry registry, guarded output, %ld finite in-view bit-identical samples, continuity max %.4f u/4e-4 phase, monotone light packet, unchanged caller phase\n",checked,worst);
}
