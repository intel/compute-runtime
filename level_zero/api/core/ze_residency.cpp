/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceMakeMemoryResident(
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->makeMemoryResident(ptr, size);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceEvictMemory(
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->evictMemory(ptr, size);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceMakeImageResident(
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->makeImageResident(hImage);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceEvictImage(
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->evictImage(hImage);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
