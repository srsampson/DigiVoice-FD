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

#define NW                  279
#define SIXTY               (TAU * 60.0f / FS)
#define FRACTPI             (0.9497f * M_PI) /* 0.95 in binary */
#define ONE_ON_R            (1.0f / (TAU / FFT_SIZE))
    
int encode_harmonics_create(void);
void encode_harmonics_destroy(void);

void analyzeOneSegment(ENCODE_MODEL *, int16_t []);

#ifdef __cplusplus
}
#endif
