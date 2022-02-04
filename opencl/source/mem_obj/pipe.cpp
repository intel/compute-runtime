/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/pipe.h"

#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"

namespace NEO {

Pipe::Pipe(Context *context,
           cl_mem_flags flags,
           cl_uint packetSize,
           cl_uint maxPackets,
           const cl_pipe_properties *properties,
           void *memoryStorage,
           MultiGraphicsAllocation multiGraphicsAllocation)
    : MemObj(context,
             CL_MEM_OBJECT_PIPE,
             ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
             flags,
             0,
             static_cast<size_t>(packetSize * (maxPackets + 1) + intelPipeHeaderReservedSpace),
             memoryStorage,
             nullptr,
             std::move(multiGraphicsAllocation),
             false,
             false,
             false),
      pipePacketSize(packetSize),
      pipeMaxPackets(maxPackets) {
    magic = objectMagic;
}

Pipe *Pipe::create(Context *context,
                   cl_mem_flags flags,
                   cl_uint packetSize,
                   cl_uint maxPackets,
                   const cl_pipe_properties *properties,
                   cl_int &errcodeRet) {
    Pipe *pPipe = nullptr;
    errcodeRet = CL_SUCCESS;

    MemoryManager *memoryManager = context->getMemoryManager();
    DEBUG_BREAK_IF(!memoryManager);

    MemoryProperties memoryProperties =
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice());
    while (true) {
        auto size = static_cast<size_t>(packetSize * (maxPackets + 1) + intelPipeHeaderReservedSpace);
        auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();
        AllocationProperties allocProperties =
            MemoryPropertiesHelper::getAllocationProperties(rootDeviceIndex, memoryProperties,
                                                            true, // allocateMemory
                                                            size, AllocationType::PIPE,
                                                            false, // isMultiStorageAllocation
                                                            context->getDevice(0)->getHardwareInfo(), context->getDeviceBitfieldForAllocation(rootDeviceIndex),
                                                            context->isSingleDeviceContext());
        GraphicsAllocation *memory = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);
        if (!memory) {
            errcodeRet = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        auto multiGraphicsAllocation = MultiGraphicsAllocation(rootDeviceIndex);
        multiGraphicsAllocation.addAllocation(memory);

        pPipe = new (std::nothrow) Pipe(context, flags, packetSize, maxPackets, properties, memory->getUnderlyingBuffer(), std::move(multiGraphicsAllocation));
        if (!pPipe) {
            memoryManager->freeGraphicsMemory(memory);
            memory = nullptr;
            errcodeRet = CL_OUT_OF_HOST_MEMORY;
            break;
        }
        // Initialize pipe_control_intel_t structure located at the beginning of the surface
        memset(memory->getUnderlyingBuffer(), 0, intelPipeHeaderReservedSpace);
        *reinterpret_cast<unsigned int *>(memory->getUnderlyingBuffer()) = maxPackets + 1;
        break;
    }

    return pPipe;
}

cl_int Pipe::getPipeInfo(cl_image_info paramName,
                         size_t paramValueSize,
                         void *paramValue,
                         size_t *paramValueSizeRet) {

    cl_int retVal;
    size_t srcParamSize = GetInfo::invalidSourceSize;
    void *srcParam = nullptr;

    switch (paramName) {
    case CL_PIPE_PACKET_SIZE:
        srcParamSize = sizeof(cl_uint);
        srcParam = &(pipePacketSize);
        break;
    case CL_PIPE_MAX_PACKETS:
        srcParamSize = sizeof(cl_uint);
        srcParam = &(pipeMaxPackets);
        break;
    case CL_PIPE_PROPERTIES:
        srcParamSize = 0;
        break;
    default:
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcParamSize, getInfoStatus);

    return retVal;
}

void Pipe::setPipeArg(void *memory, uint32_t patchSize, uint32_t rootDeviceIndex) {
    patchWithRequiredSize(memory, patchSize, static_cast<uintptr_t>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex)->getGpuAddressToPatch()));
}

Pipe::~Pipe() = default;
} // namespace NEO
