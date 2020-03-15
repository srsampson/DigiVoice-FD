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

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "encode_vqbest.h"
#include "encode_index.h"

static void mbestInsert(struct MBEST *, uint16_t [], float);

struct MBEST *mbestCreate() {
    struct MBEST *mbest = malloc(sizeof (struct MBEST));

    mbest->list = malloc(MBEST_ENTRIES * sizeof (struct MBEST_LIST));

    for (int i = 0; i < MBEST_ENTRIES; i++) {
        for (int j = 0; j < MBEST_STAGES; j++)
            mbest->list[i].index[j] = 0;
        
        mbest->list[i].error = 1E32f;
    }

    return mbest;
}

void mbestDestroy(struct MBEST *mbest) {
    free(mbest->list);
    free(mbest);
}

void mbestSearch(const float *cb, float vec[], struct MBEST *mbest, uint16_t index[]) {
    for (int j = 0; j < AMP_M; j++) {
        float error = 0.0f;
        
        for (int i = 0; i < AMP_K; i++) {
            float diff = cb[j * AMP_K + i] - vec[i];
            error += (diff * diff);
        }
        
        index[0] = j;
        mbestInsert(mbest, index, error);
    }
}

static void mbestInsert(struct MBEST *mbest, uint16_t index[], float error) {
    struct MBEST_LIST *list = mbest->list;

    bool found = false;

    for (int i = 0; i < MBEST_ENTRIES && (found == false); i++) {
        if (error < list[i].error) {

            found = true;
            
            for (int j = MBEST_ENTRIES - 1; j > i; j--)
                list[j] = list[j - 1];
            
            for (int j = 0; j < MBEST_STAGES; j++)
                list[i].index[j] = index[j];
            
            list[i].error = error;
        }
    }
}
