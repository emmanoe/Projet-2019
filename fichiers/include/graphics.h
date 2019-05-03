
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

uint8_t bits[512];
#define cur_img(y, x) (*img_cell (image, (y), (x)))
#define next_img(y, x) (*img_cell (alt_image, (y), (x)))

static uint8_t cur_bit(){
  printf("cur_bit DEBUG\n");
  int index = 0;
  for (int x=0; x<DIM;x++)
    for (int y =0; y<DIM; y++)
      if (x > 0 && x < DIM - 1 && y > 0 && y < DIM - 1){
        for (int i = y - 1; i <= y + 1; i++)
          for (int j = x - 1; j <= x + 1; j++){
            bits[index/(8*(i+1))] |= ((cur_img (i, j) != 0) << (index%8)); //set the nth bit to 1 if it's a living neighbour
            index++;
          }
        index = 0;
      }
  return 1;
}

static inline void swap_images (void)
{
  Uint32 *tmp = image;

  image     = alt_image;
  alt_image = tmp;
}

#endif
