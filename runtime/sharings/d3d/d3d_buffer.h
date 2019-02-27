/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/sharings/d3d/d3d_sharing.h"

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
