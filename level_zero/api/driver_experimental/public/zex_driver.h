/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_DRIVER_H
#define _ZEX_DRIVER_H
#if defined(__cplusplus)
#pragma once
#endif

#include "level_zero/api/driver_experimental/public/zex_api.h"

namespace L0 {

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

} // namespace L0

#endif // _ZEX_DRIVER_H
