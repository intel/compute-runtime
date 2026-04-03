/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_DRIVER_H
#define _ZEX_DRIVER_H

#if defined(__cplusplus)
#pragma once
#endif

#include <level_zero/ze_api.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef ZEX_DRIVER_IMPORT_HOST_POINTER_NAME
/// @brief Queue copy operations offload hint extension name
#define ZEX_DRIVER_IMPORT_HOST_POINTER_NAME "ZEX_driver_import_host_pointer"
#endif // ZEX_DRIVER_IMPORT_HOST_POINTER_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Queue copy operations offload hint extension version(s)
typedef enum _zex_driver_import_host_pointer_version_t {
    ZEX_DRIVER_IMPORT_HOST_POINTER_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZEX_DRIVER_IMPORT_HOST_POINTER_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZEX_DRIVER_IMPORT_HOST_POINTER_VERSION_FORCE_UINT32 = 0x7fffffff
} zex_driver_import_host_pointer_version_t;

ze_result_t ZE_APICALL
zexDriverImportExternalPointer(
    ze_driver_handle_t hDriver, ///< [in] handle of the driver
    void *ptr,                  ///< [in] pointer to be imported to the driver
    size_t size                 ///< [in] size to be imported
);

ze_result_t ZE_APICALL
zexDriverReleaseImportedPointer(
    ze_driver_handle_t hDriver, ///< [in] handle of the driver
    void *ptr                   ///< [in] pointer to be released from the driver
);

ze_result_t ZE_APICALL
zexDriverGetHostPointerBaseAddress(
    ze_driver_handle_t hDriver, ///< [in] handle of the driver
    void *ptr,                  ///< [in] pointer to be checked if imported to the driver
    void **baseAddress          ///< [out] if not null, returns address of the base pointer of the imported pointer
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZEX_DRIVER_H
