
#ifndef COMPUTE_IS_DEF
#define COMPUTE_IS_DEF

#include <dlfcn.h>

#ifdef __APPLE__
#define DLSYM_FLAG RTLD_SELF
#else
#define DLSYM_FLAG NULL
#endif

typedef void (*void_func_t) (void);
typedef unsigned (*int_func_t) (unsigned);
typedef void (*draw_func_t)(char *);

extern void_func_t the_first_touch;
extern void_func_t the_init;
extern draw_func_t the_draw;
extern void_func_t the_finalize;
extern int_func_t the_compute;

extern unsigned opencl_used;
extern char *version;

unsigned get_nb_cores (void);

#endif
