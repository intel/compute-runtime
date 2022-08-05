/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_memory_properties_helpers_base.inl"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/sharings/unified/unified_buffer.h"

namespace NEO {

bool ClMemoryPropertiesHelper::parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &memoryProperties,
                                                     cl_mem_flags &flags, cl_mem_flags_intel &flagsIntel,
                                                     cl_mem_alloc_flags_intel &allocflags, MemoryPropertiesHelper::ObjType objectType, Context &context) {
    bool deviceSet = false;
    Device *pDevice = context.getDevice(0)->getDevice().getRootDevice();
    uint64_t handle = 0;
    uint64_t handleType = 0;
    uintptr_t hostptr = 0;

    if (properties != nullptr) {
        for (int i = 0; properties[i] != 0; i += 2) {
            switch (properties[i]) {
            case CL_MEM_FLAGS:
                flags |= static_cast<cl_mem_flags>(properties[i + 1]);
                break;
            case CL_MEM_FLAGS_INTEL:
                flagsIntel |= static_cast<cl_mem_flags_intel>(properties[i + 1]);
                break;
            case CL_MEM_ALLOC_FLAGS_INTEL:
                allocflags |= static_cast<cl_mem_alloc_flags_intel>(properties[i + 1]);
                break;
            case CL_MEM_DEVICE_ID_INTEL: {
                if (deviceSet) {
                    return false;
                }
                cl_device_id deviceId = reinterpret_cast<cl_device_id>(properties[i + 1]);
                auto pClDevice = NEO::castToObject<ClDevice>(deviceId);
                if ((pClDevice == nullptr) || (!context.isDeviceAssociated(*pClDevice))) {
                    return false;
                }
                pDevice = &pClDevice->getDevice();
                deviceSet = true;
                break;
            }
            case CL_MEM_ALLOC_USE_HOST_PTR_INTEL:
                hostptr = static_cast<uintptr_t>(properties[i + 1]);
                break;
            case CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR:
                handle = static_cast<uint64_t>(properties[i + 1]);
                handleType = static_cast<uint64_t>(UnifiedSharingHandleType::LinuxFd);
                break;
            case CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR:
                handle = static_cast<uint64_t>(properties[i + 1]);
                handleType = static_cast<uint64_t>(UnifiedSharingHandleType::Win32Nt);
                break;
            default:
                return false;
            }
        }
    }

    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, allocflags, pDevice);
    memoryProperties.handleType = handleType;
    memoryProperties.handle = handle;
    memoryProperties.hostptr = hostptr;

    switch (objectType) {
    case MemoryPropertiesHelper::ObjType::BUFFER:
        return isFieldValid(flags, MemObjHelper::validFlagsForBuffer) &&
               isFieldValid(flagsIntel, MemObjHelper::validFlagsForBufferIntel);
    case MemoryPropertiesHelper::ObjType::IMAGE:
        return isFieldValid(flags, MemObjHelper::validFlagsForImage) &&
               isFieldValid(flagsIntel, MemObjHelper::validFlagsForImageIntel);
    default:
        return true;
    }
}

} // namespace NEO
