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

#include <stdint.h>

#include "defines.h"

#define TW_S                0.005           /* trapezoidal synth window overlap     */
#define TW                  (FS * TW_S)     /* 40  */
#define NW                  279
#define SIXTY               (TAU * 60.0f / FS)          /* 0.001193663 */
#define NS                  (PHASE_FFT_SIZE / 2 + 1)    /* 65 (0 - 64) */
#define SCALE               (20.0f / logf(10.0f))
#define FRACTPI             (0.9497f * M_PI) /* 0.95 in binary */
    
#define CODEC2_RND_MAX      32767.0f

#define BG_THRESH           40.0f
#define BG_BETA             0.1f
#define BG_MARGIN           6.0f
    
#define ONE_ON_R            (1.0f / (TAU / FFT_SIZE))
    
int decode_harmonics_create(void);
void decode_harmonics_destroy(void);

void magToPhase(float [], float []);
void synthesizeOneSegment(int16_t [], DECODE_MODEL *);

#ifdef __cplusplus
}
#endif
