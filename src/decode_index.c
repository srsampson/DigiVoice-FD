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

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "codec.h"
#include "defines.h"
#include "decode_harmonics.h"
#include "decode_index.h"

static void interpPara(float [], float [], float [], int, float [], int);
static void postFilterAmp(float []);
static void interpWov(float [], int [], bool [], float, bool);
static void resampleRateL(DECODE_MODEL *, int);
static void determinePhase(DECODE_MODEL *);
static void indexToRateKVec(float [], uint16_t []);
static float decodePitch(uint16_t);

static float AmpInterpolatedSurface_[N_MODELS][AMP_K];
static float AmpPrevRateKVec_[AMP_K];
static float AmpWoLeft;
static bool AmpVoicingLeft;

/*
 * Convert the quantized and indexed data bits back into a model
 * 
 * index[0] = VQ magnitude1 (9 bits)
 * index[1] = VQ magnitude2 (9 bits)
 * index[2] = energy        (4 bits)
 * index[3] = pitch         (6 bits)
 */
void indexToModels(DECODE_MODEL models[], uint16_t index[]) {
    float vec_[AMP_K];

    /* extract latest rate K vector */

    float Wo_right;
    bool voicing_right;

    indexToRateKVec(vec_, index);

    /* decode latest Wo and voicing */

    if (index[3] == 0) {
        /*
         * If pitch is zero, it is a code
         * to signal an unvoiced frame
         */
        Wo_right = TAU / 100.0f;
        voicing_right = false;
    } else {
        Wo_right = decodePitch(index[3]);
        voicing_right = true;
    }

    /* (linearly) interpolate 25Hz amplitude vectors back to 100Hz */

    float c;
    int i, k;

    for (i = 0, c = 1.0f; i < N_MODELS; i++, c -= 1.0f / N_MODELS) {
        for (k = 0; k < AMP_K; k++) {
            AmpInterpolatedSurface_[i][k] = 
                    AmpPrevRateKVec_[k] * c + vec_[k] * (1.0f - c);
        }
    }

    /* interpolate 25Hz v and Wo back to 100Hz */

    float aWo_[N_MODELS];
    bool avoicing_[N_MODELS];
    int aL_[N_MODELS];

    interpWov(aWo_, aL_, avoicing_, Wo_right, voicing_right);

    /* back to rate L amplitudes, synthesis phase for each frame */

    for (int i = 0; i < N_MODELS; i++) {
        models[i].Wo = aWo_[i];
        models[i].L = aL_[i];
        models[i].voiced = avoicing_[i];

        resampleRateL(&models[i], i);
        determinePhase(&models[i]);
    }

    /* update memories for next time */

    for (int i = 0; i < AMP_K; i++) {
        AmpPrevRateKVec_[i] = vec_[i];
    }

    AmpWoLeft = Wo_right;
    AmpVoicingLeft = voicing_right;
}

/*
 * A post filter is the key to the (relatively) high quality at such low bit rates.
 * The way it works is a little mysterious - and also a good topic for research.
 */
static void postFilterAmp(float vec[]) {
    float e_before = 0.0f;
    float e_after = 0.0f;

    for (int k = 0; k < AMP_K; k++) {
        vec[k] += AmpPre[k];
        e_before += (powf(10.0f, 2.0f * vec[k] / 20.0f));
        
        vec[k] *= 1.5f;
        e_after += (powf(10.0f, 2.0f * vec[k] / 20.0f));
    }

    float gaindB = 10.0f * log10f(e_after / e_before);

    for (int k = 0; k < AMP_K; k++) {
        vec[k] -= gaindB;
        vec[k] -= AmpPre[k];
    }
}

static void interpPara(float result[], float xp[], float yp[], int np, float x[], int n) {
    int k = 0;
    
    for (int i = 0; i < n; i++) {
        float xi = x[i];

        /* k is index into xp of where we start 3 points used to form parabola */

        while ((xp[k + 1] < xi) && (k < (np - 3)))
            k++;

        float x1 = xp[k];
        float y1 = yp[k];
        float x2 = xp[k + 1];
        float y2 = yp[k + 1];
        float x3 = xp[k + 2];
        float y3 = yp[k + 2];

        float a = ((y3 - y2) / (x3 - x2) - (y2 - y1) / (x2 - x1)) / (x3 - x1);
        float b = ((y3 - y2) / (x3 - x2) * (x2 - x1) + (y2 - y1) / (x2 - x1) * (x3 - x2)) / (x3 - x1);

        result[i] = a * (xi - x2) * (xi - x2) + b * (xi - x2) + y2;
    }
}

