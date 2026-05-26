/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/helpers/cl_validators.h"

#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/event/event.h"
#include "level_zero/api/opencl/source/kernel/kernel.h"
#include "level_zero/api/opencl/source/mem_obj/mem_obj.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/api/opencl/source/sampler/sampler.h"

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

} // namespace LEO
} // namespace NEO
