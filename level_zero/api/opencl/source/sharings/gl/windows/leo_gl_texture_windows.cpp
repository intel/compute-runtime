/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/api/opencl/extensions/public/cl_gl_private_intel.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/helpers/leo_gmm_types_converter.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_texture.h"
#include "level_zero/api/opencl/source/sharings/gl/windows/leo_gl_sharing_windows.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"

#include "CL/cl_gl.h"
#include "config.h"
#include <GL/gl.h>

namespace NEO {
namespace LEO {

Image *GlTexture::createSharedGlTexture(Context *context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture,
                                        cl_int *errcodeRet) {
    CL_GL_RESOURCE_INFO texInfo = {};
    texInfo.name = texture;
    texInfo.target = getBaseTargetType(target);

    GLSharingFunctionsWindows *sharingFunctions = context->getSharing<GLSharingFunctionsWindows>();

    if (target == GL_RENDERBUFFER_EXT) {
        sharingFunctions->acquireSharedRenderBuffer(&texInfo);
    } else {
        sharingFunctions->acquireSharedTexture(&texInfo);
    }

    ze_external_memory_import_win32_handle_t l0imageHandleDesc{ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32};
    l0imageHandleDesc.handle = reinterpret_cast<void *>(static_cast<uintptr_t>(texInfo.globalShareHandle));
    l0imageHandleDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;

    auto gmmHelper = context->getClDevice()->getDevice().getRootDeviceEnvironment().getGmmHelper();
    auto gmmResInfo = gmmHelper->getClientContext()->getGmmResInfoFromExternalResourceHandle(texInfo.pGmmResInfo);
    auto gmmResInfoMcs = gmmHelper->getClientContext()->getGmmResInfoFromExternalResourceHandle(texInfo.pGmmResInfoMCS);

    L0::ze_external_gl_texture_ext_desc_t l0glTextureExtDesc{};
    l0glTextureExtDesc.pNext = &l0imageHandleDesc;
    l0glTextureExtDesc.pGmmResInfo = gmmResInfo;
    l0glTextureExtDesc.textureBufferOffset = (target == GL_RENDERBUFFER_EXT) ? 0u : static_cast<uint64_t>(texInfo.textureBufferOffset);
    l0glTextureExtDesc.glHWFormat = static_cast<uint32_t>(texInfo.glHWFormat);
    l0glTextureExtDesc.cubeFaceIndex = static_cast<uint32_t>(GmmTypesConverter::getCubeFaceIndex(target));
    l0glTextureExtDesc.isAuxEnabled = texInfo.isAuxEnabled != 0;

    l0glTextureExtDesc.numberOfSamples = static_cast<uint32_t>(texInfo.numberOfSamples);
    l0glTextureExtDesc.mcsNtHandle = reinterpret_cast<void *>(static_cast<uintptr_t>(texInfo.globalShareHandleMCS));
    l0glTextureExtDesc.pGmmResInfoMcs = gmmResInfoMcs;

    ze_image_desc_t l0imageDesc{ZE_STRUCTURE_TYPE_IMAGE_DESC};
    l0imageDesc.pNext = &l0glTextureExtDesc;
    l0imageDesc.miplevels = miplevel;

    auto clType = getClMemObjectType(target);
    l0imageDesc.type = Image::clToL0ImageType(clType);

    if (gmmResInfo) {
        auto gmmResourceInfo = std::unique_ptr<NEO::GmmResourceInfo>(NEO::GmmResourceInfo::create(gmmHelper->getClientContext(), gmmResInfo));
        l0imageDesc.width = gmmResourceInfo->getBaseWidth();
        l0imageDesc.height = static_cast<uint32_t>(gmmResourceInfo->getBaseHeight());
        l0imageDesc.depth = static_cast<uint32_t>(gmmResourceInfo->getBaseDepth());
        l0imageDesc.arraylevels = static_cast<uint32_t>(gmmResourceInfo->getArraySize());
        l0glTextureExtDesc.hasUnifiedMcsSurface = (texInfo.numberOfSamples > 1) &&
                                                  (texInfo.globalShareHandleMCS == 0) &&
                                                  gmmResourceInfo->getResourceFlags()->Gpu.MCS;
    } else {
        l0imageDesc.width = (texInfo.textureBufferWidth != 0) ? texInfo.textureBufferWidth : 1u;
        l0imageDesc.height = 1u;
        l0imageDesc.depth = 1u;
        l0imageDesc.arraylevels = 1u;
    }

    cl_image_format imgFormat{};
    setClImageFormat(texInfo.glInternalFormat, imgFormat);
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
        return nullptr;
    }

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto image = new Image(context, memoryProperties, flags, imageHandle, nullptr, nullptr, false, imgFormat, nullptr);

    auto glTexture = new GlTexture(sharingFunctions, getClGlObjectType(target), texture, texInfo, target, std::max(miplevel, 0));
    image->setSharingHandler(glTexture);

    return image;
}

void GlTexture::synchronizeObject(UpdateData &updateData) {
    auto sharingFunctions = static_cast<GLSharingFunctionsWindows *>(this->sharingFunctions);
    CL_GL_RESOURCE_INFO resourceInfo = {0};
    resourceInfo.name = this->clGlObjectId;
    if (target == GL_RENDERBUFFER_EXT) {
        sharingFunctions->acquireSharedRenderBuffer(&resourceInfo);
    } else {
        sharingFunctions->acquireSharedTexture(&resourceInfo);
        // Set texture buffer offset acquired from OpenGL layer in graphics allocation
        updateData.memObject->getGraphicsAllocation(updateData.rootDeviceIndex)->setAllocationOffset(resourceInfo.textureBufferOffset);
    }
    updateData.sharedHandle = resourceInfo.globalShareHandle;
    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
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
        retValue = returnClGlObjectType ? CL_GL_OBJECT_TEXTURE_BUFFER : CL_MEM_OBJECT_IMAGE1D;
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
    auto sharingFunctions = static_cast<GLSharingFunctionsWindows *>(this->sharingFunctions);
    if (target == GL_RENDERBUFFER_EXT) {
        sharingFunctions->releaseSharedRenderBuffer(&textureInfo);
    } else {
        sharingFunctions->releaseSharedTexture(&textureInfo);
    }
}

void GlTexture::resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) {
    GlSharing::resolveGraphicsAllocationChange(currentSharedHandle, updateData);
}

} // namespace LEO
} // namespace NEO
