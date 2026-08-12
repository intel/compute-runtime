/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"

#include "shared/source/helpers/memory_properties_flags.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_buffer/leo_command_buffer.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/leo_surface_formats.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/source/sampler/leo_sampler.h"

#include <algorithm>

namespace NEO {
namespace LEO {

cl_int validateObject(cl_context object) noexcept {
    return castToObject<Context>(object) != nullptr ? CL_SUCCESS : CL_INVALID_CONTEXT;
}

cl_int validateObject(cl_device_id object) noexcept {
    return castToObject<ClDevice>(object) != nullptr ? CL_SUCCESS : CL_INVALID_DEVICE;
}

cl_int validateObject(cl_platform_id object) noexcept {
    return castToObject<Platform>(object) != nullptr ? CL_SUCCESS : CL_INVALID_PLATFORM;
}

cl_int validateObject(cl_command_queue object) noexcept {
    return castToObject<CommandQueue>(object) != nullptr ? CL_SUCCESS : CL_INVALID_COMMAND_QUEUE;
}

cl_int validateObject(cl_command_buffer_khr object) noexcept {
    return castToObject<CommandBuffer>(object) != nullptr ? CL_SUCCESS : CL_INVALID_COMMAND_BUFFER_KHR;
}

cl_int validateObject(cl_event object) noexcept {
    return castToObject<Event>(object) != nullptr ? CL_SUCCESS : CL_INVALID_EVENT;
}

cl_int validateObject(cl_mem object) noexcept {
    return castToObject<MemObj>(object) != nullptr ? CL_SUCCESS : CL_INVALID_MEM_OBJECT;
}

cl_int validateObject(cl_sampler object) noexcept {
    return castToObject<Sampler>(object) != nullptr ? CL_SUCCESS : CL_INVALID_SAMPLER;
}

cl_int validateObject(cl_program object) noexcept {
    return castToObject<Program>(object) != nullptr ? CL_SUCCESS : CL_INVALID_PROGRAM;
}

cl_int validateObject(cl_kernel object) noexcept {
    return castToObject<Kernel>(object) != nullptr ? CL_SUCCESS : CL_INVALID_KERNEL;
}

cl_int validateObject(EventWaitList eventWaitList) noexcept {
    if (eventWaitList.empty() && eventWaitList.data() != nullptr) {
        return CL_INVALID_EVENT_WAIT_LIST;
    }

    if (eventWaitList.data() == nullptr && !eventWaitList.empty()) {
        return CL_INVALID_EVENT_WAIT_LIST;
    }

    for (auto event : eventWaitList) {
        if (validateObject(event) != CL_SUCCESS) {
            return CL_INVALID_EVENT_WAIT_LIST;
        }
    }
    return CL_SUCCESS;
}

cl_int validateObject(DeviceList deviceList) noexcept {
    if (deviceList.empty() && deviceList.data() != nullptr) {
        return CL_INVALID_VALUE;
    }

    if (deviceList.data() == nullptr && !deviceList.empty()) {
        return CL_INVALID_VALUE;
    }

    for (auto device : deviceList) {
        if (validateObject(device) != CL_SUCCESS) {
            return CL_INVALID_DEVICE;
        }
    }
    return CL_SUCCESS;
}

cl_int validateObject(MemObjList memObjList) noexcept {
    if (memObjList.empty() && memObjList.data() != nullptr) {
        return CL_INVALID_VALUE;
    }

    if (memObjList.data() == nullptr && !memObjList.empty()) {
        return CL_INVALID_VALUE;
    }

    for (auto memObj : memObjList) {
        if (validateObject(memObj) != CL_SUCCESS) {
            return CL_INVALID_MEM_OBJECT;
        }
    }
    return CL_SUCCESS;
}

cl_int validateYuvOperation(const size_t *origin, const size_t *region) noexcept {
    if (!origin || !region) {
        return CL_INVALID_VALUE;
    }
    return ((origin[0] % 2 == 0) && (region[0] % 2 == 0)) ? CL_SUCCESS : CL_INVALID_VALUE;
}

bool isPackedYuvImage(const cl_image_format *imageFormat) noexcept {
    if (!imageFormat) {
        return false;
    }
    const auto channelOrder = imageFormat->image_channel_order;
    return (channelOrder == CL_YUYV_INTEL) ||
           (channelOrder == CL_UYVY_INTEL) ||
           (channelOrder == CL_YVYU_INTEL) ||
           (channelOrder == CL_VYUY_INTEL);
}

bool isNV12Image(const cl_image_format *imageFormat) noexcept {
    return imageFormat && (imageFormat->image_channel_order == CL_NV12_INTEL);
}

namespace {

constexpr cl_channel_type allChannelTypes[] = {
    CL_UNORM_INT8, CL_UNORM_INT16, CL_SNORM_INT8, CL_SNORM_INT16, CL_HALF_FLOAT, CL_FLOAT,
    CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32,
    CL_UNSIGNED_INT8, CL_UNSIGNED_INT16, CL_UNSIGNED_INT32};

constexpr cl_channel_type normalizedAndFloatTypes[] = {
    CL_UNORM_INT8, CL_UNORM_INT16, CL_SNORM_INT8, CL_SNORM_INT16, CL_HALF_FLOAT, CL_FLOAT};

constexpr cl_channel_type packedShortTypes[] = {
    CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010};

constexpr cl_channel_type byteTypes[] = {
    CL_UNORM_INT8, CL_SNORM_INT8, CL_SIGNED_INT8, CL_UNSIGNED_INT8};

constexpr cl_channel_type depthTypes[] = {CL_UNORM_INT16, CL_FLOAT};

constexpr cl_channel_type depthStencilTypes[] = {CL_UNORM_INT24, CL_FLOAT};

constexpr cl_channel_type unormInt8Only[] = {CL_UNORM_INT8};

struct ChannelOrderFormats {
    cl_channel_order channelOrder;
    std::span<const cl_channel_type> allowedDataTypes;
};

// clang-format off
constexpr ChannelOrderFormats validImageFormats[] = {
    {CL_A,             allChannelTypes},
    {CL_R,             allChannelTypes},
    {CL_Rx,            allChannelTypes},
    {CL_RG,            allChannelTypes},
    {CL_RGx,           allChannelTypes},
    {CL_RA,            allChannelTypes},
    {CL_RGBA,          allChannelTypes},
    {CL_INTENSITY,     normalizedAndFloatTypes},
    {CL_LUMINANCE,     normalizedAndFloatTypes},
    {CL_RGB,           packedShortTypes},
    {CL_RGBx,          packedShortTypes},
    {CL_ARGB,          byteTypes},
    {CL_BGRA,          byteTypes},
    {CL_ABGR,          byteTypes},
    {CL_sRGB,          unormInt8Only},
    {CL_sRGBx,         unormInt8Only},
    {CL_sRGBA,         unormInt8Only},
    {CL_sBGRA,         unormInt8Only},
    {CL_DEPTH,         depthTypes},
    {CL_DEPTH_STENCIL, depthStencilTypes},
    {CL_NV12_INTEL,    unormInt8Only},
    {CL_YUYV_INTEL,    unormInt8Only},
    {CL_UYVY_INTEL,    unormInt8Only},
    {CL_YVYU_INTEL,    unormInt8Only},
    {CL_VYUY_INTEL,    unormInt8Only}};
// clang-format on

cl_int validatePackedYUV(const MemoryProperties &memoryProperties, const cl_image_desc *imageDesc) noexcept {
    if (!memoryProperties.flags.readOnly) {
        return CL_INVALID_VALUE;
    }
    if ((imageDesc->image_width % 2 != 0) ||
        (imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D)) {
        return CL_INVALID_IMAGE_DESCRIPTOR;
    }
    return CL_SUCCESS;
}

cl_int validatePlanarYUV(const ClDevice &device, const MemoryProperties &memoryProperties, const cl_image_desc *imageDesc) noexcept {
    if (!memoryProperties.flags.hostNoAccess) {
        return CL_INVALID_VALUE;
    }
    if ((imageDesc->image_width % 4 != 0) ||
        (imageDesc->image_height % 4 != 0) ||
        (imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D)) {
        return CL_INVALID_IMAGE_DESCRIPTOR;
    }
    const auto &deviceInfo = device.getDeviceInfo();
    if ((imageDesc->image_width > deviceInfo.planarYuvMaxWidth) ||
        (imageDesc->image_height > deviceInfo.planarYuvMaxHeight)) {
        return CL_INVALID_IMAGE_SIZE;
    }
    return CL_SUCCESS;
}

cl_int validateImageDescriptor(const ClDevice &device,
                               const MemoryProperties &memoryProperties,
                               const ClSurfaceFormatInfo *surfaceFormat,
                               const cl_image_desc *imageDesc,
                               const void *hostPtr) noexcept {
    if (surfaceFormat == nullptr) {
        return CL_IMAGE_FORMAT_NOT_SUPPORTED;
    }

    const auto &deviceInfo = device.getSharedDeviceInfo();

    if (imageDesc->image_type == CL_MEM_OBJECT_IMAGE2D) {
        if ((imageDesc->image_width > deviceInfo.image2DMaxWidth) ||
            (imageDesc->image_height > deviceInfo.image2DMaxHeight)) {
            return CL_INVALID_IMAGE_SIZE;
        }
        if ((imageDesc->image_width == 0) || (imageDesc->image_height == 0)) {
            return CL_INVALID_IMAGE_DESCRIPTOR;
        }
    }

    if (imageDesc->image_row_pitch != 0) {
        if (hostPtr == nullptr) {
            return CL_INVALID_IMAGE_DESCRIPTOR;
        }
        const auto elementSize = surfaceFormat->surfaceFormat.imageElementSizeInBytes;
        if (((imageDesc->image_row_pitch % elementSize) != 0) ||
            (imageDesc->image_row_pitch < imageDesc->image_width * elementSize)) {
            return CL_INVALID_IMAGE_DESCRIPTOR;
        }
    }

    if (isNV12Image(&surfaceFormat->oclImageFormat)) {
        return validatePlanarYUV(device, memoryProperties, imageDesc);
    }

    if (isPackedYuvImage(&surfaceFormat->oclImageFormat)) {
        return validatePackedYUV(memoryProperties, imageDesc);
    }

    return CL_SUCCESS;
}

} // namespace

cl_int validateImageFormat(const cl_image_format *imageFormat) noexcept {
    if (!imageFormat) {
        return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
    }

    for (const auto &[channelOrder, allowedDataTypes] : validImageFormats) {
        if (channelOrder == imageFormat->image_channel_order) {
            const bool isAllowedDataType = std::ranges::find(allowedDataTypes, imageFormat->image_channel_data_type) != allowedDataTypes.end();
            return isAllowedDataType ? CL_SUCCESS : CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
        }
    }
    return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
}

cl_int validateStandaloneImageDescriptor(const ClDevice &device,
                                         const MemoryProperties &memoryProperties,
                                         cl_mem_flags flags,
                                         const cl_image_format *imageFormat,
                                         const cl_image_desc *imageDesc,
                                         const void *hostPtr) noexcept {
    return validateImageDescriptor(device,
                                   memoryProperties,
                                   Image::getSurfaceFormatFromTable(flags, imageFormat),
                                   imageDesc,
                                   hostPtr);
}

cl_int validateImageCopy(const cl_image_format &srcFormat,
                         const cl_image_format &dstFormat,
                         const size_t *srcOrigin,
                         const size_t *dstOrigin,
                         const size_t *region) noexcept {
    if ((srcFormat.image_channel_order != dstFormat.image_channel_order) ||
        (srcFormat.image_channel_data_type != dstFormat.image_channel_data_type)) {
        return CL_IMAGE_FORMAT_MISMATCH;
    }

    if (isPackedYuvImage(&srcFormat)) {
        const auto retVal = validateYuvOperation(srcOrigin, region);
        if (retVal != CL_SUCCESS) {
            return retVal;
        }
    }

    if (isPackedYuvImage(&dstFormat)) {
        const auto retVal = validateYuvOperation(dstOrigin, region);
        if (retVal != CL_SUCCESS) {
            return retVal;
        }
        // A packed YUV image is always 2D - creation rejects any other type - so a non-zero
        // slice origin can never address anything.
        if (dstOrigin[2] != 0) {
            return CL_INVALID_VALUE;
        }
    }

    return CL_SUCCESS;
}

} // namespace LEO
} // namespace NEO
