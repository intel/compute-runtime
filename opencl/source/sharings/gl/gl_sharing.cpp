/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/gl/gl_sharing.h"

#include "shared/source/helpers/string.h"
#include "shared/source/helpers/timestamp_packet.h"

#include "opencl/source/context/context.inl"
#include "opencl/source/sharings/gl/gl_context_guard.h"
#include "opencl/source/sharings/sharing_factory.h"

#include <unordered_map>

namespace NEO {
const uint32_t GLSharingFunctions::sharingId = SharingType::CLGL_SHARING;

const std::unordered_map<GLenum, const cl_image_format> GlSharing::glToCLFormats = {
    {GL_RGBA8, {CL_RGBA, CL_UNORM_INT8}},
    {GL_RGBA8I, {CL_RGBA, CL_SIGNED_INT8}},
    {GL_RGBA16, {CL_RGBA, CL_UNORM_INT16}},
    {GL_RGBA16I, {CL_RGBA, CL_SIGNED_INT16}},
    {GL_RGBA32I, {CL_RGBA, CL_SIGNED_INT32}},
    {GL_RGBA8UI, {CL_RGBA, CL_UNSIGNED_INT8}},
    {GL_RGBA16UI, {CL_RGBA, CL_UNSIGNED_INT16}},
    {GL_RGBA32UI, {CL_RGBA, CL_UNSIGNED_INT32}},
    {GL_RGBA16F, {CL_RGBA, CL_HALF_FLOAT}},
    {GL_RGBA32F, {CL_RGBA, CL_FLOAT}},
    {GL_RGBA, {CL_RGBA, CL_UNORM_INT8}},
    {GL_RGBA8_SNORM, {CL_RGBA, CL_SNORM_INT8}},
    {GL_RGBA16_SNORM, {CL_RGBA, CL_SNORM_INT16}},
    {GL_BGRA, {CL_BGRA, CL_UNORM_INT8}},
    {GL_R8, {CL_R, CL_UNORM_INT8}},
    {GL_R8_SNORM, {CL_R, CL_SNORM_INT8}},
    {GL_R16, {CL_R, CL_UNORM_INT16}},
    {GL_R16_SNORM, {CL_R, CL_SNORM_INT16}},
    {GL_R16F, {CL_R, CL_HALF_FLOAT}},
    {GL_R32F, {CL_R, CL_FLOAT}},
    {GL_R8I, {CL_R, CL_SIGNED_INT8}},
    {GL_R16I, {CL_R, CL_SIGNED_INT16}},
    {GL_R32I, {CL_R, CL_SIGNED_INT32}},
    {GL_R8UI, {CL_R, CL_UNSIGNED_INT8}},
    {GL_R16UI, {CL_R, CL_UNSIGNED_INT16}},
    {GL_R32UI, {CL_R, CL_UNSIGNED_INT32}},
    {GL_DEPTH_COMPONENT32F, {CL_DEPTH, CL_FLOAT}},
    {GL_DEPTH_COMPONENT16, {CL_DEPTH, CL_UNORM_INT16}},
    {GL_DEPTH24_STENCIL8, {CL_DEPTH_STENCIL, CL_UNORM_INT24}},
    {GL_DEPTH32F_STENCIL8, {CL_DEPTH_STENCIL, CL_FLOAT}},
    {GL_SRGB8_ALPHA8, {CL_sRGBA, CL_UNORM_INT8}},
    {GL_RG8, {CL_RG, CL_UNORM_INT8}},
    {GL_RG8_SNORM, {CL_RG, CL_SNORM_INT8}},
    {GL_RG16, {CL_RG, CL_UNORM_INT16}},
    {GL_RG16_SNORM, {CL_RG, CL_SNORM_INT16}},
    {GL_RG16F, {CL_RG, CL_HALF_FLOAT}},
    {GL_RG32F, {CL_RG, CL_FLOAT}},
    {GL_RG8I, {CL_RG, CL_SIGNED_INT8}},
    {GL_RG16I, {CL_RG, CL_SIGNED_INT16}},
    {GL_RG32I, {CL_RG, CL_SIGNED_INT32}},
    {GL_RG8UI, {CL_RG, CL_UNSIGNED_INT8}},
    {GL_RG16UI, {CL_RG, CL_UNSIGNED_INT16}},
    {GL_RG32UI, {CL_RG, CL_UNSIGNED_INT32}},
    {GL_RGB10, {CL_RGBA, CL_UNORM_INT16}}};

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
        imageType != CL_MEM_OBJECT_IMAGE1D_BUFFER && imageType != CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        return CL_INVALID_VALUE;
    }

    const auto formatsCount = GlSharing::glToCLFormats.size();
    if (numImageFormats != nullptr) {
        *numImageFormats = static_cast<cl_uint>(formatsCount);
    }

    if (formats != nullptr && formatsCount > 0) {
        const auto elementsToCopy = std::min(numEntries, formatsCount);
        uint32_t outputFormatsIndex = 0;
        for (const auto &formatMapping : GlSharing::glToCLFormats) {
            formats[outputFormatsIndex++] = formatMapping.first;
            if (outputFormatsIndex == elementsToCopy) {
                break;
            }
        }
    }

    return CL_SUCCESS;
}

int GlSharing::synchronizeHandler(UpdateData &updateData) {
    GLContextGuard guard(*sharingFunctions);
    synchronizeObject(updateData);
    return CL_SUCCESS;
}

char *createArbSyncEventName() {
    static std::atomic<uint32_t> synchCounter{0};
    uint32_t id = synchCounter++;
    constexpr int maxDigitsForId = std::numeric_limits<uint32_t>::digits10;
    static const char prefix[] = "NEO_SYNC_";
    constexpr int nameMaxLen = sizeof(prefix) + maxDigitsForId + 1;
    char *ret = new char[nameMaxLen];

    snprintf(ret, nameMaxLen, "%s_%d", prefix, id);

    return ret;
}

void destroyArbSyncEventName(char *name) { delete[] name; }

template GLSharingFunctions *Context::getSharing<GLSharingFunctions>();

} // namespace NEO
