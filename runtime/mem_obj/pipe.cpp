/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/pipe.h"

#include "runtime/context/context.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {

Pipe::Pipe(Context *context,
           cl_mem_flags flags,
           cl_uint packetSize,
           cl_uint maxPackets,
           const cl_pipe_properties *properties,
           void *memoryStorage,
           GraphicsAllocation *gfxAllocation)
    : MemObj(context,
             CL_MEM_OBJECT_PIPE,
             flags,
             static_cast<size_t>(packetSize * (maxPackets + 1) + intelPipeHeaderReservedSpace),
             memoryStorage,
             nullptr,
             gfxAllocation,
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

    MemoryProperties memoryProperties{flags};
    MemoryPropertiesFlags memoryPropertiesFlags = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    while (true) {
        auto size = static_cast<size_t>(packetSize * (maxPackets + 1) + intelPipeHeaderReservedSpace);
        AllocationProperties allocProperties =
            MemoryPropertiesParser::getAllocationProperties(memoryPropertiesFlags, true, size, GraphicsAllocation::AllocationType::PIPE, false);
        GraphicsAllocation *memory = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);
        if (!memory) {
            errcodeRet = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        pPipe = new (std::nothrow) Pipe(context, flags, packetSize, maxPackets, properties, memory->getUnderlyingBuffer(), memory);
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
    size_t srcParamSize = 0;
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

    default:
        break;
    }

    retVal = ::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = srcParamSize;
    }

    return retVal;
}

void Pipe::setPipeArg(void *memory, uint32_t patchSize) {
    patchWithRequiredSize(memory, patchSize, static_cast<uintptr_t>(getGraphicsAllocation()->getGpuAddressToPatch()));
}

Pipe::~Pipe() = default;
} // namespace NEO
