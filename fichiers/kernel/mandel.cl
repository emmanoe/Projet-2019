#include "kernel/common.cl"


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// mandelbrot
////////////////////////////////////////////////////////////////////////////////

static unsigned mandel_iter2color (unsigned iter)
{
  unsigned r = 0, g = 0, b = 0;

  if (iter < 64) {
    r = iter * 2;    /* 0x0000 to 0x007E */
  } else if (iter < 128) {
    r = (((iter - 64) * 128) / 126) + 128;    /* 0x0080 to 0x00C0 */
  } else if (iter < 256) {
    r = (((iter - 128) * 62) / 127) + 193;    /* 0x00C1 to 0x00FF */
  } else if (iter < 512) {
    r = 255;
    g = (((iter - 256) * 62) / 255) + 1;    /* 0x01FF to 0x3FFF */
  } else if (iter < 1024) {
    r = 255;
    g = (((iter - 512) * 63) / 511) + 64;   /* 0x40FF to 0x7FFF */
  } else if (iter < 2048) {
    r = 255;
    g = (((iter - 1024) * 63) / 1023) + 128;   /* 0x80FF to 0xBFFF */
  } else {
    r = 255;
    g = (((iter - 2048) * 63) / 2047) + 192;   /* 0xC0FF to 0xFFFF */
  }

  //return 0xFFFF00FF;
    return (r << 24) | (g << 16) | (b << 8) | 255 /* alpha */;
}


__kernel void mandel (__global unsigned *img,
		      float leftX, float xstep,
		      float topY, float ystep,
		      unsigned MAX_ITERATIONS)
{
  int i = get_global_id (1);
  int j = get_global_id (0);

  float xc = leftX + xstep * j;
  float yc = topY - ystep * i;
  float x = 0.0, y = 0.0;	/* Z = X+I*Y */

  unsigned iter;

  // Pour chaque pixel, on calcule les termes d'une suite, et on
  // s'arrête lorsque |Z| > 2 ou lorsqu'on atteint MAX_ITERATIONS
  for (iter = 0; iter < MAX_ITERATIONS; iter++) {
    float x2 = x*x;
    float y2 = y*y;

    /* Stop iterations when |Z| > 2 */
    if (x2 + y2 > 4.0)
      break;
	
    float twoxy = (float)2.0 * x * y;
    /* Z = Z^2 + C */
    x = x2 - y2 + xc;
    y = twoxy + yc;
  }

  img [i * DIM + j] = (iter < MAX_ITERATIONS)
    ? mandel_iter2color (iter)
    : 0x000000FF; // black
}
