#ifndef PHOSPHOR_GENERATOR_H
#define PHOSPHOR_GENERATOR_H
#include <stddef.h>
#include "field.h"
#include "creature.h"

/* Internal, compiled-in frame interface. Caller supplies N Point slots,
   1 <= N <= MAX_POINTS; every slot is written exactly once. */
typedef struct {
    double center_x,center_y,half_span,grid_step;
} GeneratorView;
typedef struct {
    const char *id,*name;
    GeneratorView view;
    int reference_points;
    void (*generate)(int N,double phase,Point *out);
} Generator;

/* Compiled-in native creatures. All are original Phosphor Field work. */
#define NATIVE_COUNT 1
static const Generator generators[NATIVE_COUNT]={
    {"pf-medusa","Medusa",{0,0,200,25},10000,medusa_generate}
};
static inline size_t generator_count(void){return sizeof generators/sizeof generators[0];}
static inline const Generator *generator_at(size_t index){
    return index<generator_count()?&generators[index]:NULL;
}
#endif
