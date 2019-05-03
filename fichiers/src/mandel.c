
#include "compute.h"
#include "debug.h"
#include "global.h"
#include "graphics.h"
#include "monitoring.h"
#include "ocl.h"
#include "pthread_barrier.h"
#include "pthread_distrib.h"
#include "scheduler.h"

#include <omp.h>
#include <stdbool.h>

#ifdef ENABLE_VECTO
#include <immintrin.h>
#endif

#define MAX_ITERATIONS 4096
#define ZOOM_SPEED -0.01

static unsigned iteration_to_color (unsigned iter)
{
  unsigned r = 0, g = 0, b = 0;

  if (iter < MAX_ITERATIONS) {
    if (iter < 64) {
      r = iter * 2; /* 0x0000 to 0x007E */
    } else if (iter < 128) {
      r = (((iter - 64) * 128) / 126) + 128; /* 0x0080 to 0x00C0 */
    } else if (iter < 256) {
      r = (((iter - 128) * 62) / 127) + 193; /* 0x00C1 to 0x00FF */
    } else if (iter < 512) {
      r = 255;
      g = (((iter - 256) * 62) / 255) + 1; /* 0x01FF to 0x3FFF */
    } else if (iter < 1024) {
      r = 255;
      g = (((iter - 512) * 63) / 511) + 64; /* 0x40FF to 0x7FFF */
    } else if (iter < 2048) {
      r = 255;
      g = (((iter - 1024) * 63) / 1023) + 128; /* 0x80FF to 0xBFFF */
    } else {
      r = 255;
      g = (((iter - 2048) * 63) / 2047) + 192; /* 0xC0FF to 0xFFFF */
    }
  }
  return (r << 24) | (g << 16) | (b << 8) | 255 /* alpha */;
}

// Cadre initial
#if 0
// Config 1
static float leftX = -0.744;
static float rightX = -0.7439;
static float topY = .146;
static float bottomY = .1459;
#endif

#if 1
// Config 2
static float leftX   = -0.2395;
static float rightX  = -0.2275;
static float topY    = .660;
static float bottomY = .648;
#endif

#if 0
// Config 3
static float leftX = -0.13749;
static float rightX = -0.13715;
static float topY = .64975;
static float bottomY = .64941;
#endif

static float xstep;
static float ystep;

static void zoom (void)
{
  float xrange = (rightX - leftX);
  float yrange = (topY - bottomY);

  leftX += ZOOM_SPEED * xrange;
  rightX -= ZOOM_SPEED * xrange;
  topY -= ZOOM_SPEED * yrange;
  bottomY += ZOOM_SPEED * yrange;

  xstep = (rightX - leftX) / DIM;
  ystep = (topY - bottomY) / DIM;
}

void mandel_init ()
{
  xstep = (rightX - leftX) / DIM;
  ystep = (topY - bottomY) / DIM;
}

static unsigned compute_one_pixel (int i, int j)
{
  float cr = leftX + xstep * j;
  float ci = topY - ystep * i;
  float zr = 0.0, zi = 0.0;

  int iter;

  // Pour chaque pixel, on calcule les termes d'une suite, et on
  // s'arrête lorsque |Z| > 2 ou lorsqu'on atteint MAX_ITERATIONS
  for (iter = 0; iter < MAX_ITERATIONS; iter++) {
    float x2 = zr * zr;
    float y2 = zi * zi;

    /* Stop iterations when |Z| > 2 */
    if (x2 + y2 > 4.0)
      break;

    float twoxy = (float)2.0 * zr * zi;
    /* Z = Z^2 + C */
    zr = x2 - y2 + cr;
    zi = twoxy + ci;
  }

  return iter;
}

///////////////////////////// Version séquentielle simple (seq)

// Renvoie le nombre d'itérations effectuées avant stabilisation, ou 0
unsigned mandel_compute_seq (unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++) {

    for (int i = 0; i < DIM; i++)
      for (int j = 0; j < DIM; j++)
        cur_img (i, j) = iteration_to_color (compute_one_pixel (i, j));

    zoom ();
  }

  return 0;
}

///////////////////////////// Version séquentielle vectorisée (vec)

#ifdef ENABLE_VECTO

