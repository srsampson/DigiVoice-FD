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
    
#include "codec.h"
#include "defines.h"

#define AMP_K               20  /* rate K vector length                            */
    
void indexToModels(DECODE_MODEL [], uint16_t []);

float decodeEnergy(uint16_t);

#ifdef __cplusplus
}
#endif
