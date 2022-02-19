/*
 *  The output includes several columns:
 *	Loop:		name of the loop
 *	Time(ns): 	time in nanoseconds to run the loop
 *	ps/it: 	    picoseconds per C loop iteration
 *	Checksum:	checksum calculated when the test has run
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <x86intrin.h>

#include "precision.h"

// Los vectores deben caber en la cache para que la velocidad de ejecución
// no esté limitada por el ancho de banda de memoria principal
#ifndef LEN 
    #define LEN     1024
#endif
#define FLOP_IT    (unsigned long int)  3     /* 3 FLOP per iteration */

// Numero total de FLOP que queremos ejecutar
// Si es múltiplo de LEN y FLOP_IT se facilitan las cuentas */
#define FLOP_COUNT  (unsigned long int) 3*4*5*512*1024*1024
// este valor de FLOP_COUNT es múltiplo para las siguientes combinaciones de LEN y FLOP_IT
//   LEN={ 1k, 2k, 4k, ... 1M, 2M, ... 512M }
//   FLOP_IT={ 1, 2, 3, 4, 5, 6 }

// para ejecuciones más rapidas con SDE */
// #define FLOP_COUNT  (unsigned long int) 3*4*5*1024*1024
                       
#define NTIMES      (unsigned long int) (FLOP_COUNT/(LEN*FLOP_IT))    /* iteraciones bucle externo */
                                  
#define SIMD_ALIGN  64  /* 512 bits (preparado para AVX-512) */
static real x[LEN] __attribute__((aligned(SIMD_ALIGN)));
static real y[LEN] __attribute__((aligned(SIMD_ALIGN)));
static real alpha = 0.4;
static real beta = 0.6;

int dummy(real a[], real b[], real alpha, real beta);

/* inhibimos el inlining de algunas funciones
 * para que el ensamblador sea más cómodo de leer */

/* return wall time in seconds */
__attribute__ ((noinline))
double
get_wall_time()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)) {
        exit(-1); // return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

/* inhibimos vectorización en esta función
 * para que los informes de compilación sean más cómodos de leer */
__attribute__((optimize("no-tree-vectorize")))
void check(const real arr[LEN])
{
    real sum = 0;
    for (unsigned int i = 0; i < LEN; i++)
        sum += arr[i];

    printf("%f \n", sum);
}

__attribute__((optimize("no-tree-vectorize")))
__attribute__ ((noinline))
int init()
{
    for (int j = 0; j < LEN; j++)
    {
	    x[j] = 2.0;
	    y[j] = 0.5;
	}
    return 0;
}

__attribute__ ((noinline))
void results(const double wall_time, const char *loop)
{
    printf("%18s  %5.1f    %5.1f     ",
            loop /* loop name */,
            wall_time/(1e-9*NTIMES),     /* ns/loop */
            wall_time/(1e-12*NTIMES*LEN) /* ps/el */);
}

/* axpby functions */
__attribute__ ((noinline))
int
axpby()
{
    double start_t, end_t;

    init();
    start_t = get_wall_time();
    for (unsigned int nl = 0; nl < NTIMES; nl++)
    {
        for (unsigned int i = 0; i < LEN; i++)
        {
            y[i] = alpha*x[i] + beta*y[i];
        }
        dummy(x, y, alpha, beta);
    }
    end_t = get_wall_time();
    results(end_t - start_t, "axpby");
    check(y);
    return 0;
}

__attribute__ ((noinline))
int
axpby_intr_SSE()
{
    double start_t, end_t;

    init();
    start_t = get_wall_time();

#if PRECISION==0
  __m128 vX, vY, valpha, vbeta;
    for (unsigned int nl = 0; nl < NTIMES; nl++)
    {
        valpha = _mm_set1_ps(alpha);     // valpha = _mm_load1_ps(&alpha);
        vbeta = _mm_set1_ps(beta);       // vbeta = _mm_load1_ps(&beta);
        for (unsigned int i = 0; i < LEN; i+= SSE_LEN)
        {
            vX = _mm_load_ps(&x[i]);
            vY = _mm_load_ps(&y[i]);
            vX = _mm_mul_ps(valpha, vX);
            vY = _mm_mul_ps(vbeta, vY);
            vY = _mm_add_ps(vX, vY);
            _mm_store_ps(&y[i], vY);
        }
        dummy(x, y, alpha, beta);
    }
#else
    __m128d vX, vY, valpha, vbeta;
    for (unsigned int nl = 0; nl < NTIMES; nl++)
    {
        valpha = _mm_set1_pd(alpha);     // valpha = _mm_load1_pd(&alpha);
        vbeta = _mm_set1_pd(beta);     // vbeta = _mm_load1_pd(&beta);
        for (unsigned int i = 0; i < LEN; i+= SSE_LEN)
        {
            vX = _mm_load_pd(&x[i]);
            vY = _mm_load_pd(&y[i]);
            vX = _mm_mul_pd(valpha, vX);
            vY = _mm_mul_pd(vbeta, vY);
            vY = _mm_add_pd(vX, vY);
            _mm_store_pd(&y[i], vY);
        }
         dummy(x, y, alpha, beta);
    }
#endif

  end_t = get_wall_time();
  results(end_t - start_t, "axpby_intr_SSE");
  check(y);
  return 0;
}

#if 0
__attribute__ ((noinline))
int axpby_intr_AVX()
{
  double start_t, end_t;

  init();
  start_t = get_wall_time();

#if PRECISION==0

#else

#endif

  end_t = get_wall_time();
  results(end_t - start_t, "axpby_intr_AVX");
  check(y);
  return 0;
}
#endif

int main()
{
  // printf("LEN: %u, NTIMES: %lu\n\n", LEN, NTIMES);
  printf("                     Time    TPI\n");
  printf("              Loop    ns     ps/el     Checksum \n");
  axpby();
  // axpby_intr_SSE();
  // axpby_intr_AVX();
  exit(0);
}