static void indexToRateKVec(float vec_[], uint16_t index[]) {
    float vec_no_mean_[AMP_K];
    int n1 = index[0];  // VQ1 Magnitude
    int n2 = index[1];  // VQ2 Magnitude

    for (int k = 0; k < AMP_K; k++) {
        vec_no_mean_[k] = Codebook1[AMP_K * n1 + k] + Codebook2[AMP_K * n2 + k];
    }

    postFilterAmp(vec_no_mean_);

    float mean = decodeEnergy(index[2]);     // 4 bits

    for (int k = 0; k < AMP_K; k++) {
        vec_[k] = vec_no_mean_[k] + mean;
    }
}

static void interpWov(float Wo_[], int L_[], bool voicing_[], float Wo2, bool voicing_right) {
    int i;

    float tval = TAU / 100.0f;
    
    for (i = 0; i < N_MODELS; i++) {
        voicing_[i] = false;
    }

    if (!AmpVoicingLeft && !voicing_right) {
        for (i = 0; i < N_MODELS; i++) {
            Wo_[i] = tval;
        }
    }

    if (AmpVoicingLeft && !voicing_right) {
        Wo_[0] = Wo_[1] = AmpWoLeft;
        Wo_[2] = Wo_[3] = tval;
        
        voicing_[0] = voicing_[1] = true;
    }

    if (!AmpVoicingLeft && voicing_right) {
        Wo_[0] = Wo_[1] = tval;
        Wo_[2] = Wo_[3] = Wo2;
        
        voicing_[2] = voicing_[3] = true;
    }

    if (AmpVoicingLeft && voicing_right) {
        float c = 1.0f;
        
        for (i = 0; i < N_MODELS; i++) {
            Wo_[i] = AmpWoLeft * c + Wo2 * (1.0f - c);
            
            voicing_[i] = true;
            c -= 0.025f;        // 1 / N_MODELS
        }
    }

    for (i = 0; i < N_MODELS; i++) {
        L_[i] = floorf(M_PI / Wo_[i]);
    }
}

static void resampleRateL(DECODE_MODEL *model, int index) {
    float rate_K_vec_term[AMP_K + 2];
    float rate_K_sample_freqs_kHz_term[AMP_K + 2];
    float amdB[MAX_AMP + 1];
    float rate_L_sample_freqs_kHz[MAX_AMP + 1];

    /* init to zero in case we dump later for debug 0..80 */
    
    for (int m = 0; m <= MAX_AMP; m++) {
        model->A[m] = 0.0f;
    }

    /* terminate either end of the rate K vecs with 0dB points */

    rate_K_vec_term[0] = rate_K_vec_term[AMP_K + 1] = 0.0f;
    
    rate_K_sample_freqs_kHz_term[0] = 0.0f;
    rate_K_sample_freqs_kHz_term[AMP_K + 1] = 4.0f;

    for (int k = 0; k < AMP_K; k++) {
        rate_K_vec_term[k + 1] = AmpInterpolatedSurface_[index][k];
        rate_K_sample_freqs_kHz_term[k + 1] = Amp_freqs_kHz[k];
    }

    float tval = model->Wo * 4.0f / M_PI;
    
    for (int m = 1; m <= model->L; m++) {
        rate_L_sample_freqs_kHz[m] = m * tval;
    }

    interpPara(&amdB[1], rate_K_sample_freqs_kHz_term, rate_K_vec_term, AMP_K + 2,
            &rate_L_sample_freqs_kHz[1], model->L);

    for (int m = 1; m <= model->L; m++) {
        model->A[m] = powf(10.0f, amdB[m] / 20.0f);
    }
}

static void determinePhase(DECODE_MODEL *model) {
    float rate_L_sample_freqs_kHz[MAX_AMP + 1];
    float amdB[MAX_AMP + 1];
    float Gdbfk[NS];
    float sample_freqs_kHz[NS];
    float phase[NS];

    float tval = model->Wo * 4.0f / M_PI;
    
    for (int m = 1; m <= model->L; m++) {
        amdB[m] = 20.0f * log10f(model->A[m]);
        rate_L_sample_freqs_kHz[m] = (float) m * tval;
    }

    for (int i = 0; i < NS; i++) {
        sample_freqs_kHz[i] = 8.0f * (float) i / PHASE_FFT_SIZE;
    }

    interpPara(Gdbfk, &rate_L_sample_freqs_kHz[1], &amdB[1], model->L, sample_freqs_kHz, NS);

    magToPhase(phase, Gdbfk);

    tval = model->Wo * (float) PHASE_FFT_SIZE / TAU;
    
    for (int m = 1; m <= model->L; m++) {
        int b = floorf(0.5f + (float) m * tval);
        model->H[m] = cmplx(phase[b]);
    }
}

float decodeEnergy(uint16_t index) {
    return EnergyTable[index & 0x0F];   // 4 bits
}

static float decodePitch(uint16_t index) {
    return PitchTable[index & 0x3F];    // 6 bits
}
