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

#include <stdbool.h>
#include <math.h>
#include <complex.h>
#include <string.h>

#include "defines.h"
#include "decode_harmonics.h"
#include "decode_fft.h"

static void synthesize(DECODE_MODEL *);
static void postfilter(DECODE_MODEL *);
static void phaseSynthZeroOrder(DECODE_MODEL *);
static int codecRandom(void);
static float cnormf(complex float);

static fft_cfg Sine_fft_fwd_cfg;
static fft_cfg Sine_fft_inv_cfg;
static fftr_cfg Sine_fftr_inv_cfg;

static float Sine_Sn_[N_SAMP * 2];
static float Sine_ex_phase;
static float Sine_bg_est;

static unsigned long Next = 1;

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

int decode_harmonics_create() {
    Sine_fftr_inv_cfg = decode_fftr_alloc(FFT_SIZE, 1, NULL, NULL);
    Sine_fft_fwd_cfg  = decode_fft_alloc(PHASE_FFT_SIZE, 0, NULL, NULL);
    Sine_fft_inv_cfg  = decode_fft_alloc(PHASE_FFT_SIZE, 1, NULL, NULL);

    if (Sine_fftr_inv_cfg == NULL || Sine_fft_fwd_cfg == NULL || Sine_fft_inv_cfg == NULL) {
        return -1;
    }

    return 0;
}

void decode_harmonics_destroy() {
    free(Sine_fft_inv_cfg);
    free(Sine_fft_fwd_cfg);
    free(Sine_fftr_inv_cfg);
}

static int codecRandom() {
    Next = Next * 1103515245 + 12345;
    return((unsigned)(Next/65536) % 32768);
}

void synthesizeOneSegment(int16_t speech[], DECODE_MODEL *decode_model) {
    phaseSynthZeroOrder(decode_model);
    postfilter(decode_model);
    synthesize(decode_model);   // Populate Sine_Sn_

    /* Limit output audio */

    float max_sample = 0.0f;

    for (int i = 0; i < N_SAMP; i++) {
        if (Sine_Sn_[i] > max_sample) {
            max_sample = Sine_Sn_[i];
        }
    }

    float over = max_sample / 30000.0f;

    if (over > 1.0f) {
        float gain = 1.0f / (over * over);

        for (int i = 0; i < N_SAMP; i++) {
            Sine_Sn_[i] *= gain;
        }
    }
    
    // Mode 700c is a little weak over-all

    for (int i = 0; i < N_SAMP; i++) {
        Sine_Sn_[i] *= 1.5f;
    }
    
    for (int i = 0; i < N_SAMP; i++) {
        if (Sine_Sn_[i] > 32760.0f) {
            speech[i] = (int16_t) 32760;        // don't saturate it
        } else if (Sine_Sn_[i] < -32760.0f) {
            speech[i] = (int16_t) -32760;       // ditto
        } else {
            speech[i] = (int16_t) Sine_Sn_[i];
        }
    }
}

void magToPhase(float phase[], float mag[]) {
    complex float Sdb[PHASE_FFT_SIZE];
    complex float cf[PHASE_FFT_SIZE];

    Sdb[0] = mag[0];

    for (int i = 1; i < NS; i++) {                     // 1 - 64
        Sdb[i                 ] = mag[i] + 0.0f * I;   // 1 - 64
        Sdb[PHASE_FFT_SIZE - i] = mag[i] + 0.0f * I;   // 63 - 0
    }

    /* compute real cepstrum from log magnitude spectrum */

    complex float c[PHASE_FFT_SIZE];

    decode_fft(Sine_fft_inv_cfg, Sdb, c);  // 128

    for (int i = 0; i < PHASE_FFT_SIZE; i++) {
        c[i] = c[i] / (float) PHASE_FFT_SIZE;
    }

    /* Fold cepstrum to reflect non-min-phase zeros inside unit circle */

    for (int i = 0; i < PHASE_FFT_SIZE; i++) {
	cf[i] = 0.0f;
    }

    cf[0] = c[0];

    for (int i = 1; i < (NS - 1); i++) {
        cf[i] = c[i] + c[PHASE_FFT_SIZE - i];
    }

    cf[NS - 1] = c[NS - 1];

    decode_fft(Sine_fft_fwd_cfg, cf, cf);  // in-place 128

    for (int i = 0; i < NS; i++) {
        phase[i] = cimagf(cf[i]) / SCALE;
    }
}

