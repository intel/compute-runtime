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

    /* Prepare export request */
    struct mesa_glinterop_export_in texIn = {};
    struct mesa_glinterop_export_out texOut = {};

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

    if (texIn.target != GL_TEXTURE_2D) {
        printf("target %x not supported\n", target);
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

    texOut.version = 2;

    /* Call MESA interop */
    GLSharingFunctionsLinux *sharingFunctions = context->getSharing<GLSharingFunctionsLinux>();

    int retValue = sharingFunctions->exportObject(&texIn, &texOut);
    if ((retValue != MESA_GLINTEROP_SUCCESS) || (texOut.version != 2)) {
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
    }

    /* Map result for rest of the function */
    CL_GL_RESOURCE_INFO texInfo = {
        .name = texIn.obj,
        .globalShareHandle = static_cast<unsigned int>(texOut.dmabuf_fd),
        .glInternalFormat = static_cast<GLint>(texOut.internal_format),
        .textureBufferSize = static_cast<GLint>(texOut.buf_size),
        .textureBufferOffset = static_cast<GLint>(texOut.buf_offset),
    };

    imgDesc.image_width = texOut.width;
    imgDesc.image_height = texOut.height;
    imgDesc.image_depth = texOut.depth;
    imgDesc.image_row_pitch = texOut.stride;

    imgInfo.imgDesc.imageType = ImageType::image2D;
    imgInfo.imgDesc.imageWidth = imgDesc.image_width;
    imgInfo.imgDesc.imageHeight = imgDesc.image_height;
    imgInfo.imgDesc.imageDepth = imgDesc.image_depth;
    imgInfo.imgDesc.imageRowPitch = imgDesc.image_row_pitch;
    imgInfo.linearStorage = (texOut.modifier == 0);

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
        imgDesc.image_row_pitch = texInfo.textureBufferSize;
    } else if (imgDesc.image_row_pitch == 0) {
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

    uint32_t cubeFaceIndex = GmmTypesConverter::getCubeFaceIndex(target);

    uint32_t qPitch = gmm->queryQPitch(gmm->gmmResourceInfo->getResourceType());

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

    /* Prepare flush request */
    struct mesa_glinterop_export_in texIn = {};

    texIn.version = 2;
    texIn.target = this->target;
    texIn.obj = this->clGlObjectId;
    texIn.miplevel = this->miplevel;

    /* Call MESA interop */
    int retValue = sharingFunctions->flushObjects(1, &texIn, nullptr);
    if (retValue != MESA_GLINTEROP_SUCCESS) {
        updateData.synchronizationStatus = SynchronizeStatus::SYNCHRONIZE_ERROR;
        return;
    }

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
    auto memoryManager = memObject->getMemoryManager();
    memoryManager->closeSharedHandle(memObject->getGraphicsAllocation(rootDeviceIndex));
}
void GlTexture::resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) {
    const auto memObject = updateData->memObject;
    auto graphicsAllocation = memObject->getGraphicsAllocation(updateData->rootDeviceIndex);
    graphicsAllocation->setSharedHandle(updateData->sharedHandle);
}

} // namespace NEO
