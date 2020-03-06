/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver_handle.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeDriverAllocSharedMem(
    ze_driver_handle_t hDriver,
    const ze_device_mem_alloc_desc_t *deviceDesc,
    const ze_host_mem_alloc_desc_t *hostDesc,
    size_t size,
    size_t alignment,
    ze_device_handle_t hDevice,
    void **pptr) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->allocSharedMem(hDevice, deviceDesc->flags, hostDesc->flags, size, alignment, pptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverAllocDeviceMem(
    ze_driver_handle_t hDriver,
    const ze_device_mem_alloc_desc_t *deviceDesc,
    size_t size,
    size_t alignment,
    ze_device_handle_t hDevice,
    void **pptr) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->allocDeviceMem(hDevice, deviceDesc->flags, size, alignment, pptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverAllocHostMem(
    ze_driver_handle_t hDriver,
    const ze_host_mem_alloc_desc_t *hostDesc,
    size_t size,
    size_t alignment,
    void **pptr) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->allocHostMem(hostDesc->flags, size, alignment, pptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverFreeMem(
    ze_driver_handle_t hDriver,
    void *ptr) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->freeMem(ptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverGetMemAllocProperties(
    ze_driver_handle_t hDriver,
    const void *ptr,
    ze_memory_allocation_properties_t *pMemAllocProperties,
    ze_device_handle_t *phDevice) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pMemAllocProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->getMemAllocProperties(ptr, pMemAllocProperties, phDevice);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverGetMemAddressRange(
    ze_driver_handle_t hDriver,
    const void *ptr,
    void **pBase,
    size_t *pSize) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->getMemAddressRange(ptr, pBase, pSize);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverGetMemIpcHandle(
    ze_driver_handle_t hDriver,
    const void *ptr,
    ze_ipc_mem_handle_t *pIpcHandle) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pIpcHandle)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->getIpcMemHandle(ptr, pIpcHandle);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverOpenMemIpcHandle(
    ze_driver_handle_t hDriver,
    ze_device_handle_t hDevice,
    ze_ipc_mem_handle_t handle,
    ze_ipc_memory_flag_t flags,
    void **pptr) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->openIpcMemHandle(hDevice, handle, flags, pptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverCloseMemIpcHandle(
    ze_driver_handle_t hDriver,
    const void *ptr) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->closeIpcMemHandle(ptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
