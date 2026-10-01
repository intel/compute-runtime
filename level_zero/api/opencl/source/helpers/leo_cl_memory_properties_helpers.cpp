/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers_base.inl"
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj.h"

namespace NEO {
namespace LEO {

bool ClMemoryPropertiesHelper::parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &memoryProperties,
                                                     cl_mem_flags &flags, cl_mem_flags_intel &flagsIntel,
                                                     cl_mem_alloc_flags_intel &allocflags, ClMemoryPropertiesHelper::ObjType objectType, Context &context) {
    NEO::Device *pDevice = L0::Device::fromHandle(context.getL0Object()->getDevices().begin()->second)->getNEODevice();
    uint64_t handle = 0;
    uint64_t handleType = 0;
    uintptr_t hostptr = 0;
    std::vector<NEO::Device *> devices;

    if (properties != nullptr) {
        size_t i = 0;
        while (properties[i] != 0) {
            const auto propertyName = properties[i];

            if (MemObj::isHandleListProperty(propertyName)) {
                if ((propertyName == CL_IMAGE_L0_HANDLE_INTEL) && (objectType != ClMemoryPropertiesHelper::ObjType::image)) {
                    return false;
                }
                i = MemObj::getHandleListEnd(properties, i) + 1;
                continue;
            }

            switch (propertyName) {
            case CL_MEM_FLAGS:
                flags |= static_cast<cl_mem_flags>(properties[i + 1]);
                break;
            case CL_MEM_FLAGS_INTEL:
                flagsIntel |= static_cast<cl_mem_flags_intel>(properties[i + 1]);
                break;
            case CL_MEM_ALLOC_FLAGS_INTEL:
                allocflags |= static_cast<cl_mem_alloc_flags_intel>(properties[i + 1]);
                break;
            case CL_MEM_ALLOC_USE_HOST_PTR_INTEL:
                hostptr = static_cast<uintptr_t>(properties[i + 1]);
                break;
            case CL_MEM_L0_HANDLE_INTEL:
                if (objectType != ClMemoryPropertiesHelper::ObjType::buffer) {
                    return false;
                }
                break;
            case CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR:
            case CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR:
            case CL_MEM_DEVICE_ID_INTEL_DEPRECATED:
            case CL_MEM_DEVICE_ID_INTEL:
                break;
            default:
                return false;
            }
            i += 2;
        }
    }

    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, allocflags, pDevice);
    memoryProperties.handleType = handleType;
    memoryProperties.handle = handle;
    memoryProperties.hostptr = hostptr;
    memoryProperties.associatedDevices = std::move(devices);
    return true;
}

} // namespace LEO
} // namespace NEO
