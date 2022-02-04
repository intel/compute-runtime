/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/d3d/d3d_sharing.h"

namespace NEO {
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
        AllocationProperties properties = {context->getDevice(0)->getRootDeviceIndex(),
                                           false, // allocateMemory
                                           0,     // size
                                           AllocationType::SHARED_BUFFER,
                                           false, // isMultiStorageAllocation
                                           context->getDeviceBitfieldForAllocation(context->getDevice(0)->getRootDeviceIndex())};
        auto alloc = context->getMemoryManager()->createGraphicsAllocationFromSharedHandle(toOsHandle(sharedHandle), properties, true, false);

        auto d3dBufferObj = new D3DBuffer<D3D>(context, d3dBuffer, bufferStaging, sharedResource);

        auto rootDeviceIndex = alloc->getRootDeviceIndex();
        auto multiGraphicsAllocation = MultiGraphicsAllocation(rootDeviceIndex);
        multiGraphicsAllocation.addAllocation(alloc);

        return Buffer::createSharedBuffer(context, flags, d3dBufferObj, std::move(multiGraphicsAllocation));
    }
    ~D3DBuffer() override = default;

  protected:
    D3DBuffer(Context *context, D3DBufferObj *d3dBuffer, D3DBufferObj *bufferStaging, bool sharedResource)
        : D3DSharing(context, d3dBuffer, bufferStaging, 0, sharedResource){};
};
} // namespace NEO
