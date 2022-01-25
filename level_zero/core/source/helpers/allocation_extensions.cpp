/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/allocation_extensions.h"

#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

ze_result_t handleAllocationExtensions(NEO::GraphicsAllocation *alloc, ze_memory_type_t type, void *pNext, struct DriverHandleImp *driverHandle) {
    if (pNext != nullptr) {
        ze_base_properties_t *extendedProperties =
            reinterpret_cast<ze_base_properties_t *>(pNext);
        if (extendedProperties->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD) {
            ze_external_memory_export_fd_t *extendedMemoryExportProperties =
                reinterpret_cast<ze_external_memory_export_fd_t *>(extendedProperties);
            if (extendedMemoryExportProperties->flags & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
            if (type != ZE_MEMORY_TYPE_DEVICE) {
                return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            uint64_t handle = alloc->peekInternalHandle(driverHandle->getMemoryManager());
            extendedMemoryExportProperties->fd = static_cast<int>(handle);
        } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_WIN32) {
            ze_external_memory_export_win32_handle_t *exportStructure = reinterpret_cast<ze_external_memory_export_win32_handle_t *>(extendedProperties);
            if (exportStructure->flags != ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
            uint64_t handle = alloc->peekInternalHandle(driverHandle->getMemoryManager());
            exportStructure->handle = reinterpret_cast<void *>(handle);
        } else {
            return ZE_RESULT_ERROR_INVALID_ENUMERATION;
        }
    }

    return ZE_RESULT_SUCCESS;
}
} // namespace L0
