#ifndef PHOSPHOR_THEME_H
#define PHOSPHOR_THEME_H
#include <SDL.h>
#include <stdint.h>
typedef struct {
    SDL_Color background,foreground,dim,warning,signal,minor,major,axis;
} Palette;
typedef struct {
    Palette palette;
    char file[4096],name_file[4096],name[80],event[32];
    uint64_t applied,pending;
    double next_poll;
    int fixed,valid,failed,initialized;
} Theme;
enum { THEME_EVENT=1, THEME_COLORS=2 };
Palette palette_fallback(void);
double palette_contrast(SDL_Color a,SDL_Color b);
int palette_parse(const char *data,Palette *out);
int theme_init(Theme *theme,int fixed,const char *file);
int theme_set_current(Theme *theme,const char *directory);
int theme_poll(Theme *theme,double now);
#endif
