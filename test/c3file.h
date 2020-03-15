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

#define CODEC2_MODE_700C     8

#define CODEC2_VERSION_MAJOR 0
#define CODEC2_VERSION_MINOR 9
#define CODEC2_VERSION_PATCH 2
#define CODEC2_VERSION "0.9.2"
    
const char c3_file_magic[] = {0xc1, 0xdf, 0xc3};

struct c3_header {
    char magic[3];
    char version_major;
    char version_minor;
    char mode;
    char flags;
};

#ifdef __cplusplus
}
#endif
