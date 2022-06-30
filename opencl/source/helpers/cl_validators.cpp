/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_validators.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/program.h"
#include "opencl/source/sampler/sampler.h"

namespace NEO {

cl_int validateObject(void *ptr) {
    return ptr != nullptr
               ? CL_SUCCESS
               : CL_INVALID_VALUE;
}

cl_int validateObject(cl_context object) {
    return castToObject<Context>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_CONTEXT;
}

cl_int validateObject(cl_device_id object) {
    return castToObject<ClDevice>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_DEVICE;
}

cl_int validateObject(cl_platform_id object) {
    return castToObject<Platform>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_PLATFORM;
}

cl_int validateObject(cl_command_queue object) {
    return castToObject<CommandQueue>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_COMMAND_QUEUE;
}

cl_int validateObject(cl_event object) {
    return castToObject<Event>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_EVENT;
}

cl_int validateObject(cl_mem object) {
    return castToObject<MemObj>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_MEM_OBJECT;
}

cl_int validateObject(cl_sampler object) {
    return castToObject<Sampler>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_SAMPLER;
}

cl_int validateObject(cl_program object) {
    return castToObject<Program>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_PROGRAM;
}

cl_int validateObject(cl_kernel object) {
    return castToObject<MultiDeviceKernel>(object) != nullptr
               ? CL_SUCCESS
               : CL_INVALID_KERNEL;
}

cl_int validateObject(const EventWaitList &eventWaitList) {
    if ((!eventWaitList.first) != (!eventWaitList.second))
        return CL_INVALID_EVENT_WAIT_LIST;

    for (cl_uint i = 0; i < eventWaitList.first; i++) {
        if (validateObject(eventWaitList.second[i]) != CL_SUCCESS)
            return CL_INVALID_EVENT_WAIT_LIST;
    }

    return CL_SUCCESS;
}

cl_int validateObject(const DeviceList &deviceList) {
    if ((!deviceList.first) != (!deviceList.second))
        return CL_INVALID_VALUE;

    for (cl_uint i = 0; i < deviceList.first; i++) {
        if (validateObject(deviceList.second[i]) != CL_SUCCESS)
            return CL_INVALID_DEVICE;
    }

    return CL_SUCCESS;
}

cl_int validateObject(const MemObjList &memObjList) {
    if ((!memObjList.first) != (!memObjList.second))
        return CL_INVALID_VALUE;

    for (cl_uint i = 0; i < memObjList.first; i++) {
        if (validateObject(memObjList.second[i]) != CL_SUCCESS)
            return CL_INVALID_MEM_OBJECT;
    }

    return CL_SUCCESS;
}

cl_int validateObject(const NonZeroBufferSize &nzbs) {
    return nzbs ? CL_SUCCESS : CL_INVALID_BUFFER_SIZE;
}

cl_int validateObject(const PatternSize &ps) {
    switch ((cl_int)ps) {
    case 128:
    case 64:
    case 32:
    case 16:
    case 8:
    case 4:
    case 2:
    case 1:
        return CL_SUCCESS;
    default:
        break;
    }

    return CL_INVALID_VALUE;
}

cl_int validateObject(bool isValid) {
    return isValid ? CL_SUCCESS : CL_INVALID_VALUE;
}

cl_int validateYuvOperation(const size_t *origin, const size_t *region) {
    if (!origin || !region)
        return CL_INVALID_VALUE;
    return ((origin[0] % 2 == 0) && (region[0] % 2 == 0)) ? CL_SUCCESS : CL_INVALID_VALUE;
}

bool isPackedYuvImage(const cl_image_format *imageFormat) {
    auto channelOrder = imageFormat->image_channel_order;
    return (channelOrder == CL_YUYV_INTEL) ||
           (channelOrder == CL_UYVY_INTEL) ||
           (channelOrder == CL_YVYU_INTEL) ||
           (channelOrder == CL_VYUY_INTEL);
}

bool isNV12Image(const cl_image_format *imageFormat) {
    return imageFormat->image_channel_order == CL_NV12_INTEL;
}
} // namespace NEO
