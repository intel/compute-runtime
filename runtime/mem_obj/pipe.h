/*
 * Copyright (c) 2017, Intel Corporation
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

#pragma once
#include "runtime/mem_obj/buffer.h"

namespace OCLRT {
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
} // namespace OCLRT
