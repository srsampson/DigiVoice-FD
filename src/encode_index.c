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
#include "encode_harmonics.h"
#include "encode_vqbest.h"
#include "encode_index.h"

static void rateKMbestEncode(uint16_t [], float []);
static void interpPara(float [], float [], float [], int, const float [], int);
static void resampleConstRateF(float [], ENCODE_MODEL *);
static uint16_t encodeEnergy(float);
static uint16_t encodePitch(float);

static float AmpWoLeft;
static bool AmpVoicingLeft;

/*
 * Quantize the model parameters into indexed bits
 * This is the data that will be transmitted across the medium
 * 
 * index[0] = VQ magnitude1 (9 bits)
 * index[1] = VQ magnitude2 (9 bits)
 * index[2] = energy        (4 bits)
 * index[3] = pitch         (6 bits)
 */
void modelToIndex(uint16_t index[], ENCODE_MODEL *model) {
    float vec[AMP_K];
    float vec_no_mean[AMP_K];

    /* convert variable rate L to fixed rate K */

    resampleConstRateF(vec, model);

    /* remove mean and two stage VQ */

    float sum = 0.0f;
    
    for (int k = 0; k < AMP_K; k++) {
        sum += vec[k];
    }

    float mean = sum / AMP_K;
    
    /* scalar quantize mean (effectively the frame energy) */

    index[2] = encodeEnergy(mean);     // energy 4 bits
    
    for (int k = 0; k < AMP_K; k++) {
        vec_no_mean[k] = vec[k] - mean;
    }

    rateKMbestEncode(index, vec_no_mean);    // VQ pair, each 9 bits

    /*
     * We steal the smallest Wo index to signal an unvoiced frame
     */

    if (model->voiced) {
        uint16_t pitch = encodePitch(model->Wo);

        if (pitch == 0) {
            pitch = 1;
        }

        index[3] = pitch;       // pitch 6 bits
    } else {
        index[3] = 0;
    }
}

static void interpPara(float result[], float xp[], float yp[], int np, const float x[], int n) {
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

static void resampleConstRateF(float vec[], ENCODE_MODEL *model) {
    float amdB[MAX_AMP + 1];
    float rate_L_sample_freqs_kHz[MAX_AMP + 1];

    /* convert rate L=pi/Wo amplitude samples to fixed rate K */

    float amdB_peak = -100.0f;
    float tval = model->Wo * 4.0f / M_PI;
    
    for (int m = 1; m <= model->L; m++) {
        amdB[m] = 20.0f * log10f(model->A[m] + 1E-16f);

        if (amdB[m] > amdB_peak) {
            amdB_peak = amdB[m];
        }

        rate_L_sample_freqs_kHz[m] = (float) m * tval;
    }

    /* clip between peak and peak -50dB, to reduce dynamic range */

    for (int m = 1; m <= model->L; m++) {
        if (amdB[m] < (amdB_peak - 50.0f)) {
            amdB[m] = amdB_peak - 50.0f;
        }
    }

    interpPara(vec, &rate_L_sample_freqs_kHz[1], &amdB[1], model->L, Amp_freqs_kHz, AMP_K);
}

static void rateKMbestEncode(uint16_t index[], float vec[]) {
    uint16_t entry[MBEST_STAGES];

    for (int i = 0; i < MBEST_STAGES; i++) {
        entry[i] = 0;
    }
    
    /* codebook is compiled for a fixed K */

    struct MBEST *mbest_stage1 = mbestCreate();
    struct MBEST *mbest_stage2 = mbestCreate();

    /* Stage 1 */

    mbestSearch(Codebook1, vec, mbest_stage1, entry);

    /* Stage 2 */

    float target[AMP_K];
    int n1;

    for (int j = 0; j < MBEST_ENTRIES; j++) {
        entry[1] = n1 = mbest_stage1->list[j].index[0];

        for (int i = 0; i < AMP_K; i++) {
            target[i] = vec[i] - Codebook1[AMP_K * n1 + i];
        }
        
        mbestSearch(Codebook2, target, mbest_stage2, entry);
    }

    index[0] = mbest_stage2->list[0].index[1];
    index[1] = mbest_stage2->list[0].index[0];

    mbestDestroy(mbest_stage1);
    mbestDestroy(mbest_stage2);
}

static uint16_t encodeEnergy(float energy) {
    uint16_t bestindex = 0;
    float besterror = 1E32f;

    for (int i = 0; i < ENERGY_M; i++) {
        float diff = EnergyTable[i] - energy;
        float error = (diff * diff);

        if (error < besterror) {
            besterror = error;
            bestindex = i;
        }
    }

    return bestindex & 0x0F;   // 4 bits
}

static uint16_t encodePitch(float wo) {
    uint16_t index = (uint16_t) floorf(WO_LEVELS *
                ((log10f(wo) - log10f(WO_MIN)) / WO_DIFF) + 0.5f);

    if (index < 0) {
        index = 0;
    } else if (index > (WO_LEVELS - 1)) {
        index = WO_LEVELS - 1;
    }

    return index & 0x3F;    // 6 bits
}
