#ifndef OCL_IS_DEF
#define OCL_IS_DEF

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#include <OpenGL/CGLContext.h>
#include <OpenGL/CGLCurrent.h>
#else
#include <CL/opencl.h>
#include <GL/glx.h>
#endif

#ifndef NOSDL
#include <SDL_opengl.h>
#endif

void ocl_init (void);
void ocl_map_textures (GLuint texid);
void ocl_send_image (unsigned *image);
void ocl_retrieve_image (unsigned *image);
unsigned ocl_compute (unsigned nb_iter);
void ocl_wait (void);
void ocl_update_texture (void);
size_t ocl_get_max_workgroup_size (void);

#define check(err, ...)                                                        \
  do {                                                                         \
    if (err != CL_SUCCESS) {                                                   \
      fprintf (stderr, "(%d) Error: " __VA_ARGS__ "\n", err);                  \
      exit (EXIT_FAILURE);                                                     \
    }                                                                          \
  } while (0)

extern unsigned SIZE, TILE, TILEX, TILEY;
extern cl_kernel compute_kernel;
extern cl_command_queue queue;
extern cl_mem cur_buffer, next_buffer;

#endif
