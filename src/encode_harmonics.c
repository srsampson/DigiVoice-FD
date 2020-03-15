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

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>
#include <string.h>

#include "defines.h"
#include "encode_harmonics.h"
#include "encode_pitch.h"
#include "encode_fft.h"

static void hsPitchRefinement(ENCODE_MODEL *, float, float, float);
static void twoStagePitchRefinement(ENCODE_MODEL *);
static void estimateAmplitudes(ENCODE_MODEL *);
static void estVoicingMBE(ENCODE_MODEL *);
static float cnormf(complex float);

static fftr_cfg Sine_fftr_fwd_cfg;

static complex float Sine_Sw[FFT_SIZE];
static float Sine_Sn[M_PITCH];

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

int encode_harmonics_create() {
    Sine_fftr_fwd_cfg = encode_fftr_alloc(FFT_SIZE, 0, NULL, NULL);

    if (Sine_fftr_fwd_cfg == NULL) {
        return -1;
    }

    return 0;
}

void encode_harmonics_destroy() {
    free(Sine_fftr_fwd_cfg);
}

void analyzeOneSegment(ENCODE_MODEL *model, int16_t speech[]) {
    /* process the new 80 samples of speech */

    for (int i = 0; i < (M_PITCH - N_SAMP); i++) {
        Sine_Sn[i] = Sine_Sn[N_SAMP + i];         // Left shift history 80 samples
    }

    for (int i = 0; i < N_SAMP; i++) {
        Sine_Sn[(M_PITCH - N_SAMP) + i] = (float) speech[i]; // Add new 80 samples to end
    }

    model->Wo = TAU / encodeDetectPitch(Sine_Sn);      // returns pitch
    model->L = M_PI / model->Wo;

    float sw[FFT_SIZE];

    for (int i = 0; i < FFT_SIZE; i++) {
	sw[i] = 0.0f;
    }

    /* move 2nd half to start of FFT input vector */

    for (int i = 0; i < (NW / 2); i++) {
        int half = i + (M_PITCH / 2);

        sw[i] = Sine_Sn[half] * Hamming2[half];
    }

    /* move 1st half to end of FFT input vector */

    for (int i = 0; i < (NW / 2); i++) {
        int half = i + (M_PITCH / 2) - (NW / 2);

        sw[(FFT_SIZE - (NW / 2)) + i] = Sine_Sn[half] * Hamming2[half];
    }

    encode_fftr(Sine_fftr_fwd_cfg, sw, Sine_Sw);

    /* fill-in the model values */

    twoStagePitchRefinement(model);   // operates on Sine_Sw
    estimateAmplitudes(model);          // operates on Sine_Sw
    estVoicingMBE(model);              // operates on Sine_Sw
}

static void hsPitchRefinement(ENCODE_MODEL *model, float pmin, float pmax, float pstep) {
    float pitch;

    model->L = M_PI / model->Wo; /* use initial pitch est. for L */

    float wom = model->Wo; /* Wo that maximizes E */
    float em = 0.0f; /* maximum energy */

    /* Determine harmonic sum for a range of Wo values */

    for (pitch = pmin; pitch <= pmax; pitch += pstep) {
        float e = 0.0f;
        float wo = TAU / pitch;

        /* Sum harmonic magnitudes */
        float tval = wo * ONE_ON_R;
        
        for (int m = 1; m <= model->L; m++) {
            int b = (int) ((float) m * tval + 0.5f);
            e += cnormf(Sine_Sw[b]);
        }

        /* Compare to see if this is a maximum */

        if (e > em) {
            em = e;
            wom = wo;
        }
    }

    model->Wo = wom;
}

static void twoStagePitchRefinement(ENCODE_MODEL *model) {
    float tval = TAU / model->Wo; /* compute once for below */
    
    /* Coarse refinement */

    float pmax = tval + 5.0f;
    float pmin = tval - 5.0f;
    hsPitchRefinement(model, pmin, pmax, 1.0f);

    tval = TAU / model->Wo; /* compute once for below */

    /* Fine refinement */

    pmax = tval + 1.0f;
    pmin = tval - 1.0f;
    hsPitchRefinement(model, pmin, pmax, 0.25f);

    /* Limit range */

    if (model->Wo < (TAU / P_MAX)) {            // 0.039269875 (L = 80)
        model->Wo = (TAU / P_MAX);
    } else if (model->Wo > (TAU / P_MIN)) {     // 0.314159 (L = 10)
        model->Wo = (TAU / P_MIN);
    }

    model->L = floorf(M_PI / model->Wo);
    
    if (model->Wo * model->L >= FRACTPI) {
        model->L--;
    }
}

static void estimateAmplitudes(ENCODE_MODEL *model) {
    float amp = model->Wo * ONE_ON_R;

    /* init to zero in case we dump later for debug 0..80 */
    
    for (int m = 0; m <= MAX_AMP; m++) {
        model->A[m] = 0.0f;
    }

    for (int m = 1; m <= model->L; m++) {
        int am = (int) ((m - 0.5f) * amp + 0.5f);
        int bm = (int) ((m + 0.5f) * amp + 0.5f);

        float den = 0.0f;

        for (int i = am; i < bm; i++) {
            den += cnormf(Sine_Sw[i]);
        }

        model->A[m] = sqrtf(den);
    }
}

static void estVoicingMBE(ENCODE_MODEL *model) {
    float sig = 1E-4f;

    for (int l = 1; l <= (model->L / 4); l++) {
        sig += (model->A[l] * model->A[l]);
    }

    float wo = model->Wo * FFT_SIZE / TAU;
    float error = 1E-4f;

    /*
     * accumulated error between original and synthesized
     * Just test across the harmonics in the first 1000 Hz (L/4)
     */

    for (int l = 1; l <= (model->L / 4); l++) {
        complex float am = 0.0f;
        float den = 0.0f;

        int al = ceilf((l - 0.5f) * wo);
        int bl = ceilf((l + 0.5f) * wo);

        /* Estimate amplitude of harmonic assuming harmonic is totally voiced */

        int offset = (FFT_SIZE / 2) - l * wo + 0.5f;

        for (int m = al; m < bl; m++) {
            am += (Sine_Sw[m] * Hamming[offset + m]);
            den += (Hamming[offset + m] * Hamming[offset + m]);
        }

        am /= den;

        /* Determine error between estimated harmonic and original */

        for (int m = al; m < bl; m++) {
            error += cnormf(Sine_Sw[m] - (am * Hamming[offset + m]));
        }
    }

    float snr = 10.0f * log10f(sig / error);

    if (snr > V_THRESH)
        model->voiced = true;
    else
        model->voiced = false;

    float elow = 1E-4f;
    float ehigh = 1E-4f;

    for (int l = 1; l <= (model->L / 2); l++) {
        elow += (model->A[l] * model->A[l]);
    }

    for (int l = (model->L / 2); l <= model->L; l++) {
        ehigh += (model->A[l] * model->A[l]);
    }

    float eratio = 10.0f * log10f(elow / ehigh);

    if (model->voiced == false)
        if (eratio > 10.0f)
            model->voiced = true;

    if (model->voiced == true) {
        if (eratio < -10.0f)
            model->voiced = false;

        if ((eratio < -4.0f) && (model->Wo <= SIXTY))
            model->voiced = false;
    }
}
