/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/image_imp.h"

#define GEN10PLUS_IMAGE_FORMATS()                                                 \
    {                                                                             \
        mediaSurfaceFormatTable                                                   \
            [ZE_IMAGE_FORMAT_LAYOUT_P016 - ZE_IMAGE_FORMAT_MEDIA_LAYOUT_OFFSET] = \
                RSS::SURFACE_FORMAT_PLANAR_420_16;                                \
    }

#define GEN11PLUS_IMAGE_FORMATS() \
    {                             \
        GEN10PLUS_IMAGE_FORMATS() \
    }

#define GEN12PLUS_IMAGE_FORMATS()                                             \
    {                                                                         \
        GEN11PLUS_IMAGE_FORMATS()                                             \
        surfaceFormatTable                                                    \
            [ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2][ZE_IMAGE_FORMAT_TYPE_FLOAT] = \
                RSS::SURFACE_FORMAT_R10G10B10_FLOAT_A2_UNORM;                 \
    }
