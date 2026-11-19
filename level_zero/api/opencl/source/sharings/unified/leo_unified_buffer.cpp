/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/unified/leo_unified_buffer.h"

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"

namespace NEO {
namespace LEO {

namespace {

union ExternalMemoryImportDesc {
    ze_external_memory_import_fd_t linuxImport;
    ze_external_memory_import_win32_handle_t windowsImport;
};

void fillExternalMemoryImportDesc(const UnifiedSharingMemoryDescription &extMem, ExternalMemoryImportDesc &importDesc) {
    if (extMem.type == UnifiedSharingHandleType::linuxFd) {
        importDesc.linuxImport.fd = toOsHandle(extMem.handle);
        importDesc.linuxImport.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
        importDesc.linuxImport.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    } else {
        importDesc.windowsImport = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32, nullptr, ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32, extMem.handle, nullptr};
    }
}

} // namespace

Buffer *UnifiedBuffer::importExternalMemoryBuffer(Context *context, MemoryProperties memoryProperties, cl_mem_flags flags, UnifiedSharingMemoryDescription extMem,
                                                  ClDevice *importDevice, cl_int *errcodeRet) {
    ExternalMemoryImportDesc importDesc{};
    fillExternalMemoryImportDesc(extMem, importDesc);

    ze_device_mem_alloc_desc_t deviceAllocDesc{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceAllocDesc.pNext = &importDesc;

    void *usmPtr = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    auto ret = zeMemAllocDevice(context->getL0ContextHandle(), &deviceAllocDesc, extMem.size, 0, importDevice->getL0Handle(), &usmPtr);
    if (ret != ZE_RESULT_SUCCESS) {
        err.set(CL_INVALID_MEM_OBJECT);
        return nullptr;
    }

    memoryProperties.pDevice = &importDevice->getDevice();
    return new Buffer(context, memoryProperties, flags, usmPtr, nullptr, extMem.size, false);
}

Buffer *UnifiedBuffer::createSharedUnifiedBuffer(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription extMem, cl_int *errcodeRet) {
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto buffer = importExternalMemoryBuffer(context, memoryProperties, flags, extMem, context->getClDevice(), errcodeRet);
    if (buffer == nullptr) {
        return nullptr;
    }

    auto sharingHandler = new UnifiedBuffer(context->getSharing<UnifiedSharingFunctions>(), extMem.type);
    buffer->setSharingHandler(sharingHandler);

    return buffer;
}

ClDevice *UnifiedBuffer::selectExternalMemoryImportDevice(Context *context, const MemoryProperties &memoryProperties) {
    if (!memoryProperties.associatedDevices.empty()) {
        for (auto clDevice : context->getClDevices()) {
            if (&clDevice->getDevice() == memoryProperties.associatedDevices[0]) {
                return clDevice;
            }
        }
    }
    return context->getClDevice();
}

} // namespace LEO
} // namespace NEO
