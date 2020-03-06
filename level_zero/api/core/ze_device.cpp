/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/driver.h"
#include "level_zero/core/source/driver_handle.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceGet(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_device_handle_t *phDevices) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->getDevice(pCount, phDevices);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetSubDevices(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_handle_t *phSubdevices) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getSubDevices(pCount, phSubdevices);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetProperties(
    ze_device_handle_t hDevice,
    ze_device_properties_t *pDeviceProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pDeviceProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getProperties(pDeviceProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetComputeProperties(
    ze_device_handle_t hDevice,
    ze_device_compute_properties_t *pComputeProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pComputeProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getComputeProperties(pComputeProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetKernelProperties(
    ze_device_handle_t hDevice,
    ze_device_kernel_properties_t *pKernelProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pKernelProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getKernelProperties(pKernelProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetMemoryProperties(
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_device_memory_properties_t *pMemProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getMemoryProperties(pCount, pMemProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetMemoryAccessProperties(
    ze_device_handle_t hDevice,
    ze_device_memory_access_properties_t *pMemAccessProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pMemAccessProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getMemoryAccessProperties(pMemAccessProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetCacheProperties(
    ze_device_handle_t hDevice,
    ze_device_cache_properties_t *pCacheProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCacheProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getCacheProperties(pCacheProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetImageProperties(
    ze_device_handle_t hDevice,
    ze_device_image_properties_t *pImageProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pImageProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getDeviceImageProperties(pImageProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceGetP2PProperties(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_device_p2p_properties_t *pP2PProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hPeerDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pP2PProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->getP2PProperties(hPeerDevice, pP2PProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceCanAccessPeer(
    ze_device_handle_t hDevice,
    ze_device_handle_t hPeerDevice,
    ze_bool_t *value) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hPeerDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == value)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->canAccessPeer(hPeerDevice, value);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceSetLastLevelCacheConfig(
    ze_device_handle_t hDevice,
    ze_cache_config_t cacheConfig) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->setLastLevelCacheConfig(cacheConfig);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
