/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/context/context.h"
#include "runtime/mem_obj/pipe.h"
#include "runtime/helpers/get_info.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {

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

    while (true) {
        auto size = static_cast<size_t>(packetSize * (maxPackets + 1) + intelPipeHeaderReservedSpace);
        GraphicsAllocation *memory = memoryManager->allocateGraphicsMemoryInPreferredPool(true, true, false, false, nullptr, size, GraphicsAllocation::AllocationType::PIPE);
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
    patchWithRequiredSize(memory, patchSize, (uintptr_t)getCpuAddress());
}

Pipe::~Pipe() = default;
} // namespace OCLRT
