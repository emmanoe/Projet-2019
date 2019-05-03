#ifdef NOSDL

unsigned do_monitoring  = 0;
unsigned MONITOR_WIDTH  = 0;
unsigned MONITOR_HEIGHT = 0;

void monitoring_init (int x, int y)
{
}
void monitoring_clean ()
{
}

void monitoring_begin ()
{
}
void monitoring_end ()
{
}
void __monitoring_add_tile (int x, int y, int width, int height, int color)
{
}

#else

#include <SDL_image.h>
#include <SDL_opengl.h>
#include <stdio.h>

#include "constants.h"
#include "debug.h"
#include "error.h"
#include "global.h"
#include "monitoring.h"

unsigned do_monitoring  = 0;
unsigned MONITOR_WIDTH  = 0;
unsigned MONITOR_HEIGHT = 0;

static SDL_Window *win      = NULL;
static SDL_Renderer *ren    = NULL;
static SDL_Surface *surface = NULL;
static SDL_Texture *texture = NULL;

Uint32 *restrict trace = NULL;

void monitoring_init (int x, int y)
{
  if (!display)
    return;

  MONITOR_WIDTH  = 3 * WIN_WIDTH / 8;
  MONITOR_HEIGHT = 3 * WIN_HEIGHT / 8;

  PRINT_DEBUG ('m', "Monitoring window: %u x %u\n", MONITOR_WIDTH,
               MONITOR_HEIGHT);

  // Création de la fenêtre sur l'écran
  win = SDL_CreateWindow ("Monitor", x, y, MONITOR_WIDTH, MONITOR_HEIGHT,
                          SDL_WINDOW_SHOWN);
  if (win == NULL)
    exit_with_error ("SDL_CreateWindow");

  // Initialisation du moteur de rendu
  ren = SDL_CreateRenderer (
      win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (ren == NULL)
    exit_with_error ("SDL_CreateRenderer");

  // Creation d'une surface capable de mémoire quel processeur/thread a
  // travaillé sur quel pixel
  trace = malloc (DIM * DIM * sizeof (Uint32));

  Uint32 rmask = 0xff000000;
  Uint32 gmask = 0x00ff0000;
  Uint32 bmask = 0x0000ff00;
  Uint32 amask = 0x000000ff;

  surface = SDL_CreateRGBSurfaceFrom (
      trace, DIM, DIM, 32, DIM * sizeof (Uint32), rmask, gmask, bmask, amask);
  if (surface == NULL)
    exit_with_error ("SDL_CreateRGBSurfaceFrom () failed: %s", SDL_GetError ());

  // Création d'une texture DIM x DIM sur la carte graphique
  texture = SDL_CreateTexture (ren, SDL_PIXELFORMAT_RGBA32,
                               SDL_TEXTUREACCESS_STATIC, DIM, DIM);
  if (texture == NULL)
    exit_with_error ("SDL_CreateTexture failed: %s", SDL_GetError ());

  monitoring_begin ();
}

void monitoring_begin ()
{
  if (!display)
    return;

  bzero (trace, DIM * DIM * sizeof (Uint32));
}

#define MAX_COLORS 12

static Uint32 colors[MAX_COLORS] = {
    0xFFFF00FF, // Yellow
    0xFF0000FF, // Red
    0x00FF00FF, // Green
    0xFFFFFFFF, // White
    0xCCCCCCFF, // Grey
    0xAE4AFFFF, // Purple
    0x4B9447FF, // Dark green
    0xFFBFF7FF, // Rose pale
    0x0000FFFF, // Blue
    0x00FFFFFF, // Cyan
    0xFFD591FF, // Cream
    0xCFFFBFFF  // Vert pale
};

void __monitoring_add_tile (int x, int y, int width, int height, int color)
{
  SDL_Rect dst;

  if (!display)
    return;

  dst.x = x;
  dst.y = y;
  dst.w = width;
  dst.h = height;

  // La tuile est dessinée comme un rectangle plein sur la surface
  SDL_FillRect (surface, &dst, colors[color % MAX_COLORS]);
}

void monitoring_end ()
{
  if (!display)
    return;

  SDL_Rect src, dst;

  SDL_GL_BindTexture (texture, NULL, NULL);

  glTexSubImage2D (GL_TEXTURE_2D, 0, /* mipmap level */
                   0, 0,             /* x, y */
                   DIM, DIM,         /* width, height */
                   GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, trace);

  src.x = 0;
  src.y = 0;
  src.w = DIM;
  src.h = DIM;

  // On redimensionne l'image pour qu'elle occupe toute la fenêtre
  dst.x = 0;
  dst.y = 0;
  dst.w = MONITOR_WIDTH;
  dst.h = MONITOR_HEIGHT;

  SDL_RenderClear (ren);

  SDL_RenderCopy (ren, texture, &src, &dst);

  SDL_RenderPresent (ren);
}

void monitoring_clean ()
{
  if (!display)
    return;

  if (ren != NULL)
    SDL_DestroyRenderer (ren);
  else
    return;

  if (win != NULL)
    SDL_DestroyWindow (win);
  else
    return;

  if (surface != NULL)
    SDL_FreeSurface (surface);

  if (texture != NULL)
    SDL_DestroyTexture (texture);
}

#endif
