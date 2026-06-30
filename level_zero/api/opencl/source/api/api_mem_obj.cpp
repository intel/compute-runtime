/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/mem_obj/buffer.h"
#include "level_zero/api/opencl/source/mem_obj/image.h"
#include "level_zero/api/opencl/source/tracing/tracing_notify.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_imp.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

cl_mem CL_API_CALL clCreateBuffer(cl_context context,
                                  cl_mem_flags flags,
                                  size_t size,
                                  void *hostPtr,
                                  cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateBuffer, &context, &flags, &size, &hostPtr, &errcodeRet);
    cl_mem tracingRetVal = clCreateBufferWithProperties(context, nullptr, flags, size, hostPtr, errcodeRet);
    TRACING_EXIT(ClCreateBuffer, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreateBufferWithProperties(cl_context context,
                                                const cl_mem_properties *properties,
                                                cl_mem_flags flags,
                                                size_t size,
                                                void *hostPtr,
                                                cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateBufferWithProperties, &context, &properties, &flags, &size, &hostPtr, &errcodeRet);
    NEO::LEO::MemoryProperties memoryProperties{};
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    cl_mem_flags_intel emptyFlagsIntel = 0;
    if ((false == NEO::LEO::ClMemoryPropertiesHelper::parseMemoryProperties(nullptr, memoryProperties, flags, emptyFlagsIntel, allocflags,
                                                                            NEO::LEO::ClMemoryPropertiesHelper::ObjType::buffer, *NEO::LEO::castToObject<NEO::LEO::Context>(context)))) {
        if (errcodeRet) {
            *errcodeRet = CL_INVALID_VALUE;
        }
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateBufferWithProperties, &tracingRetVal);
        return tracingRetVal;
    }

    if ((false == NEO::LEO::ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                            NEO::LEO::ClMemoryPropertiesHelper::ObjType::buffer, *NEO::LEO::castToObject<NEO::LEO::Context>(context)))) {
        if (errcodeRet) {
            *errcodeRet = CL_INVALID_VALUE;
        }
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateBufferWithProperties, &tracingRetVal);
        return tracingRetVal;
    }

    void *ptr = nullptr;
    void *cpuPtr = nullptr;
    ze_result_t ret = ZE_RESULT_SUCCESS;

    bool copyFromHostPtr = memoryProperties.flags.copyHostPtr || memoryProperties.flags.useHostPtr;
    bool usesSvm = false;

    bool inputMemObjFound = false;
    auto inputMemObjHandle = NEO::LEO::MemObj::getMemObjProperties<uintptr_t>(properties, CL_L0_MEM_OBJ_HANDLE, &inputMemObjFound);
    if (inputMemObjFound) {
        ptr = reinterpret_cast<void *>(inputMemObjHandle);
    } else {
        ze_memory_compression_hints_ext_desc_t compressionHints{ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC, nullptr};
        if (memoryProperties.flags.compressedHint) {
            compressionHints.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;
        } else if (memoryProperties.flags.uncompressedHint) {
            compressionHints.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED;
        }

        auto allocData = NEO::LEO::castToObject<NEO::LEO::Context>(context)->getL0Object()->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(hostPtr);
        if (memoryProperties.flags.useHostPtr && allocData) {
            auto rootDeviceIndex = NEO::LEO::castToObject<NEO::LEO::Context>(context)->getClDevice()->getRootDeviceIndex();
            auto allocationEndAddress = allocData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex)->getGpuAddress() + allocData->size;
            auto bufferEndAddress = castToUint64(hostPtr) + size;

            if ((size > allocData->size) || (bufferEndAddress > allocationEndAddress)) {
                ret = ZE_RESULT_ERROR_INVALID_SIZE;
            } else {
                usesSvm = true;
                ptr = cpuPtr = hostPtr;
                copyFromHostPtr = false;
            }
        } else if (memoryProperties.flags.forceHostMemory || (NEO::LEO::castToObject<NEO::LEO::Context>(context)->getClDevice()->getHardwareInfo().capabilityTable.isIntegratedDevice && !memoryProperties.flags.useHostPtr)) {
            ze_host_mem_alloc_desc_t hostAllocDesc{ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, compressionHints.flags ? &compressionHints : nullptr, 0};
            ret = zeMemAllocHost(NEO::LEO::castToObject<NEO::LEO::Context>(context)->getL0ContextHandle(), &hostAllocDesc, size, 0, &ptr);
            cpuPtr = ptr;
        } else {
            auto pCtx = NEO::LEO::castToObject<NEO::LEO::Context>(context);
            ze_device_mem_alloc_desc_t deviceAllocDesc{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, compressionHints.flags ? &compressionHints : nullptr, 0, 0};
            if (pCtx->isSingleDeviceContext()) {
                ret = zeMemAllocDevice(pCtx->getL0ContextHandle(), &deviceAllocDesc, size, 0, pCtx->getL0Object()->getDevices().begin()->second, &ptr);
            } else {
                ze_host_mem_alloc_desc_t sharedHostDesc{ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr, 0};
                ret = zeMemAllocShared(pCtx->getL0ContextHandle(), &deviceAllocDesc, &sharedHostDesc, size, 0, nullptr, &ptr);
            }
        }
    }

    if (memoryProperties.flags.useHostPtr) {
        cpuPtr = hostPtr;
    }

    if (errcodeRet) {
        *errcodeRet = L0ToClResultMapper(ret);
    }

    if (ret != ZE_RESULT_SUCCESS) {
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateBufferWithProperties, &tracingRetVal);
        return tracingRetVal;
    }

    if (copyFromHostPtr) {
        auto pContext = NEO::LEO::castToObject<NEO::LEO::Context>(context);
        {
            auto lock = pContext->lockInternalCopy();
            zeCommandListAppendMemoryCopy(pContext->getInternalCopyCmdList(),
                                          ptr,
                                          hostPtr,
                                          size,
                                          nullptr,
                                          0,
                                          nullptr);
        }
        zeCommandListHostSynchronize(pContext->getInternalCopyCmdList(), std::numeric_limits<uint64_t>::max());
    }
    auto buffer = new NEO::LEO::Buffer(NEO::LEO::castToObject<NEO::LEO::Context>(context), memoryProperties, flags, ptr, cpuPtr, size, inputMemObjFound);
    buffer->storeProperties(properties);
    buffer->setUsesSvm(usesSvm);
    cl_mem tracingRetVal = buffer;
    TRACING_EXIT(ClCreateBufferWithProperties, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreateBufferWithPropertiesINTEL(cl_context context,
                                                     const cl_mem_properties_intel *properties,
                                                     cl_mem_flags flags,
                                                     size_t size,
                                                     void *hostPtr,
                                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateBufferWithPropertiesINTEL, &context, &properties, &flags, &size, &hostPtr, &errcodeRet);
    cl_mem tracingRetVal = clCreateBufferWithProperties(context, properties, flags, size, hostPtr, errcodeRet);
    TRACING_EXIT(ClCreateBufferWithPropertiesINTEL, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreateSubBuffer(cl_mem buffer,
                                     cl_mem_flags flags,
                                     cl_buffer_create_type bufferCreateType,
                                     const void *bufferCreateInfo,
                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateSubBuffer, &buffer, &flags, &bufferCreateType, &bufferCreateInfo, &errcodeRet);
    auto parentBuffer = NEO::LEO::castToObject<NEO::LEO::Buffer>(buffer);
    const cl_buffer_region *region = reinterpret_cast<const cl_buffer_region *>(bufferCreateInfo);
    NEO::LEO::Buffer *subBuffer = nullptr;
    cl_int ret = CL_INVALID_MEM_OBJECT;
    if (parentBuffer && CL_SUCCESS == (ret = parentBuffer->validateSubBufferRegion(region))) {
        cl_mem_flags_intel emptyFlagsIntel = 0;
        subBuffer = parentBuffer->createSubBuffer(flags, emptyFlagsIntel, region);
    }
    if (errcodeRet) {
        *errcodeRet = ret;
    }
    cl_mem tracingRetVal = subBuffer;
    TRACING_EXIT(ClCreateSubBuffer, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreateImage(cl_context context,
                                 cl_mem_flags flags,
                                 const cl_image_format *imageFormat,
                                 const cl_image_desc *imageDesc,
                                 void *hostPtr,
                                 cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateImage, &context, &flags, &imageFormat, &imageDesc, &hostPtr, &errcodeRet);
    cl_mem tracingRetVal = clCreateImageWithProperties(context, nullptr, flags, imageFormat, imageDesc, hostPtr, errcodeRet);
    TRACING_EXIT(ClCreateImage, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreateImageWithProperties(cl_context context,
                                               const cl_mem_properties *properties,
                                               cl_mem_flags flags,
                                               const cl_image_format *imageFormat,
                                               const cl_image_desc *imageDesc,
                                               void *hostPtr,
                                               cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateImageWithProperties, &context, &properties, &flags, &imageFormat, &imageDesc, &hostPtr, &errcodeRet);
    NEO::LEO::MemoryProperties memoryProperties{};
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_flags_intel emptyFlagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    if ((false == NEO::LEO::ClMemoryPropertiesHelper::parseMemoryProperties(nullptr, memoryProperties, flags, emptyFlagsIntel, allocflags,
                                                                            NEO::LEO::ClMemoryPropertiesHelper::ObjType::image, *NEO::LEO::castToObject<NEO::LEO::Context>(context)))) {
        if (errcodeRet) {
            *errcodeRet = CL_INVALID_VALUE;
        }
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateImageWithProperties, &tracingRetVal);
        return tracingRetVal;
    }

    if ((false == NEO::LEO::ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                            NEO::LEO::ClMemoryPropertiesHelper::ObjType::image, *NEO::LEO::castToObject<NEO::LEO::Context>(context)))) {
        if (errcodeRet) {
            *errcodeRet = CL_INVALID_VALUE;
        }
        cl_mem tracingRetVal = nullptr;
        TRACING_EXIT(ClCreateImageWithProperties, &tracingRetVal);
        return tracingRetVal;
    }

    ze_image_handle_t imageHandle{};
    std::map<uint32_t, ze_image_handle_t> extraImageHandles{};
    ze_result_t ret = ZE_RESULT_SUCCESS;
    ze_image_desc_t l0imageDesc{ZE_STRUCTURE_TYPE_IMAGE_DESC};
    cl_image_format resolvedFormat{};

    bool inputMemObjFound = false;
    auto inputMemObjHandle = NEO::LEO::MemObj::getMemObjProperties<uintptr_t>(properties, CL_L0_MEM_OBJ_HANDLE, &inputMemObjFound);
    if (inputMemObjFound) {
        imageHandle = reinterpret_cast<ze_image_handle_t>(inputMemObjHandle);
        auto l0Image = static_cast<L0::ImageImp *>(L0::Image::fromHandle(imageHandle));
        l0imageDesc = l0Image->getImageDesc();
        resolvedFormat.image_channel_data_type = L0::getClChannelDataType(l0imageDesc.format);
        resolvedFormat.image_channel_order = L0::getClChannelOrder(l0imageDesc.format, l0Image->isSrgb());
    } else {
        l0imageDesc.miplevels = imageDesc->num_mip_levels;
        l0imageDesc.width = static_cast<uint32_t>(imageDesc->image_width);
        l0imageDesc.height = static_cast<uint32_t>(NEO::LEO::SurfaceFormats::getImageHeight(*imageDesc));
        l0imageDesc.depth = static_cast<uint32_t>(NEO::LEO::SurfaceFormats::getImageDepth(*imageDesc));
        l0imageDesc.arraylevels = static_cast<uint32_t>(NEO::LEO::SurfaceFormats::getImageArraySize(*imageDesc));

        l0imageDesc.type = NEO::LEO::Image::clToL0ImageType(imageDesc->image_type);
        NEO::LEO::Image::clToL0ImageFormat(l0imageDesc.format, imageFormat->image_channel_order, imageFormat->image_channel_data_type);

        ze_srgb_ext_desc_t srgbExtDesc{ZE_STRUCTURE_TYPE_SRGB_EXT_DESC, l0imageDesc.pNext, NEO::LEO::Image::isSRGB(imageFormat->image_channel_order)};
        l0imageDesc.pNext = &srgbExtDesc;

        ze_image_pitched_exp_desc_t imageFromBuffer{ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC};

        auto device = NEO::LEO::castToObject<NEO::LEO::Context>(context)->getL0Object()->getDevices().begin()->second;
        if (imageDesc->mem_object) {
            imageFromBuffer.ptr = NEO::LEO::castToObject<NEO::LEO::Buffer>(imageDesc->mem_object)->getUsmPtr();
            imageFromBuffer.pNext = l0imageDesc.pNext;
            l0imageDesc.pNext = &imageFromBuffer;
        }

        ze_custom_pitch_exp_desc_t customPitchDesc{ZE_STRUCTURE_TYPE_CUSTOM_PITCH_EXP_DESC};
        if (imageDesc->mem_object && (imageDesc->image_row_pitch != 0 || imageDesc->image_slice_pitch != 0) &&
            imageDesc->image_type != CL_MEM_OBJECT_IMAGE1D_BUFFER) {
            customPitchDesc.rowPitch = imageDesc->image_row_pitch;
            customPitchDesc.slicePitch = imageDesc->image_slice_pitch;
            customPitchDesc.pNext = l0imageDesc.pNext;
            l0imageDesc.pNext = &customPitchDesc;
        }

        L0::ze_depth_stencil_format_ext_desc_t depthStencilDesc{};
        if (imageFormat->image_channel_order == CL_DEPTH_STENCIL) {
            if (imageFormat->image_channel_data_type == CL_UNORM_INT24) {
                depthStencilDesc.format = L0::ZE_DEPTH_STENCIL_FORMAT_D24_UNORM_S8_UINT;
            } else {
                depthStencilDesc.format = L0::ZE_DEPTH_STENCIL_FORMAT_D32_FLOAT_S8X24_UINT;
            }
            depthStencilDesc.pNext = l0imageDesc.pNext;
            l0imageDesc.pNext = &depthStencilDesc;
        }

        ret = zeImageCreate(NEO::LEO::castToObject<NEO::LEO::Context>(context)->getL0ContextHandle(),
                            device,
                            &l0imageDesc,
                            &imageHandle);

        auto pCtx = NEO::LEO::castToObject<NEO::LEO::Context>(context);
        if (ret == ZE_RESULT_SUCCESS && !pCtx->isSingleDeviceContext()) {
            const auto primaryRootDeviceIndex = pCtx->getL0Object()->getDevices().begin()->first;
            for (auto clDevice : pCtx->getClDevices()) {
                const auto extraRootDeviceIndex = clDevice->getRootDeviceIndex();
                if (extraRootDeviceIndex == primaryRootDeviceIndex || extraImageHandles.count(extraRootDeviceIndex) != 0) {
                    continue;
                }
                ze_image_handle_t extraImageHandle{};
                if (zeImageCreate(pCtx->getL0ContextHandle(), clDevice->getL0Handle(), &l0imageDesc, &extraImageHandle) == ZE_RESULT_SUCCESS) {
                    extraImageHandles[extraRootDeviceIndex] = extraImageHandle;
                }
            }
        }

        resolvedFormat = *imageFormat;
    }

    void *cpuPtr = nullptr;
    if (memoryProperties.flags.useHostPtr) {
        cpuPtr = hostPtr;
    }

    if (memoryProperties.flags.copyHostPtr || memoryProperties.flags.useHostPtr) {
        auto pContext = NEO::LEO::castToObject<NEO::LEO::Context>(context);
        {
            auto lock = pContext->lockInternalCopy();
            uint32_t regionHeight = std::max(l0imageDesc.height, 1u);
            uint32_t regionDepth = std::max(l0imageDesc.depth, 1u);
            if (l0imageDesc.type == ZE_IMAGE_TYPE_1DARRAY) {
                regionHeight = std::max(l0imageDesc.arraylevels, 1u);
            } else if (l0imageDesc.type == ZE_IMAGE_TYPE_2DARRAY) {
                regionDepth = std::max(l0imageDesc.arraylevels, 1u);
            }
            ze_image_region_t region{0u, 0u, 0u, static_cast<uint32_t>(l0imageDesc.width),
                                     regionHeight, regionDepth};
            ret = zeCommandListAppendImageCopyFromMemoryExt(NEO::LEO::castToObject<NEO::LEO::Context>(context)->getInternalCopyCmdList(),
                                                            imageHandle,
                                                            hostPtr,
                                                            &region,
                                                            static_cast<uint32_t>(imageDesc->image_row_pitch),
                                                            static_cast<uint32_t>(imageDesc->image_slice_pitch),
                                                            nullptr, 0, nullptr);
        }
        zeCommandListHostSynchronize(pContext->getInternalCopyCmdList(), std::numeric_limits<uint64_t>::max());
    }

    if (errcodeRet) {
        *errcodeRet = L0ToClResultMapper(ret);
    }

    cl_mem associatedMemObject = inputMemObjFound ? nullptr : imageDesc->mem_object;
    auto image = new NEO::LEO::Image(NEO::LEO::castToObject<NEO::LEO::Context>(context), memoryProperties, flags, imageHandle, cpuPtr, nullptr, inputMemObjFound, resolvedFormat, associatedMemObject);
    image->setOwnerRootDeviceIndex(NEO::LEO::castToObject<NEO::LEO::Context>(context)->getL0Object()->getDevices().begin()->first);
    for (auto &[extraRootDeviceIndex, extraImageHandle] : extraImageHandles) {
        image->addPerDeviceHandle(extraRootDeviceIndex, extraImageHandle);
    }
    image->storeProperties(properties);
    image->setHostPtrRowPitch(imageDesc->image_row_pitch);
    image->setHostPtrSlicePitch(imageDesc->image_slice_pitch);

    cl_mem tracingRetVal = image;
    TRACING_EXIT(ClCreateImageWithProperties, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreateImageWithPropertiesINTEL(cl_context context,
                                                    const cl_mem_properties_intel *properties,
                                                    cl_mem_flags flags,
                                                    const cl_image_format *imageFormat,
                                                    const cl_image_desc *imageDesc,
                                                    void *hostPtr,
                                                    cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateImageWithPropertiesINTEL, &context, &properties, &flags, &imageFormat, &imageDesc, &hostPtr, &errcodeRet);
    cl_mem tracingRetVal = clCreateImageWithProperties(context, properties, flags, imageFormat, imageDesc, hostPtr, errcodeRet);
    TRACING_EXIT(ClCreateImageWithPropertiesINTEL, &tracingRetVal);
    return tracingRetVal;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateImage2D(cl_context context,
                                   cl_mem_flags flags,
                                   const cl_image_format *imageFormat,
                                   size_t imageWidth,
                                   size_t imageHeight,
                                   size_t imageRowPitch,
                                   void *hostPtr,
                                   cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateImage2D, &context, &flags, &imageFormat, &imageWidth, &imageHeight, &imageRowPitch, &hostPtr, &errcodeRet);
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = imageWidth;
    imageDesc.image_height = imageHeight;
    imageDesc.image_row_pitch = imageRowPitch;
    cl_mem tracingRetVal = clCreateImageWithProperties(context, nullptr, flags, imageFormat, &imageDesc, hostPtr, errcodeRet);
    TRACING_EXIT(ClCreateImage2D, &tracingRetVal);
    return tracingRetVal;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateImage3D(cl_context context,
                                   cl_mem_flags flags,
                                   const cl_image_format *imageFormat,
                                   size_t imageWidth,
                                   size_t imageHeight,
                                   size_t imageDepth,
                                   size_t imageRowPitch,
                                   size_t imageSlicePitch,
                                   void *hostPtr,
                                   cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateImage3D, &context, &flags, &imageFormat, &imageWidth, &imageHeight, &imageDepth, &imageRowPitch, &imageSlicePitch, &hostPtr, &errcodeRet);
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = imageWidth;
    imageDesc.image_height = imageHeight;
    imageDesc.image_depth = imageDepth;
    imageDesc.image_row_pitch = imageRowPitch;
    imageDesc.image_slice_pitch = imageSlicePitch;
    cl_mem tracingRetVal = clCreateImageWithProperties(context, nullptr, flags, imageFormat, &imageDesc, hostPtr, errcodeRet);
    TRACING_EXIT(ClCreateImage3D, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clRetainMemObject(cl_mem memobj) {
    TRACING_ENTER(ClRetainMemObject, &memobj);
    auto [retVal, pMemObj] = NEO::LEO::validateAndCast(std::make_tuple(memobj));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClRetainMemObject, &retVal);
        return retVal;
    }

    pMemObj->incRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClRetainMemObject, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clReleaseMemObject(cl_mem memobj) {
    TRACING_ENTER(ClReleaseMemObject, &memobj);
    auto [retVal, pMemObj] = NEO::LEO::validateAndCast(std::make_tuple(memobj));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClReleaseMemObject, &retVal);
        return retVal;
    }

    pMemObj->decRefApi();
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClReleaseMemObject, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetSupportedImageFormats(cl_context context,
                                              cl_mem_flags flags,
                                              cl_mem_object_type imageType,
                                              cl_uint numEntries,
                                              cl_image_format *imageFormats,
                                              cl_uint *numImageFormats) {
    TRACING_ENTER(ClGetSupportedImageFormats, &context, &flags, &imageType, &numEntries, &imageFormats, &numImageFormats);
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetSupportedImageFormats, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = CL_SUCCESS;
    if (pContext->getClDevice()->getHardwareInfo().capabilityTable.supportsImages) {
        tracingRetVal = pContext->getSupportedImageFormats(flags,
                                                           imageType,
                                                           numEntries,
                                                           imageFormats,
                                                           numImageFormats);
    } else if (numImageFormats) {
        *numImageFormats = 0u;
    }
    TRACING_EXIT(ClGetSupportedImageFormats, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetMemObjectInfo(cl_mem memobj,
                                      cl_mem_info paramName,
                                      size_t paramValueSize,
                                      void *paramValue,
                                      size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetMemObjectInfo, &memobj, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pMemObj] = NEO::LEO::validateAndCast(std::make_tuple(memobj));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetMemObjectInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pMemObj->getMemObjectInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetMemObjectInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetImageInfo(cl_mem image,
                                  cl_image_info paramName,
                                  size_t paramValueSize,
                                  void *paramValue,
                                  size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetImageInfo, &image, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto [retVal, pImage] = NEO::LEO::validateAndCast(std::make_tuple(NEO::LEO::ImageObj{image}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClGetImageInfo, &retVal);
        return retVal;
    }

    cl_int tracingRetVal = pImage->getImageInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetImageInfo, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetImageParamsINTEL(cl_context context,
                                         const cl_image_format *imageFormat,
                                         const cl_image_desc *imageDesc,
                                         size_t *imageRowPitch,
                                         size_t *imageSlicePitch) {
    TRACING_ENTER(ClGetImageParamsINTEL, &context, &imageFormat, &imageDesc, &imageRowPitch, &imageSlicePitch);
    cl_int tracingRetVal = CL_INVALID_OPERATION;
    TRACING_EXIT(ClGetImageParamsINTEL, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clSetMemObjectDestructorCallback(cl_mem memobj,
                                                    void(CL_CALLBACK *funcNotify)(cl_mem, void *),
                                                    void *userData) {
    TRACING_ENTER(ClSetMemObjectDestructorCallback, &memobj, &funcNotify, &userData);
    auto [retVal, pMemObj] = NEO::LEO::validateAndCast(std::make_tuple(memobj), std::make_tuple(reinterpret_cast<void *>(funcNotify)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClSetMemObjectDestructorCallback, &retVal);
        return retVal;
    }

    pMemObj->addCallback(funcNotify, userData);
    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClSetMemObjectDestructorCallback, &tracingRetVal);
    return tracingRetVal;
}

cl_mem CL_API_CALL clCreatePipe(cl_context context,
                                cl_mem_flags flags,
                                cl_uint pipePacketSize,
                                cl_uint pipeMaxPackets,
                                const cl_pipe_properties *properties,
                                cl_int *errcodeRet) {
    TRACING_ENTER(ClCreatePipe, &context, &flags, &pipePacketSize, &pipeMaxPackets, &properties, &errcodeRet);
    ErrorCodeHelper err(errcodeRet, CL_INVALID_OPERATION);
    cl_mem tracingRetVal = nullptr;
    TRACING_EXIT(ClCreatePipe, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clGetPipeInfo(cl_mem pipe,
                                 cl_pipe_info paramName,
                                 size_t paramValueSize,
                                 void *paramValue,
                                 size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetPipeInfo, &pipe, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int tracingRetVal = CL_INVALID_MEM_OBJECT;
    TRACING_EXIT(ClGetPipeInfo, &tracingRetVal);
    return tracingRetVal;
}
