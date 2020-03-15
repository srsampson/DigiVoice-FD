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

#include <math.h>
#include <complex.h>

#include "defines.h"
#include "encode_pitch.h"
#include "encode_fft.h"

static float cnormf(complex float);
static int postProcessSubMultiples(float [], float, int);

static float Nlp_sq[M_PITCH];       // 320
static float Nlp_mem_x;
static float Nlp_mem_y;
static float Nlp_mem_fir[NLP_NTAP]; // 48
static int Nlp_prev_f0;

static fft_cfg Nlp_fft_cfg;

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

int encodePitchCreate() {
    if ((Nlp_fft_cfg = encode_fft_alloc(FFT_SIZE, 0, NULL, NULL)) == NULL) {
        return -1;
    }

    return 0;
}

void encodePitchDestroy() {
    free(Nlp_fft_cfg);
}

int encodeDetectPitch(float Sn[]) {
    complex float Fw[FFT_SIZE];
    float fw[FFT_SIZE];
    
    for (int i = 0; i < FFT_SIZE; i++) {
	Fw[i] = 0.0f;
    }

    /* Square, notch filter at DC, and LP filter vector */

    for (int i = (M_PITCH - N_SAMP); i < M_PITCH; i++) { /* square last 80 speech samples */
        Nlp_sq[i] = (Sn[i] * Sn[i]);
    }

    for (int i = (M_PITCH - N_SAMP); i < M_PITCH; i++) { /* notch filter at DC */
        float notch = (Nlp_sq[i] - Nlp_mem_x) + (COEFF * Nlp_mem_y);

        Nlp_mem_x = Nlp_sq[i];
        Nlp_mem_y = notch;

        Nlp_sq[i] = notch + 1.0f;
    }

    for (int i = (M_PITCH - N_SAMP); i < M_PITCH; i++) { /* FIR filter vector */
        for (int j = 0; j < NLP_NTAP - 1; j++)
            Nlp_mem_fir[j] = Nlp_mem_fir[j + 1];

        Nlp_mem_fir[NLP_NTAP - 1] = Nlp_sq[i];

        Nlp_sq[i] = 0.0f;

        for (int j = 0; j < NLP_NTAP; j++)
            Nlp_sq[i] += (Nlp_mem_fir[j] * Nlp_fir[j]);
    }

    /* Decimate and FFT */

    for (int i = 0; i < (M_PITCH / DEC); i++) {
        Fw[i] = Nlp_sq[DEC * i] * Nlp_cosw[i];
    }

    encode_fft(Nlp_fft_cfg, Fw, Fw);

    for (int i = 0; i < FFT_SIZE; i++) {
        fw[i] = cnormf(Fw[i]);
    }

    /* find global peak over 16..128 FFT filters */

    float gmax = 0.0f;
    int gmax_bin = FFT_SIZE * DEC / P_MAX;

    for (int i = (FFT_SIZE * DEC) / P_MAX; i <= (FFT_SIZE * DEC) / P_MIN; i++) {
        if (fw[i] > gmax) {
            gmax = fw[i];
            gmax_bin = i;
        }
    }

    /* Save as previous on next pass */
    
    Nlp_prev_f0 = postProcessSubMultiples(fw, gmax, gmax_bin);
    
    /* Shift samples in buffer to make room for new samples */

    for (int i = 0; i < (M_PITCH - N_SAMP); i++) {
        Nlp_sq[i] = Nlp_sq[N_SAMP + i];
    }

    /* return pitch */

    return FS / Nlp_prev_f0;
}

static int postProcessSubMultiples(float fw[], float gmax, int gmax_bin) {
    float thresh;
    int mult = 2;
    int cmax_bin = gmax_bin;
    int prev_f0_bin = Nlp_prev_f0 * (FFT_SIZE * DEC) / FS;

    while ((gmax_bin / mult) >= MIN_BIN) {

        int b = gmax_bin / mult; /* determine search interval */
        int bmin = 0.8f * b;
        int bmax = 1.2f * b;

        if (bmin < MIN_BIN) {
            bmin = MIN_BIN;
        }

        /* lower threshold to favor previous frames pitch estimate,
            this is a form of pitch tracking */

        if ((prev_f0_bin > bmin) && (prev_f0_bin < bmax)) {
            thresh = CNLP * gmax * 0.5f;
        } else {
            thresh = CNLP * gmax;
        }

        float lmax = 0.0f;
        int lmax_bin = bmin;

        for (int i = bmin; i <= bmax; i++) { /* look for maximum in interval */
            if (fw[i] > lmax) {
                lmax = fw[i];
                lmax_bin = i;
            }
        }

        if (lmax > thresh) {
            if ((lmax > fw[lmax_bin - 1]) && (lmax > fw[lmax_bin + 1])) {
                cmax_bin = lmax_bin;
            }
        }

        mult++;
    }

    return cmax_bin * (FS / (FFT_SIZE * DEC));
}
