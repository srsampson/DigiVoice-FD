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
#include <stdint.h>
    
#include "codec.h"
#include "defines.h"
#include "encode_fft.h"

#define WO_MIN      (TAU / P_MAX)
#define WO_MAX      (TAU / P_MIN)
#define WO_DIFF     (log10f(WO_MAX) - log10f(WO_MIN))
#define WO_LEVELS   (1 << 6)
    
#define ENERGY_M    16

#define AMP_K               20  /* rate K vector length                            */
#define AMP_M               512 /* number of elements in codebook                  */
    
void modelToIndex(uint16_t index[], ENCODE_MODEL *model);

#ifdef __cplusplus
}
#endif
