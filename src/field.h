#ifndef FIELD_H
#define FIELD_H
#include <math.h>
typedef struct { double x,y; } Point;
typedef struct { int points,grid,trail,paused,help,controls; double speed,zoom,t,tau; } Settings;
static Settings defaults(void) {
    return (Settings){.points=10000,.grid=1,.trail=1,.speed=1,.zoom=1,.tau=.14};
}
static void advance(Settings *s, double dt) {
    if (!s->paused) s->t+=dt*s->speed*(3.141592653589793/80)*60;
}
#endif