#if VEC_SIZE == 8
static void compute_multiple_pixels (unsigned *iterations, int i, int j)
{
  __m256 zr, zi, cr, ci, norm; //, iter;
  __m256 deux     = _mm256_set1_ps (2.0);
  __m256 max_norm = _mm256_set1_ps (4.0);

  __m256i iter = _mm256_setzero_si256 ();
  __m256i un   = _mm256_set1_epi32 (1);
  __m256i vrai = _mm256_set1_epi32 (-1);

  zr = zi = norm = _mm256_set1_ps (0);

  cr = _mm256_add_ps (_mm256_set1_ps (j),
                      _mm256_set_ps (7, 6, 5, 4, 3, 2, 1, 0));

  cr = _mm256_fmadd_ps (cr, _mm256_set1_ps (xstep), _mm256_set1_ps (leftX));

  ci = _mm256_set1_ps (topY - ystep * i);

  for (int i = 0; i < MAX_ITERATIONS; i++) {
    __m256 rc    = _mm256_mul_ps (zr, zr);
    norm         = _mm256_fmadd_ps (zi, zi, rc);
    __m256i mask = (__m256i)_mm256_cmp_ps (norm, max_norm, _CMP_LE_OS);
    if (_mm256_testz_si256 (mask, vrai))
      break;
    iter = _mm256_add_epi32 (iter, _mm256_and_si256 (mask, un));

    __m256 x = _mm256_add_ps (rc, _mm256_fnmadd_ps (zi, zi, cr));
    __m256 y = _mm256_fmadd_ps (deux, _mm256_mul_ps (zr, zi), ci);
    zr       = x;
    zi       = y;
  }

  _mm256_store_si256 ((__m256i *)iterations, iter);
}

#elif VEC_SIZE == 4

static void compute_multiple_pixels (unsigned *iterations, int i, int j)
{
  __m128 zr, zi, cr, ci, norm, iter;
  __m128 deux     = _mm_set1_ps (2.0);
  __m128 un       = _mm_set1_ps (1.0);
  __m128 vrai     = _mm_set1_ps (-1);
  __m128 max_norm = _mm_set1_ps (4.0);

  zr = zi = norm = iter = _mm_set1_ps (0);
  cr = _mm_set_ps (leftX + xstep * (j + 3), leftX + xstep * (j + 2),
                   leftX + xstep * (j + 1), leftX + xstep * (j + 0));
  ci = _mm_set1_ps (topY - ystep * i);

  for (int i = 0; i < MAX_ITERATIONS; i++) {
    norm        = _mm_fmadd_ps (zr, zr, _mm_mul_ps (zi, zi));
    __m128 mask = _mm_cmp_ps (norm, max_norm, _CMP_LE_OS);
    if (_mm_testz_ps (mask, vrai))
      break;
    iter = _mm_add_ps (iter, _mm_and_ps (mask, un));

    __m128 x = _mm_fmadd_ps (zr, zr, _mm_fnmadd_ps (zi, zi, cr));
    __m128 y = _mm_fmadd_ps (deux, _mm_mul_ps (zr, zi), ci);
    zr       = x;
    zi       = y;
  }

  __m128i res = _mm_cvttps_epi32 (iter);
  _mm_store_si128 ((__m128i *)iterations, res);
}

#else
#warning Only 128bits SSE (VEC_SIZE=4) or 256bit AVX (VEC_SIZE=8) vectorization are currently supported
#endif

#endif

#if defined(ENABLE_VECTO) && (VEC_SIZE == 4 || VEC_SIZE == 8)

static void do_computation (int i, int j)
{
  unsigned iterations[VEC_SIZE];

  compute_multiple_pixels (iterations, i, j);

  for (int v = 0; v < VEC_SIZE; v++)
    cur_img (i, j + v) = iteration_to_color (iterations[v]);
}

static void traiter_tuile_vec (int i_d, int j_d, int i_f, int j_f)
{
  PRINT_DEBUG ('c', "tuile [%d-%d][%d-%d] traitée\n", i_d, i_f, j_d, j_f);

  for (int i = i_d; i <= i_f; i++)
    for (int j = j_d; j <= j_f; j += VEC_SIZE)
      do_computation (i, j);
}

// Renvoie le nombre d'itérations effectuées avant stabilisation, ou 0
unsigned mandel_compute_vec (unsigned nb_iter)
{
  for (unsigned it = 1; it <= nb_iter; it++) {

    // On traite toute l'image en une seule fois
    traiter_tuile_vec (0, 0, DIM - 1, DIM - 1);
    zoom ();
  }

  return 0;
}

#else

