/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

gtpin::GTPIN_DI_STATUS GTPin_Init(gtpin::ocl::gtpin_events_t *pGtpinEvents, gtpin::driver_services_t *pDriverServices, gtpin::interface_version_t *pDriverVersion);

#ifdef __cplusplus
}
#endif
