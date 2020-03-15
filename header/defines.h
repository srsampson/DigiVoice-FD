/*
 * Copyright (C) 1993-2019 David Rowe
 *
 * All rights reserved
 *
 * Modified March 2020 by Steve Sampson
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <complex.h>
#include <stdbool.h>

/* General defines */

#define N_SAMP          80  /* number of PCM samples per 10ms at 8 kHz   */
#define MAX_AMP         80  /* maximum number of harmonics               */
#define N_MODELS        4   /* Number of encode models                   */

#ifndef M_PI
#define M_PI            3.14159265358979323846f
#endif

#define TAU             (2.0f * M_PI)

#define FS              8000    /* sample rate in Hz                    */
#define FFT_SIZE        512     /* size of FFT used for encoder/decoder */
#define PHASE_FFT_SIZE  128
#define V_THRESH        6.0f    /* voicing threshold in dB              */

/* Pitch estimation defines */

#define M_PITCH         320     /* pitch analysis frame size            */
#define P_MIN           20      /* minimum pitch in samples             */
#define P_MAX           160     /* maximum pitch in samples             */
#define cmplx(value) (cosf(value) + sinf(value) * I)

/* Structure to hold model parameters for one frame */
    
typedef struct {
    float Wo;                /* fundamental frequency estimate */
    int L;                   /* number of harmonics            */
    float A[MAX_AMP + 1];    /* amplitude of each harmonic     */
    bool voiced;             /* voiced boolean                 */
} ENCODE_MODEL;

typedef struct {
    complex float H[MAX_AMP + 1];
    float Wo;                /* fundamental frequency estimate */
    int L;                   /* number of harmonics            */
    float A[MAX_AMP + 1];    /* amplitude of each harmonic     */
    float phi[MAX_AMP + 1];  /* phase of each harmonic         */
    bool voiced;             /* voiced boolean                 */
} DECODE_MODEL;

/* Complex FFT */

struct fft_state {
    int nfft;
    int inverse;
    int factors[64];
    complex float twiddles[1];
};

typedef struct fft_state *fft_cfg;

/* Real FFT */

struct fftr_state {
    fft_cfg substate;
    complex float *tmpbuf;
    complex float *super_twiddles;
};

typedef struct fftr_state *fftr_cfg;

extern const float Codebook1[];
extern const float Codebook2[];
extern const float Nlp_cosw[];
extern const float Nlp_fir[];
extern const float Parzen[];
extern const float Hamming[];
extern const float Hamming2[];
extern const float Amp_freqs_kHz[];
extern const float AmpPre[];
extern const float EnergyTable[];
extern const float PitchTable[];

#ifdef __cplusplus
}
#endif