#define traiter_tuile_vec(i_d, j_d, i_f, j_f) traiter_tuile (i_d, j_d, i_f, j_f)

#endif

///////////////////////////// Version séquentielle tuilée (tiled)

static unsigned tranche = 0;

static void traiter_tuile (int i_d, int j_d, int i_f, int j_f)
{
  PRINT_DEBUG ('c', "tuile [%d-%d][%d-%d] traitée\n", i_d, i_f, j_d, j_f);

  for (int i = i_d; i <= i_f; i++)
    for (int j = j_d; j <= j_f; j++) {
      unsigned n     = compute_one_pixel (i, j);
      cur_img (i, j) = iteration_to_color (n);
    }
}

unsigned mandel_compute_tiled (unsigned nb_iter)
{
  tranche = DIM / GRAIN;

  for (unsigned it = 1; it <= nb_iter; it++) {

    // On itére sur les coordonnées des tuiles
    for (int i = 0; i < GRAIN; i++)
      for (int j = 0; j < GRAIN; j++)
        traiter_tuile_vec (i * tranche /* i debut */, j * tranche /* j debut */,
                           (i + 1) * tranche - 1 /* i fin */,
                           (j + 1) * tranche - 1 /* j fin */);

    zoom ();
  }

  return 0;
}

///////////////////////////// Version thread bloc

static unsigned nb_threads = 2;
static unsigned iterations = 1;

static pthread_barrier_t barrier;

static void *thread_starter_bloc (void *arg)
{
  unsigned me    = (unsigned)(intptr_t)arg;
  unsigned slice = DIM / nb_threads;
  unsigned i_d   = me * slice;
  unsigned i_f   = ((me == nb_threads - 1) ? DIM - 1 : (me + 1) * slice - 1);

  PRINT_DEBUG ('t', "Thread %d/%d started, computing slice [%4u-%4u]\n", me,
               nb_threads, i_d, i_f);
  for (unsigned it = 1; it <= iterations; it++) {
    traiter_tuile_vec (i_d, 0, i_f, DIM - 1);
#ifdef ENABLE_MONITORING
    monitoring_add_tile (0, i_d, DIM, i_f - i_d + 1, me);
#endif

#ifdef __APPLE__
    pthread_barrier_single (&barrier, zoom);
#else
    pthread_barrier_wait (&barrier);
    if (me == 0)
      zoom ();
    pthread_barrier_wait (&barrier);
#endif
  }

  return NULL;
}

unsigned mandel_compute_thread (unsigned nb_iter)
{
  char *str = getenv ("OMP_NUM_THREADS");

  if (str != NULL)
    nb_threads = atoi (str);
  else
    nb_threads = get_nb_cores ();

  iterations = nb_iter;

  pthread_t pid[nb_threads - 1];

  pthread_barrier_init (&barrier, NULL, nb_threads);

  for (int i = 0; i < nb_threads - 1; i++)
    pthread_create (&pid[i], NULL, thread_starter_bloc,
                    (void *)(intptr_t) (i + 1));

  thread_starter_bloc (0);

  for (int i = 0; i < nb_threads - 1; i++)
    pthread_join (pid[i], NULL);

  return 0;
}

///////////////////////////// Version thread cyclic

static void *thread_starter_cyclic (void *arg)
{
  unsigned me = (unsigned)(intptr_t)arg;

  PRINT_DEBUG ('t', "Thread %d/%d started\n", me, nb_threads);

  for (unsigned it = 1; it <= iterations; it++) {

    for (unsigned line = me; line < DIM; line += nb_threads) {
      traiter_tuile_vec (line, 0, line, DIM - 1);
#ifdef ENABLE_MONITORING
      monitoring_add_tile (0, line, DIM, 1, me);
#endif
    }

#ifdef __APPLE__
    pthread_barrier_single (&barrier, zoom);
#else
    pthread_barrier_wait (&barrier);
    if (me == 0)
      zoom ();
    pthread_barrier_wait (&barrier);
#endif
  }

  return NULL;
}

unsigned mandel_compute_thread_cyclic (unsigned nb_iter)
{
  char *str = getenv ("OMP_NUM_THREADS");

  if (str != NULL)
    nb_threads = atoi (str);
  else
    nb_threads = get_nb_cores ();

  iterations = nb_iter;

  pthread_t pid[nb_threads - 1];

  pthread_barrier_init (&barrier, NULL, nb_threads);

  for (int i = 0; i < nb_threads - 1; i++)
    pthread_create (&pid[i], NULL, thread_starter_cyclic,
                    (void *)(intptr_t) (i + 1));

  thread_starter_cyclic (0);

  for (int i = 0; i < nb_threads - 1; i++)
    pthread_join (pid[i], NULL);

  return 0;
}

