/*
 *
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
#ifndef _ZES_API_EXT_H
#define _ZES_API_EXT_H
#if defined(__cplusplus)
#pragma once
#endif

// 'core' API headers
#include "third_party/level_zero/ze_api_ext.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Intel 'oneAPI' Level-Zero Sysman API common types

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle to a driver instance
typedef ze_driver_handle_t zes_driver_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of device object
typedef ze_device_handle_t zes_device_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle for a Sysman device power domain
typedef struct _zes_pwr_handle_t *zes_pwr_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Get handle of power domains
///
/// @details
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pCount`
ZE_APIEXPORT ze_result_t ZE_APICALL
zesDeviceEnumPowerDomains(
    zes_device_handle_t hDevice, ///< [in] Sysman handle of the device.
    uint32_t *pCount,            ///< [in,out] pointer to the number of components of this type.
                                 ///< if count is zero, then the driver will update the value with the total
                                 ///< number of components of this type.
                                 ///< if count is non-zero, then driver will only retrieve that number of components.
                                 ///< if count is larger than the number of components available, then the
                                 ///< driver will update the value with the correct number of components
                                 ///< that are returned.
    zes_pwr_handle_t *phPower    ///< [in,out][optional][range(0, *pCount)] array of handle of components of
                                 ///< this type
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZES_API_H