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

#include "defines.h"

#define COEFF       0.9497f /* notch filter parameter .95 in binary */
#define DEC         5       /* decimation factor                    */
#define CNLP        .3f     /* post processor constant              */
#define NLP_NTAP    48      /* Decimation LPF order                 */
#define MIN_BIN     (FFT_SIZE * DEC / P_MAX)    /* 16 */

int encodePitchCreate(void);
void encodePitchDestroy(void);
int encodeDetectPitch(float []);

#ifdef __cplusplus
}
#endif