///////////////////////////// Version thread dynamique

static pthread_distrib_t distrib;

static void *thread_starter_dyn (void *arg)
{
  unsigned me = (unsigned)(intptr_t)arg;

  PRINT_DEBUG ('t', "Thread %d/%d started\n", me, nb_threads);

  for (unsigned it = 1; it <= iterations; it++) {
    for (;;) {
      int line = pthread_distrib_get (&distrib);
      if (line == -1)
        break;
      PRINT_DEBUG ('t', "Thread %d got slice [%d]\n", me, line);
      traiter_tuile_vec (line /* i debut */, 0 /* j debut */, line /* i fin */,
                         DIM - 1 /* j fin */);
#ifdef ENABLE_MONITORING
      monitoring_add_tile (0, line, DIM, 1, me);
#endif
    }
  }

  return NULL;
}

unsigned mandel_compute_thread_dyn (unsigned nb_iter)
{
  char *str = getenv ("OMP_NUM_THREADS");

  if (str != NULL)
    nb_threads = atoi (str);
  else
    nb_threads = get_nb_cores ();

  iterations = nb_iter;

  pthread_t pid[nb_threads - 1];

  pthread_distrib_init (&distrib, nb_threads, DIM, zoom);

  for (int i = 0; i < nb_threads - 1; i++)
    pthread_create (&pid[i], NULL, thread_starter_dyn,
                    (void *)(intptr_t) (i + 1));

  thread_starter_dyn (0);

  for (int i = 0; i < nb_threads - 1; i++)
    pthread_join (pid[i], NULL);

  return 0;
}

///////////////////////////// Version thread dynamique avec tuiles rectangles

static void *thread_starter_dyn_tiled (void *arg)
{
  unsigned me = (unsigned)(intptr_t)arg;

  PRINT_DEBUG ('t', "Thread %d/%d started\n", me, nb_threads);

  for (unsigned it = 1; it <= iterations; it++) {
    for (;;) {
      int slice = pthread_distrib_get (&distrib);
      if (slice == -1)
        break;
      unsigned i = slice / GRAIN;
      unsigned j = slice % GRAIN;
      PRINT_DEBUG ('t', "Thread %d got slice [%d, %d]\n", me, i, j);
      traiter_tuile_vec (i * tranche /* i debut */, j * tranche /* j debut */,
                         (i + 1) * tranche - 1 /* i fin */,
                         (j + 1) * tranche - 1 /* j fin */);
#ifdef ENABLE_MONITORING
      monitoring_add_tile (j * tranche, i * tranche, tranche, tranche, me);
#endif
    }
  }

  return NULL;
}

unsigned mandel_compute_thread_dyn_tiled (unsigned nb_iter)
{
  char *str = getenv ("OMP_NUM_THREADS");

  if (str != NULL)
    nb_threads = atoi (str);
  else
    nb_threads = get_nb_cores ();

  tranche = DIM / GRAIN;

  iterations = nb_iter;

  pthread_t pid[nb_threads - 1];

  pthread_distrib_init (&distrib, nb_threads, GRAIN * GRAIN, zoom);

  for (int i = 0; i < nb_threads - 1; i++)
    pthread_create (&pid[i], NULL, thread_starter_dyn_tiled,
                    (void *)(intptr_t) (i + 1));

  thread_starter_dyn_tiled (0);

  for (int i = 0; i < nb_threads - 1; i++)
    pthread_join (pid[i], NULL);

  return 0;
}

///////////////////////////// Version OpenMP avec omp for (omp)

unsigned mandel_compute_omp (unsigned nb_iter)
{
  tranche = DIM / GRAIN;

  for (unsigned it = 1; it <= nb_iter; it++) {

    // On itére sur les coordonnées des tuiles
#pragma omp parallel for collapse(2) schedule(runtime)
    for (int i = 0; i < GRAIN; i++)
      for (int j = 0; j < GRAIN; j++) {
        traiter_tuile_vec (i * tranche /* i debut */, j * tranche /* j debut */,
                           (i + 1) * tranche - 1 /* i fin */,
                           (j + 1) * tranche - 1 /* j fin */);
#ifdef ENABLE_MONITORING
        monitoring_add_tile (j * tranche, i * tranche, tranche, tranche,
                             omp_get_thread_num ());
#endif
      }

    zoom ();
  }

  return 0;
}

