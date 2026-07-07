/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/helpers/leo_gmm_types_converter.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_texture.h"
#include "level_zero/api/opencl/source/sharings/gl/linux/leo_gl_sharing_linux.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"

#include "CL/cl_gl.h"
#include "config.h"
#include "drm_fourcc.h"
#include <GL/gl.h>

namespace NEO {
namespace LEO {

Image *GlTexture::createSharedGlTexture(Context *context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture,
                                        cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    struct mesa_glinterop_export_in texIn = {};
    struct mesa_glinterop_export_out texOut = {};
    struct mesa_glinterop_flush_out flushOut = {};
    int fenceFd = -1;

    texIn.version = 2;
    texIn.target = getBaseTargetType(target);
    texIn.obj = texture;
    texIn.miplevel = miplevel;

    switch (flags) {
    case CL_MEM_READ_ONLY:
        texIn.access = MESA_GLINTEROP_ACCESS_READ_ONLY;
        break;
    case CL_MEM_WRITE_ONLY:
        texIn.access = MESA_GLINTEROP_ACCESS_WRITE_ONLY;
        break;
    case CL_MEM_READ_WRITE:
        texIn.access = MESA_GLINTEROP_ACCESS_READ_WRITE;
        break;
    default:
        errorCode.set(CL_INVALID_VALUE);
        return nullptr;
    }

    auto clMemObjectType = getClMemObjectType(target);
    if (clMemObjectType == 0) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "target %x not supported\n", target);
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

    flushOut.version = 1;
    flushOut.fence_fd = &fenceFd;

    texOut.version = 2;

    GLSharingFunctionsLinux *sharingFunctions = context->getSharing<GLSharingFunctionsLinux>();
    int retValue;

    bool success = sharingFunctions->flushObjectsAndWait(1, &texIn, &flushOut, &retValue);
    if (success) {
        retValue = sharingFunctions->exportObject(&texIn, &texOut);
    }

