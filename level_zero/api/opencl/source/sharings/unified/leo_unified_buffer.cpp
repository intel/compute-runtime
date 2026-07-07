/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/unified/leo_unified_buffer.h"

#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"

namespace NEO {
namespace LEO {

Buffer *UnifiedBuffer::createSharedUnifiedBuffer(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription extMem, cl_int *errcodeRet) {
    union {
        ze_external_memory_import_fd_t linuxImport;
        ze_external_memory_import_win32_handle_t windowsImport;
    } handleImport{};

    if (extMem.type == UnifiedSharingHandleType::linuxFd) {
        handleImport.linuxImport.fd = toOsHandle(extMem.handle);
        handleImport.linuxImport.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
        handleImport.linuxImport.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    } else {
        handleImport.windowsImport.handle = extMem.handle;
        handleImport.windowsImport.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
        handleImport.windowsImport.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    }

    ze_device_mem_alloc_desc_t deviceAllocDesc{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceAllocDesc.pNext = &handleImport;

    void *usmPtr = nullptr;

    auto ret = zeMemAllocDevice(context->getL0ContextHandle(), &deviceAllocDesc, extMem.size, 0, context->getClDevice()->getL0Handle(), &usmPtr);

    if (ret != ZE_RESULT_SUCCESS) {
        return nullptr;
    }

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto buffer = new Buffer(context, memoryProperties, flags, usmPtr, nullptr, extMem.size, false);

    auto sharingHandler = new UnifiedBuffer(context->getSharing<UnifiedSharingFunctions>(), extMem.type);
    buffer->setSharingHandler(sharingHandler);

    return buffer;
}

} // namespace LEO
} // namespace NEO
