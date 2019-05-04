
#ifndef GRAPHICS_IS_DEF
#define GRAPHICS_IS_DEF

#ifndef NOSDL
#include <SDL.h>
#else
#include <stdint.h>
typedef uint32_t Uint32;
typedef uint8_t Uint8;
#endif

#include "global.h"

void graphics_init ();
void graphics_share_texture_buffers (void);
void graphics_refresh (void);
void graphics_dump_image_to_file (char *filename);
void graphics_clean (void);
int graphics_display_enabled (void);

extern Uint32 *restrict image, *restrict alt_image;

static inline Uint32 *img_cell (Uint32 *i, int l, int c)
{
  return i + l * DIM + c;
}

uint8_t* img_bit_array;
#define cur_img(y, x) (*img_cell (image, (y), (x)))
#define next_img(y, x) (*img_cell (alt_image, (y), (x)))

static inline uint8_t cur_bit(int x, int y){
  return img_bit_array[(DIM*y+x)];
}

static inline void swap_images (void)
{
  Uint32 *tmp = image;

  image     = alt_image;
  alt_image = tmp;
}

#endif
