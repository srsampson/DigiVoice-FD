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
#include <complex.h>
#include <string.h>
#include <math.h>

#include "defines.h"
#include "encode_harmonics.h"
#include "encode_pitch.h"
#include "codec.h"
#include "encode_vqbest.h"
#include "encode_index.h"
#include "decode_harmonics.h"
#include "decode_index.h"

/*
 * This version is designed for Full-Duplex operation, where
 * the encoder and decoder are operating at the same time.
 */
static ENCODE_MODEL EncodeModel;
static DECODE_MODEL DecodeModels[N_MODELS];

int codec_create() {
    if (encode_harmonics_create() != 0) {
        return -1;
    }

    if (decode_harmonics_create() != 0) {
        return -2;
    }

    if (encodePitchCreate() != 0) {
        return -3;
    }

    return 0;
}

void codec_destroy() {
    encode_harmonics_destroy();
    encodePitchDestroy();
    decode_harmonics_destroy();
}

int codec_indexes_per_frame() {
    return 4;
}

int codec_samples_per_frame() {
    return 320;
}

/*
 * Encodes frames of 320 samples of 15-bit + sign PCM
 * speech into an array index of bits.
 * 
 * 40ms segments, or at a 25 Hz rate
 *
 * index[0] = VQ magnitude1 (9 bits)
 * index[1] = VQ magnitude2 (9 bits)
 * index[2] = energy        (4 bits)
 * index[3] = pitch         (6 bits)
 */
void codec_encode(uint16_t index[], int16_t speech[]) {
    /*
     * Process each 10 ms segment and update model
     * Only last model update gets used going forward
     */
    for (int i = 0; i < N_MODELS; i++) {
        analyzeOneSegment(&EncodeModel, &speech[N_SAMP * i]);
    }

    /*
     * Convert the model into the indexed bits
     */
    modelToIndex(index, &EncodeModel);
}

/*
 * Decodes array of indexed bits into 320 samples of speech
 * 
 * 40ms segments, or at a 25 Hz rate
 *
 * index[0] = VQ magnitude1 (9 bits)
 * index[1] = VQ magnitude2 (9 bits)
 * index[2] = energy        (4 bits)
 * index[3] = pitch         (6 bits)
 */
void codec_decode(int16_t speech[], uint16_t index[]) {

    indexToModels(DecodeModels, index);

    for (int i = 0; i < N_MODELS; i++) {
        synthesizeOneSegment(&speech[N_SAMP * i], &DecodeModels[i]);
    }
}

/*
 * Decodes energy value from encoded bits
 */
float codec_get_energy(uint16_t index[]) {
    float mean = decodeEnergy(index[2]) - 10.0f;

    /* Decrease mean if unvoiced */

    if (index[3] == 0) // pitch == 0 means unvoiced
        mean -= 10.0f;

    return powf(10.0f, mean / 10.0f);
}