    if (!success || (retValue != MESA_GLINTEROP_SUCCESS) || (texOut.version != 2)) {
        switch (retValue) {
        case MESA_GLINTEROP_INVALID_DISPLAY:
        case MESA_GLINTEROP_INVALID_CONTEXT:
            errorCode.set(CL_INVALID_CONTEXT);
            break;
        case MESA_GLINTEROP_INVALID_TARGET:
            errorCode.set(CL_INVALID_VALUE);
            break;
        case MESA_GLINTEROP_INVALID_MIP_LEVEL:
            errorCode.set(CL_INVALID_MIP_LEVEL);
            break;
        case MESA_GLINTEROP_INVALID_OBJECT:
            errorCode.set(CL_INVALID_GL_OBJECT);
            break;
        case MESA_GLINTEROP_UNSUPPORTED:
            errorCode.set(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
            break;
        case MESA_GLINTEROP_INVALID_OPERATION:
            errorCode.set(CL_INVALID_OPERATION);
            break;
        case MESA_GLINTEROP_OUT_OF_HOST_MEMORY:
            errorCode.set(CL_OUT_OF_HOST_MEMORY);
            break;
        case MESA_GLINTEROP_OUT_OF_RESOURCES:
        default:
            errorCode.set(CL_OUT_OF_RESOURCES);
            break;
        }
        return nullptr;
    }

    CL_GL_RESOURCE_INFO texInfo = {};
    texInfo.name = texIn.obj;
    texInfo.globalShareHandle = static_cast<unsigned int>(texOut.dmabuf_fd);
    texInfo.glInternalFormat = static_cast<GLint>(texOut.internal_format);
    texInfo.textureBufferSize = static_cast<GLint>(texOut.buf_size);
    texInfo.textureBufferOffset = static_cast<GLint>(texOut.buf_offset);

    cl_image_format imgFormat{};
    if (setClImageFormat(texInfo.glInternalFormat, imgFormat) == false) {
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

    L0::ze_image_tiling_ext_desc_t l0TilingDesc{};
    switch (texOut.modifier) {
    case DRM_FORMAT_MOD_LINEAR:
        l0TilingDesc.linearStorage = true;
        break;
    case I915_FORMAT_MOD_X_TILED:
        l0TilingDesc.forceTiling = ImageTilingMode::tiledX;
        break;
    case I915_FORMAT_MOD_Y_TILED:
        l0TilingDesc.forceTiling = ImageTilingMode::tiledY;
        break;
    case I915_FORMAT_MOD_Yf_TILED:
        l0TilingDesc.forceTiling = ImageTilingMode::tiledYf;
        break;
    case I915_FORMAT_MOD_4_TILED:
        l0TilingDesc.forceTiling = ImageTilingMode::tiled4;
        break;
    default:
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Unsupported DRM format modifier 0x%llx in CL-GL sharing\n",
                     static_cast<unsigned long long>(texOut.modifier));
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }
    l0TilingDesc.rowPitch = static_cast<uint64_t>(texOut.stride);

    ze_external_memory_import_fd_t l0imageHandleDesc{ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD};
    l0imageHandleDesc.fd = static_cast<int>(texOut.dmabuf_fd);
    l0imageHandleDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    l0imageHandleDesc.pNext = &l0TilingDesc;

    ze_image_desc_t l0imageDesc{ZE_STRUCTURE_TYPE_IMAGE_DESC};
    l0imageDesc.pNext = &l0imageHandleDesc;
    l0imageDesc.type = Image::clToL0ImageType(clMemObjectType);
    l0imageDesc.miplevels = std::max(miplevel, 0);

    const auto exportWidth = static_cast<uint32_t>(texOut.width);
    const auto exportHeight = static_cast<uint32_t>(texOut.height);
    const auto exportDepth = static_cast<uint32_t>(texOut.depth);

    switch (clMemObjectType) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        l0imageDesc.width = exportWidth;
        l0imageDesc.height = 1;
        l0imageDesc.depth = 1;
        l0imageDesc.arraylevels = 0;
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        l0imageDesc.width = exportWidth;
        l0imageDesc.height = 1;
        l0imageDesc.depth = 1;
        l0imageDesc.arraylevels = exportHeight;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        l0imageDesc.width = exportWidth;
        l0imageDesc.height = exportHeight;
        l0imageDesc.depth = 1;
        l0imageDesc.arraylevels = 0;
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        l0imageDesc.width = exportWidth;
        l0imageDesc.height = exportHeight;
        l0imageDesc.depth = 1;
        l0imageDesc.arraylevels = exportDepth;
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        l0imageDesc.width = exportWidth;
        l0imageDesc.height = exportHeight;
        l0imageDesc.depth = std::max(exportDepth, 1u);
        l0imageDesc.arraylevels = 0;
        break;
    default:
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

    Image::clToL0ImageFormat(l0imageDesc.format, imgFormat.image_channel_order, imgFormat.image_channel_data_type);

    ze_srgb_ext_desc_t srgbExtDesc{ZE_STRUCTURE_TYPE_SRGB_EXT_DESC, l0imageDesc.pNext, Image::isSRGB(imgFormat.image_channel_order)};
    l0imageDesc.pNext = &srgbExtDesc;

    L0::ze_depth_stencil_format_ext_desc_t depthStencilDesc{};
    if (imgFormat.image_channel_order == CL_DEPTH_STENCIL) {
        if (imgFormat.image_channel_data_type == CL_UNORM_INT24) {
            depthStencilDesc.format = L0::ZE_DEPTH_STENCIL_FORMAT_D24_UNORM_S8_UINT;
        } else {
            depthStencilDesc.format = L0::ZE_DEPTH_STENCIL_FORMAT_D32_FLOAT_S8X24_UINT;
        }
        depthStencilDesc.pNext = l0imageDesc.pNext;
        l0imageDesc.pNext = &depthStencilDesc;
        imgFormat.image_channel_order = CL_DEPTH;
    }

    ze_image_handle_t imageHandle{};
    auto result = zeImageCreate(context->getL0ContextHandle(), context->getClDevice()->getL0Handle(), &l0imageDesc, &imageHandle);
    if (result != ZE_RESULT_SUCCESS) {
        errorCode.set(L0ToClResultMapper(result));
        return nullptr;
    }

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto image = new Image(context, memoryProperties, flags, imageHandle, nullptr, nullptr, false, imgFormat, nullptr);

    auto glTexture = new GlTexture(sharingFunctions, getClGlObjectType(target), texture, texInfo, target, std::max(miplevel, 0));
    image->setSharingHandler(glTexture);

    return image;
}

void GlTexture::synchronizeObject(UpdateData &updateData) {
    auto sharingFunctions = static_cast<GLSharingFunctionsLinux *>(this->sharingFunctions);

    /* Prepare flush request */
    struct mesa_glinterop_export_in texIn = {};
    struct mesa_glinterop_flush_out syncOut = {};
    int fenceFd = -1;

    texIn.version = 2;
    texIn.target = this->target;
    texIn.obj = this->clGlObjectId;
    texIn.miplevel = this->miplevel;

    syncOut.version = 1;
    syncOut.fence_fd = &fenceFd;

    bool success = sharingFunctions->flushObjectsAndWait(1, &texIn, &syncOut);
    updateData.synchronizationStatus = success ? SynchronizeStatus::ACQUIRE_SUCCESFUL : SynchronizeStatus::SYNCHRONIZE_ERROR;
}

cl_int GlTexture::getGlTextureInfo(cl_gl_texture_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const {
    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    if (paramName == CL_GL_TEXTURE_TARGET) {
        info.set<GLenum>(target);
    } else if (paramName == CL_GL_MIPMAP_LEVEL) {
        info.set<GLenum>(miplevel);
    } else if (paramName == CL_GL_NUM_SAMPLES) {
        info.set<GLsizei>(textureInfo.numberOfSamples > 1 ? textureInfo.numberOfSamples : 0);
    } else {
        return CL_INVALID_VALUE;
    }
    return CL_SUCCESS;
}

cl_mem_object_type GlTexture::getClMemObjectType(cl_GLenum glType) {
    return static_cast<cl_mem_object_type>(getClObjectType(glType, false));
}
cl_gl_object_type GlTexture::getClGlObjectType(cl_GLenum glType) {
    return static_cast<cl_gl_object_type>(getClObjectType(glType, true));
}

uint32_t GlTexture::getClObjectType(cl_GLenum glType, bool returnClGlObjectType) {
    // return cl_gl_object_type if returnClGlObjectType is true, otherwise cl_mem_object_type
    uint32_t retValue = 0;
    switch (glType) {
    case GL_TEXTURE_1D:
        retValue = returnClGlObjectType ? CL_GL_OBJECT_TEXTURE1D : CL_MEM_OBJECT_IMAGE1D;
        break;
    case GL_TEXTURE_1D_ARRAY:
        retValue = returnClGlObjectType ? CL_GL_OBJECT_TEXTURE1D_ARRAY : CL_MEM_OBJECT_IMAGE1D_ARRAY;
        break;
    case GL_TEXTURE_2D:
    case GL_TEXTURE_RECTANGLE:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_2D_MULTISAMPLE:
        retValue = returnClGlObjectType ? CL_GL_OBJECT_TEXTURE2D : CL_MEM_OBJECT_IMAGE2D;
        break;
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        retValue = returnClGlObjectType ? CL_GL_OBJECT_TEXTURE2D_ARRAY : CL_MEM_OBJECT_IMAGE2D_ARRAY;
        break;
    case GL_TEXTURE_3D:
        retValue = returnClGlObjectType ? CL_GL_OBJECT_TEXTURE3D : CL_MEM_OBJECT_IMAGE3D;
        break;
    case GL_TEXTURE_BUFFER:
        retValue = returnClGlObjectType ? CL_GL_OBJECT_TEXTURE_BUFFER : CL_MEM_OBJECT_IMAGE1D_BUFFER;
        break;
    case GL_RENDERBUFFER_EXT:
        retValue = returnClGlObjectType ? CL_GL_OBJECT_RENDERBUFFER : CL_MEM_OBJECT_IMAGE2D;
        break;
    default:
        retValue = 0;
        break;
    }
    return retValue;
}

cl_GLenum GlTexture::getBaseTargetType(cl_GLenum target) {
    cl_GLenum returnTarget = 0;
    switch (target) {
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        returnTarget = GL_TEXTURE_CUBE_MAP_ARB;
        break;
    default:
        returnTarget = target;
        break;
    }
    return returnTarget;
}

void GlTexture::releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) {
}

void GlTexture::resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) {
}

} // namespace LEO
} // namespace NEO
