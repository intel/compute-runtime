/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/helpers/cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/buffer.h"
#include "level_zero/api/opencl/source/sharings/d3d/d3d_sharing.h"

namespace NEO {
namespace LEO {

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

        ze_external_memory_import_win32_handle_t windowsImport{};

        windowsImport.handle = sharedHandle;
        windowsImport.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
        windowsImport.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;

        ze_device_mem_alloc_desc_t deviceAllocDesc{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        deviceAllocDesc.pNext = &windowsImport;

        void *usmPtr = nullptr;

        auto ret = zeMemAllocDevice(context->getL0ContextHandle(), &deviceAllocDesc, bufferDesc.ByteWidth, 0, context->getClDevice()->getL0Handle(), &usmPtr);

        if (ret != ZE_RESULT_SUCCESS) {
            return nullptr;
        }

        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
        auto buffer = new Buffer(context, memoryProperties, flags, usmPtr, nullptr, bufferDesc.ByteWidth, false);

        auto d3dBufferObj = new D3DBuffer<D3D>(context, d3dBuffer, bufferStaging, sharedResource);
        buffer->setSharingHandler(d3dBufferObj);

        return buffer;
    }
    ~D3DBuffer() override = default;

  protected:
    D3DBuffer(Context *context, D3DBufferObj *d3dBuffer, D3DBufferObj *bufferStaging, bool sharedResource)
        : D3DSharing<D3D>(context, d3dBuffer, bufferStaging, 0, sharedResource) {};
};

} // namespace LEO
} // namespace NEO
