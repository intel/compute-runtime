/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "opencl/source/helpers/windows/gl_helper.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"
#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace Os {
extern const char *openglDllName;
}

namespace NEO {

cl_int GLSharingFunctions::getSupportedFormats(cl_mem_flags flags,
                                               cl_mem_object_type imageType,
                                               size_t numEntries,
                                               cl_GLenum *formats,
                                               uint32_t *numImageFormats) {
    if (flags != CL_MEM_READ_ONLY && flags != CL_MEM_WRITE_ONLY && flags != CL_MEM_READ_WRITE && flags != CL_MEM_KERNEL_READ_AND_WRITE) {
        return CL_INVALID_VALUE;
    }

    if (imageType != CL_MEM_OBJECT_IMAGE1D && imageType != CL_MEM_OBJECT_IMAGE2D &&
        imageType != CL_MEM_OBJECT_IMAGE3D && imageType != CL_MEM_OBJECT_IMAGE1D_ARRAY &&
        imageType != CL_MEM_OBJECT_IMAGE1D_BUFFER) {
        return CL_INVALID_VALUE;
    }

    const auto formatsCount = GlSharing::gLToCLFormats.size();
    if (numImageFormats != nullptr) {
        *numImageFormats = static_cast<cl_uint>(formatsCount);
    }

    if (formats != nullptr && formatsCount > 0) {
        auto elementsToCopy = std::min(numEntries, formatsCount);
        uint32_t i = 0;
        for (auto &x : GlSharing::gLToCLFormats) {
            formats[i++] = x.first;
            if (i == elementsToCopy) {
                break;
            }
        }
    }

    return CL_SUCCESS;
}
} // namespace NEO
