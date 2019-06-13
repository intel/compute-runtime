/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/gl/gl_texture.h"

#include "public/cl_gl_private_intel.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/memory_manager.h"

#include "CL/cl_gl.h"
#include "config.h"
#include <GL/gl.h>

namespace NEO {
Image *GlTexture::createSharedGlTexture(Context *context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture,
                                        cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_INVALID_GL_OBJECT);
    auto memoryManager = context->getMemoryManager();
    cl_image_desc imgDesc = {};
    cl_image_format imgFormat = {};
    McsSurfaceInfo mcsSurfaceInfo = {};

    CL_GL_RESOURCE_INFO texInfo = {};
    texInfo.name = texture;
    texInfo.target = getBaseTargetType(target);

    GLSharingFunctions *sharingFunctions = context->getSharing<GLSharingFunctions>();

    if (target == GL_RENDERBUFFER_EXT) {
        sharingFunctions->acquireSharedRenderBuffer(&texInfo);
    } else {
        sharingFunctions->acquireSharedTexture(&texInfo);
    }

    errorCode.set(CL_SUCCESS);

    AllocationProperties allocProperties(false, 0u, GraphicsAllocation::AllocationType::SHARED_IMAGE, false);
    auto alloc = memoryManager->createGraphicsAllocationFromSharedHandle(texInfo.globalShareHandle, allocProperties, false);

    if (alloc == nullptr) {
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }
    if (texInfo.pGmmResInfo) {
        DEBUG_BREAK_IF(alloc->getDefaultGmm() != nullptr);
        alloc->setDefaultGmm(new Gmm(texInfo.pGmmResInfo));
    }

    auto gmm = alloc->getDefaultGmm();

    imgDesc.image_type = getClMemObjectType(target);
    if (target == GL_TEXTURE_BUFFER) {
        imgDesc.image_width = texInfo.textureBufferWidth;
        imgDesc.image_row_pitch = texInfo.textureBufferSize;
    } else {
        imgDesc.image_width = gmm->gmmResourceInfo->getBaseWidth();
        imgDesc.image_row_pitch = gmm->gmmResourceInfo->getRenderPitch();
        if (imgDesc.image_row_pitch == 0) {
            size_t alignedWidth = alignUp(imgDesc.image_width, gmm->gmmResourceInfo->getHAlign());
            size_t bpp = gmm->gmmResourceInfo->getBitsPerPixel() >> 3;
            imgDesc.image_row_pitch = alignedWidth * bpp;
        }
    }

    uint32_t numSamples = static_cast<uint32_t>(gmm->gmmResourceInfo->getNumSamples());
    imgDesc.num_samples = getValidParam(numSamples, 0u, 1u);
    imgDesc.image_height = gmm->gmmResourceInfo->getBaseHeight();
    imgDesc.image_array_size = gmm->gmmResourceInfo->getArraySize();
    if (target == GL_TEXTURE_3D) {
        imgDesc.image_depth = gmm->gmmResourceInfo->getBaseDepth();
    }

    if (imgDesc.image_array_size > 1 || imgDesc.image_depth > 1) {
        GMM_REQ_OFFSET_INFO GMMReqInfo = {};
        GMMReqInfo.ArrayIndex = imgDesc.image_array_size > 1 ? 1 : 0;
        GMMReqInfo.Slice = imgDesc.image_depth > 1 ? 1 : 0;
        GMMReqInfo.ReqLock = 1;
        gmm->gmmResourceInfo->getOffset(GMMReqInfo);
        imgDesc.image_slice_pitch = GMMReqInfo.Lock.Offset;
    } else {
        imgDesc.image_slice_pitch = alloc->getUnderlyingBufferSize();
    }

    uint32_t cubeFaceIndex = GmmHelper::getCubeFaceIndex(target);

    auto qPitch = gmm->queryQPitch(gmm->gmmResourceInfo->getResourceType());

    if (setClImageFormat(texInfo.glInternalFormat, imgFormat) == false) {
        memoryManager->freeGraphicsMemory(alloc);
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }
    auto surfaceFormatInfoAddress = Image::getSurfaceFormatFromTable(flags, &imgFormat);
    if (!surfaceFormatInfoAddress) {
        memoryManager->freeGraphicsMemory(alloc);
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }
    auto surfaceFormatInfo = *surfaceFormatInfoAddress;
    if (texInfo.glInternalFormat != GL_RGB10) {
        surfaceFormatInfo.GenxSurfaceFormat = (GFX3DSTATE_SURFACEFORMAT)texInfo.glHWFormat;
    }

    GraphicsAllocation *mcsAlloc = nullptr;
    if (texInfo.globalShareHandleMCS) {
        AllocationProperties allocProperties(0, GraphicsAllocation::AllocationType::MCS);
        mcsAlloc = memoryManager->createGraphicsAllocationFromSharedHandle(texInfo.globalShareHandleMCS, allocProperties, false);
        if (texInfo.pGmmResInfoMCS) {
            DEBUG_BREAK_IF(mcsAlloc->getDefaultGmm() != nullptr);
            mcsAlloc->setDefaultGmm(new Gmm(texInfo.pGmmResInfoMCS));
        }
        mcsSurfaceInfo.pitch = getValidParam(static_cast<uint32_t>(mcsAlloc->getDefaultGmm()->gmmResourceInfo->getRenderPitch() / 128));
        mcsSurfaceInfo.qPitch = mcsAlloc->getDefaultGmm()->gmmResourceInfo->getQPitch();
    }
    mcsSurfaceInfo.multisampleCount = GmmHelper::getRenderMultisamplesCount(static_cast<uint32_t>(imgDesc.num_samples));

    ImageInfo imgInfo = {0};
    imgInfo.imgDesc = &imgDesc;
    imgInfo.surfaceFormat = &surfaceFormatInfo;
    imgInfo.qPitch = qPitch;

    if (miplevel < 0) {
        imgDesc.num_mip_levels = gmm->gmmResourceInfo->getMaxLod() + 1;
    }

    auto glTexture = new GlTexture(sharingFunctions, getClGlObjectType(target), texture, texInfo, target, std::max(miplevel, 0));

    if (alloc->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        alloc->getDefaultGmm()->isRenderCompressed = context->getMemoryManager()->mapAuxGpuVA(alloc);
    }
    return Image::createSharedImage(context, glTexture, mcsSurfaceInfo, alloc, mcsAlloc, flags, imgInfo, cubeFaceIndex,
                                    std::max(miplevel, 0), imgDesc.num_mip_levels);
} // namespace NEO

void GlTexture::synchronizeObject(UpdateData &updateData) {
    CL_GL_RESOURCE_INFO resourceInfo = {0};
    resourceInfo.name = this->clGlObjectId;
    if (target == GL_RENDERBUFFER_EXT) {
        sharingFunctions->acquireSharedRenderBuffer(&resourceInfo);
    } else {
        sharingFunctions->acquireSharedTexture(&resourceInfo);
        // Set texture buffer offset acquired from OpenGL layer in graphics allocation
        updateData.memObject->getGraphicsAllocation()->setAllocationOffset(resourceInfo.textureBufferOffset);
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
    // return cl_gl_object_type if returnClGlObjectType is ture, otherwise cl_mem_object_type
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

void GlTexture::releaseResource(MemObj *memObject) {
    if (target == GL_RENDERBUFFER_EXT) {
        sharingFunctions->releaseSharedRenderBuffer(&textureInfo);
    } else {
        sharingFunctions->releaseSharedTexture(&textureInfo);
    }
}

} // namespace NEO