static void phaseSynthZeroOrder(DECODE_MODEL *model) {
    complex float ex[MAX_AMP + 1];

    Sine_ex_phase += ((model->Wo * N_SAMP) - (floorf(Sine_ex_phase / TAU + 0.5f) * TAU));

    for (int m = 1; m <= model->L; m++) {

        /* generate excitation */

        if (model->voiced == true) {
            ex[m] = cmplx((float) m * Sine_ex_phase);
        } else {
            /* pick a random point on the circle */
            ex[m] = cmplx(TAU / CODEC2_RND_MAX * (float) codecRandom());
        }

        /* filter using LPC filter */
        /* Note: H was populated during determine_phase() in Amp */

        ex[m] *= model->H[m];

        /* modify sinusoidal phase */

        model->phi[m] = atan2f(cimagf(ex[m]), crealf(ex[m]) + 1E-12f);
    }
}

static void postfilter(DECODE_MODEL *decode_model) {
    /* determine average energy across spectrum */

    float e = 1E-12f;

    for (int i = 1; i <= decode_model->L; i++)
        e += (decode_model->A[i] * decode_model->A[i]);

    e = 10.0f * log10f(e / decode_model->L);

    /* filter the running threshold */
    
    if ((e < BG_THRESH) && (decode_model->voiced == false))
        Sine_bg_est *= (1.0f - BG_BETA) + e * BG_BETA;

    float thresh = powf(10.0f, (Sine_bg_est + BG_MARGIN) / 20.0f);

    if (decode_model->voiced == true) {
        for (int i = 1; i <= decode_model->L; i++) {
            /* for mean values below threshold randomize the phase */
            if (decode_model->A[i] < thresh) {
                /* pick a random point on the circle */
                decode_model->phi[i] = (TAU / CODEC2_RND_MAX * (float) codecRandom());
            }
        }
    }
}

/*
 * Synthesize a speech signal in the frequency domain from the sinusoidal
 * model parameters. Uses overlap-add with a trapezoidal window to smoothly
 * interpolate between frames.
 */
static void synthesize(DECODE_MODEL *model) {
    complex float Sw_[FFT_SIZE / 2 + 1];
    float sw_[FFT_SIZE];

    /* Update memories */

    for (int i = 0; i < (N_SAMP - 1); i++) {
        Sine_Sn_[i] = Sine_Sn_[N_SAMP + i]; // Left shift history 80 samples
    }

    Sine_Sn_[N_SAMP - 1] = 0.0f;

    /* Now set up frequency domain synthesized speech */

    for (int i = 0; i < (FFT_SIZE / 2 + 1); i++) {
	Sw_[i] = 0.0f;
    }

    float wo = model->Wo * FFT_SIZE / TAU;

    for (int l = 1; l <= model->L; l++) {
        int b = (int) (l * wo + 0.5f);

        if (b > ((FFT_SIZE / 2) - 1)) {
            b = (FFT_SIZE / 2) - 1;
        }

        Sw_[b] = cmplx(model->phi[l]) * model->A[l];
    }

    /* Perform inverse real FFT */

    decode_fftri(Sine_fftr_inv_cfg, Sw_, sw_);

    /* Overlap add to previous samples */

    for (int i = 0; i < (N_SAMP - 1); i++) {
        Sine_Sn_[i] += sw_[FFT_SIZE - N_SAMP + 1 + i] * Parzen[i];
    }

    /* put the new data on the end of the window */

    for (int i = (N_SAMP - 1), j = 0; i < (N_SAMP * 2); i++, j++) {
        Sine_Sn_[i] = sw_[j] * Parzen[i];
    }
}

