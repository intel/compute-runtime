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

#pragma once
#include "runtime/sharings/d3d/d3d_sharing.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {
class Buffer;
class Context;

template <typename D3D>
class D3DBuffer : public D3DSharing<D3D> {
    typedef typename D3D::D3DBufferObj D3DBufferObj;
    typedef typename D3D::D3DBufferDesc D3DBufferDesc;

  public:
    static Buffer *create(Context *context, D3DBufferObj *d3dBuffer, cl_mem_flags flags, cl_int *retCode) {
        ErrorCodeHelper err(retCode, CL_SUCCESS);
        auto sharingFcns = context->getSharing<D3DSharingFunctions<D3D>>();
        void *sharedHandle = nullptr;

        D3DBufferDesc bufferDesc = {};
        sharingFcns->getBufferDesc(&bufferDesc, d3dBuffer);

        bool sharedResource = false;
        D3DBufferObj *bufferStaging = nullptr;
        if (bufferDesc.MiscFlags & D3DResourceFlags::MISC_SHARED) {
            bufferStaging = d3dBuffer;
            sharedResource = true;
        } else {
            sharingFcns->createBuffer(&bufferStaging, bufferDesc.ByteWidth);
        }

        sharingFcns->getSharedHandle(bufferStaging, &sharedHandle);
        auto alloc = context->getMemoryManager()->createGraphicsAllocationFromSharedHandle((osHandle)((UINT_PTR)sharedHandle), true);

        auto d3dBufferObj = new D3DBuffer<D3D>(context, d3dBuffer, bufferStaging, sharedResource);

        return Buffer::createSharedBuffer(context, flags, d3dBufferObj, alloc);
    }
    ~D3DBuffer() override = default;

  protected:
    D3DBuffer(Context *context, D3DBufferObj *d3dBuffer, D3DBufferObj *bufferStaging, bool sharedResource)
        : D3DSharing(context, d3dBuffer, bufferStaging, 0, sharedResource){};
};
} // namespace OCLRT
