/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
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

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sharings/gl/gl_texture.h"
#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"

#include "CL/cl_gl.h"
#include "config.h"
#include <GL/gl.h>

namespace NEO {
Image *GlTexture::createSharedGlTexture(Context *context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture,
                                        cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_INVALID_GL_OBJECT);
    auto memoryManager = context->getMemoryManager();
    cl_image_desc imgDesc = {};
    ImageInfo imgInfo = {};
    cl_image_format imgFormat = {};
    McsSurfaceInfo mcsSurfaceInfo = {};

    CL_GL_RESOURCE_INFO texInfo = {};
    texInfo.name = texture;
    texInfo.target = getBaseTargetType(target);
    if (texInfo.target != GL_TEXTURE_2D) {
        printf("target %x not supported\n", target);
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

    uint32_t qPitch = 0;
    uint32_t cubeFaceIndex = __GMM_NO_CUBE_MAP;
    int imageWidth = 0, imageHeight = 0, internalFormat = 0;

    GLSharingFunctionsLinux *sharingFunctions = context->getSharing<GLSharingFunctionsLinux>();

    sharingFunctions->glGetTexLevelParameteriv(target, miplevel, GL_TEXTURE_WIDTH, &imageWidth);
    sharingFunctions->glGetTexLevelParameteriv(target, miplevel, GL_TEXTURE_HEIGHT, &imageHeight);
    sharingFunctions->glGetTexLevelParameteriv(target, miplevel, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);

    imgDesc.image_width = imageWidth;
    imgDesc.image_height = imageHeight;
    switch (internalFormat) {
    case GL_RGBA:
    case GL_RGBA8:
    case GL_RGBA16F:
    case GL_RGB:
        texInfo.glInternalFormat = internalFormat;
        break;
    default:
        printf("internal format %x not supported\n", internalFormat);
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

    imgInfo.imgDesc.imageWidth = imgDesc.image_width;
    imgInfo.imgDesc.imageType = ImageType::image2D;
    imgInfo.imgDesc.imageHeight = imgDesc.image_height;

    if (target == GL_RENDERBUFFER_EXT) {
#if 0
        sharingFunctions->acquireSharedRenderBuffer(&texInfo);
#endif
    } else {
        if (sharingFunctions->acquireSharedTexture(&texInfo) != EGL_TRUE) {
            errorCode.set(CL_INVALID_GL_OBJECT);
            return nullptr;
        }
    }

    errorCode.set(CL_SUCCESS);

    if (setClImageFormat(texInfo.glInternalFormat, imgFormat) == false) {
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

    auto surfaceFormatInfoAddress = Image::getSurfaceFormatFromTable(flags, &imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    if (!surfaceFormatInfoAddress) {
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }
    auto surfaceFormatInfo = *surfaceFormatInfoAddress;
    imgInfo.surfaceFormat = &surfaceFormatInfo.surfaceFormat;
    AllocationProperties allocProperties(context->getDevice(0)->getRootDeviceIndex(),
                                         false, // allocateMemory
                                         &imgInfo,
                                         AllocationType::sharedImage,
                                         context->getDeviceBitfieldForAllocation(context->getDevice(0)->getRootDeviceIndex()));
    auto alloc = memoryManager->createGraphicsAllocationFromSharedHandle(texInfo.globalShareHandle, allocProperties, false, false, false, nullptr);

    if (alloc == nullptr) {
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }
    memoryManager->closeSharedHandle(alloc);

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
        GMM_REQ_OFFSET_INFO gmmReqInfo = {};
        gmmReqInfo.ArrayIndex = imgDesc.image_array_size > 1 ? 1 : 0;
        gmmReqInfo.Slice = imgDesc.image_depth > 1 ? 1 : 0;
        gmmReqInfo.ReqLock = 1;
        gmm->gmmResourceInfo->getOffset(gmmReqInfo);
        imgDesc.image_slice_pitch = gmmReqInfo.Lock.Offset;
    } else {
        imgDesc.image_slice_pitch = alloc->getUnderlyingBufferSize();
    }

    cubeFaceIndex = GmmTypesConverter::getCubeFaceIndex(target);

    qPitch = gmm->queryQPitch(gmm->gmmResourceInfo->getResourceType());

    GraphicsAllocation *mcsAlloc = nullptr;

    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgInfo.imgDesc = Image::convertDescriptor(imgDesc);

    imgInfo.surfaceFormat = &surfaceFormatInfo.surfaceFormat;
    imgInfo.qPitch = qPitch;

    auto glTexture = new GlTexture(sharingFunctions, getClGlObjectType(target), texture, texInfo, target, std::max(miplevel, 0));

    if (texInfo.isAuxEnabled && alloc->getDefaultGmm()->unifiedAuxTranslationCapable()) {
        const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
        const auto &productHelper = context->getDevice(0)->getRootDeviceEnvironment().getHelper<ProductHelper>();
        alloc->getDefaultGmm()->isCompressionEnabled = productHelper.isPageTableManagerSupported(hwInfo) ? memoryManager->mapAuxGpuVA(alloc)
                                                                                                         : true;
    }
    auto multiGraphicsAllocation = MultiGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(alloc);

    return Image::createSharedImage(context, glTexture, mcsSurfaceInfo, std::move(multiGraphicsAllocation), mcsAlloc, flags, 0, &surfaceFormatInfo, imgInfo, cubeFaceIndex,
                                    std::max(miplevel, 0), imgInfo.imgDesc.numMipLevels);
}

void GlTexture::synchronizeObject(UpdateData &updateData) {
    auto sharingFunctions = static_cast<GLSharingFunctionsLinux *>(this->sharingFunctions);
    CL_GL_RESOURCE_INFO resourceInfo = {0};
    resourceInfo.name = this->clGlObjectId;
    if (target == GL_RENDERBUFFER_EXT) {
#if 0
        sharingFunctions->acquireSharedRenderBuffer(&resourceInfo);
#endif
    } else {
        if (sharingFunctions->acquireSharedTexture(&resourceInfo) == EGL_TRUE) {
            // Set texture buffer offset acquired from OpenGL layer in graphics allocation
            updateData.memObject->getGraphicsAllocation(updateData.rootDeviceIndex)->setAllocationOffset(resourceInfo.textureBufferOffset);
        } else {
            updateData.synchronizationStatus = SynchronizeStatus::SYNCHRONIZE_ERROR;
            return;
        }
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

void GlTexture::releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) {
    auto sharingFunctions = static_cast<GLSharingFunctionsLinux *>(this->sharingFunctions);
    if (target == GL_RENDERBUFFER_EXT) {
#if 0
        sharingFunctions->releaseSharedRenderBuffer(&textureInfo);
#endif
    } else {
        sharingFunctions->releaseSharedTexture(&textureInfo);
        auto memoryManager = memObject->getMemoryManager();
        memoryManager->closeSharedHandle(memObject->getGraphicsAllocation(rootDeviceIndex));
    }
}
void GlTexture::resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) {
    const auto memObject = updateData->memObject;
    auto graphicsAllocation = memObject->getGraphicsAllocation(updateData->rootDeviceIndex);
    graphicsAllocation->setSharedHandle(updateData->sharedHandle);
}

} // namespace NEO
