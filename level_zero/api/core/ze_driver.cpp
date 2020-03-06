/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver.h"
#include "level_zero/core/source/driver_handle.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeInit(
    ze_init_flag_t flags) {
    try {
        {
        }
        return L0::init(flags);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverGet(
    uint32_t *pCount,
    ze_driver_handle_t *phDrivers) {
    try {
        {
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::driverHandleGet(pCount, phDrivers);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverGetProperties(
    ze_driver_handle_t hDriver,
    ze_driver_properties_t *pProperties) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->getProperties(pProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverGetApiVersion(
    ze_driver_handle_t hDriver,
    ze_api_version_t *version) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == version)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->getApiVersion(version);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverGetIPCProperties(
    ze_driver_handle_t hDriver,
    ze_driver_ipc_properties_t *pIPCProperties) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pIPCProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->getIPCProperties(pIPCProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDriverGetExtensionFunctionAddress(
    ze_driver_handle_t hDriver,
    const char *pFuncName,
    void **pfunc) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pFuncName)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pfunc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::DriverHandle::fromHandle(hDriver)->getExtensionFunctionAddress(pFuncName, pfunc);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
