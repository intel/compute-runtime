/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/basic_math.h"

#include "CL/cl.h"

#include <utility>

namespace NEO {

inline int32_t selectNormalizingFactor(const cl_channel_type &channelType) {
    if (channelType == CL_UNORM_INT8) {
        return 0xFF;
    }
    if (channelType == CL_SNORM_INT8) {
        return 0x7F;
    }
    if (channelType == CL_UNORM_INT16) {
        return 0xFFFF;
    }
    if (channelType == CL_SNORM_INT16) {
        return 0x7fFF;
    }
    return 0;
}

inline void convertFillColor(const void *fillColor,
                             int32_t *iFillColor,
                             const cl_image_format &oldImageFormat,
                             const cl_image_format &newImageFormat) {
    float fFillColor[4] = {0.0f};

    for (auto i = 0; i < 4; i++) {
        iFillColor[i] = *((int32_t *)fillColor + i);
        fFillColor[i] = *((float *)fillColor + i);
    }

    if (oldImageFormat.image_channel_order == CL_A) {
        std::swap(iFillColor[0], iFillColor[3]);
        std::swap(fFillColor[0], fFillColor[3]);
    } else if (oldImageFormat.image_channel_order == CL_BGRA || oldImageFormat.image_channel_order == CL_sBGRA) {
        std::swap(iFillColor[0], iFillColor[2]);
        std::swap(fFillColor[0], fFillColor[2]);
    }

    if (oldImageFormat.image_channel_order == CL_sRGBA || oldImageFormat.image_channel_order == CL_sBGRA) {
        for (auto i = 0; i < 3; i++) {
            if (fFillColor[i] != fFillColor[i]) {
                fFillColor[i] = 0.0f;
            }
            if (fFillColor[i] > 1.0f) {
                fFillColor[i] = 1.0f;
            } else if (fFillColor[i] < 0.0f) {
                fFillColor[i] = 0.0f;
            } else if (fFillColor[i] < 0.0031308f) {
                fFillColor[i] = 12.92f * fFillColor[i];
            } else {
                fFillColor[i] = 1.055f * pow(fFillColor[i], 1.0f / 2.4f) - 0.055f;
            }
        }
    }

    if (newImageFormat.image_channel_data_type == CL_UNSIGNED_INT8) {
        auto normalizingFactor = selectNormalizingFactor(oldImageFormat.image_channel_data_type);
        if (normalizingFactor > 0) {
            for (auto i = 0; i < 4; i++) {
                if ((oldImageFormat.image_channel_order == CL_sRGBA || oldImageFormat.image_channel_order == CL_sBGRA) && i < 3) {
                    iFillColor[i] = static_cast<int32_t>(normalizingFactor * fFillColor[i] + 0.5f);
                } else {
                    iFillColor[i] = static_cast<int32_t>(normalizingFactor * fFillColor[i]);
                }
            }
        }

        for (auto i = 0; i < 4; i++) {
            iFillColor[i] = iFillColor[i] & 0xFF;
        }
    } else if (newImageFormat.image_channel_data_type == CL_UNSIGNED_INT16) {
        auto normalizingFactor = selectNormalizingFactor(oldImageFormat.image_channel_data_type);
        if (normalizingFactor > 0) {
            for (auto i = 0; i < 4; i++) {
                iFillColor[i] = static_cast<int32_t>(normalizingFactor * fFillColor[i]);
            }
        } else if (oldImageFormat.image_channel_data_type == CL_HALF_FLOAT) {
            //float to half convert.
            for (auto i = 0; i < 4; i++) {
                uint16_t temp = Math::float2Half(fFillColor[i]);
                iFillColor[i] = temp;
            }
        }

        for (auto i = 0; i < 4; i++) {
            iFillColor[i] = iFillColor[i] & 0xFFFF;
        }
    }
}
} // namespace NEO
