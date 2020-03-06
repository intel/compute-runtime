/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/image.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeImageGetProperties(
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_properties_t *pImageProperties) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pImageProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_IMAGE_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::Device::fromHandle(hDevice)->imageGetProperties(desc, pImageProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeImageCreate(
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t *phImage) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_IMAGE_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::Device::fromHandle(hDevice)->createImage(desc, phImage);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeImageDestroy(
    ze_image_handle_t hImage) {
    try {
        {
            if (nullptr == hImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Image::fromHandle(hImage)->destroy();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
