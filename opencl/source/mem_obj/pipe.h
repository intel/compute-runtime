/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/mem_obj/buffer.h"

namespace NEO {
class Pipe : public MemObj {
  public:
    static const size_t intelPipeHeaderReservedSpace = 128;
    static const cl_ulong maskMagic = 0xFFFFFFFFFFFFFFFFLL;
    static const cl_ulong objectMagic = MemObj::objectMagic | 0x03;
    static Pipe *create(
        Context *context,
        cl_mem_flags flags,
        cl_uint packetSize,
        cl_uint maxPackets,
        const cl_pipe_properties *properties,
        cl_int &errcodeRet);

    ~Pipe() override;

    cl_int getPipeInfo(cl_image_info paramName,
                       size_t paramValueSize,
                       void *paramValue,
                       size_t *paramValueSizeRet);

    void setPipeArg(void *memory, uint32_t patchSize);

  protected:
    Pipe(Context *context,
         cl_mem_flags flags,
         cl_uint packetSize,
         cl_uint maxPackets,
         const cl_pipe_properties *properties,
         void *memoryStorage,
         GraphicsAllocation *gfxAllocation);

    cl_uint pipePacketSize;
    cl_uint pipeMaxPackets;
};
} // namespace NEO