///////////////////////////// Version utilisant un ordonnanceur maison (sched)

unsigned P;

void mandel_init_sched ()
{
  xstep = (rightX - leftX) / DIM;
  ystep = (topY - bottomY) / DIM;

  P = scheduler_init (-1);
}

void mandel_finalize_sched ()
{
  scheduler_finalize ();
}

static inline void *pack (int i, int j)
{
  uint64_t x = (uint64_t)i << 32 | j;
  return (void *)x;
}

static inline void unpack (void *a, int *i, int *j)
{
  *i = (uint64_t)a >> 32;
  *j = (uint64_t)a & 0xFFFFFFFF;
}

static inline unsigned cpu (int i, int j)
{
  return -1; // was: i % P
}

static inline void create_task (task_func_t t, int i, int j)
{
  scheduler_create_task (t, pack (i, j), cpu (i, j));
}

//////// First Touch

static void zero_seq (int i_d, int j_d, int i_f, int j_f)
{

  for (int i = i_d; i <= i_f; i++)
    for (int j = j_d; j <= j_f; j++)
      cur_img (i, j) = 0;
}

static void first_touch_task (void *p, unsigned proc)
{
  int i, j;

  unpack (p, &i, &j);

  // PRINT_DEBUG ('s', "First-touch Task is running on tile (%d, %d) over cpu
  // #%d\n", i, j, proc);
  zero_seq (i * tranche, j * tranche, (i + 1) * tranche - 1,
            (j + 1) * tranche - 1);
}

void mandel_ft_sched (void)
{
  tranche = DIM / GRAIN;

  for (int i = 0; i < GRAIN; i++)
    for (int j = 0; j < GRAIN; j++)
      create_task (first_touch_task, i, j);

  scheduler_task_wait ();
}

//////// Compute

static void compute_task (void *p, unsigned proc)
{
  int i, j;

  unpack (p, &i, &j);

  // PRINT_DEBUG ('s', "Compute Task is running on tile (%d, %d) over cpu
  // #%d\n", i, j, proc);
  traiter_tuile_vec (i * tranche, j * tranche, (i + 1) * tranche - 1,
                     (j + 1) * tranche - 1);

#ifdef ENABLE_MONITORING
  monitoring_add_tile (j * tranche, i * tranche, tranche, tranche, proc);
#endif
}

unsigned mandel_compute_sched (unsigned nb_iter)
{
  tranche = DIM / GRAIN;

  for (unsigned it = 1; it <= nb_iter; it++) {

    for (int i = 0; i < GRAIN; i++)
      for (int j = 0; j < GRAIN; j++)
        create_task (compute_task, i, j);

    scheduler_task_wait ();

    zoom ();
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////
///////////////////////////// Version OpenCL

void mandel_init_ocl ()
{
  xstep = (rightX - leftX) / DIM;
  ystep = (topY - bottomY) / DIM;
}

unsigned mandel_compute_ocl (unsigned nb_iter)
{
  size_t global[2] = {SIZE, SIZE};   // global domain size for our calculation
  size_t local[2]  = {TILEX, TILEY}; // local domain size for our calculation
  cl_int err;
  unsigned max_iter = MAX_ITERATIONS;

  for (unsigned it = 1; it <= nb_iter; it++) {

    // Set kernel arguments
    //
    err = 0;
    err |= clSetKernelArg (compute_kernel, 0, sizeof (cl_mem), &cur_buffer);
    err |= clSetKernelArg (compute_kernel, 1, sizeof (float), &leftX);
    err |= clSetKernelArg (compute_kernel, 2, sizeof (float), &xstep);
    err |= clSetKernelArg (compute_kernel, 3, sizeof (float), &topY);
    err |= clSetKernelArg (compute_kernel, 4, sizeof (float), &ystep);
    err |= clSetKernelArg (compute_kernel, 5, sizeof (unsigned), &max_iter);

    check (err, "Failed to set kernel arguments");

    err = clEnqueueNDRangeKernel (queue, compute_kernel, 2, NULL, global, local,
                                  0, NULL, NULL);
    check (err, "Failed to execute kernel");

    zoom ();
  }

  return 0;
}
