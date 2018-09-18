/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtpin_init.h"
#include "gtpin_helpers.h"
#include "CL/cl.h"
#include "runtime/device/device.h"
#include "runtime/device/device_info.h"
#include "runtime/platform/platform.h"

using namespace gtpin;
using namespace OCLRT;

namespace OCLRT {
bool isGTPinInitialized = false;
gtpin::ocl::gtpin_events_t GTPinCallbacks = {0};
} // namespace OCLRT

GTPIN_DI_STATUS GTPin_Init(gtpin::ocl::gtpin_events_t *pGtpinEvents, driver_services_t *pDriverServices,
                           interface_version_t *pDriverVersion) {
    if (isGTPinInitialized) {
        return GTPIN_DI_ERROR_INSTANCE_ALREADY_CREATED;
    }
    if (pDriverVersion != nullptr) {
        // GT-Pin is asking to obtain GT-Pin Interface version that is supported
        pDriverVersion->common = gtpin::GTPIN_COMMON_INTERFACE_VERSION;
        pDriverVersion->specific = gtpin::ocl::GTPIN_OCL_INTERFACE_VERSION;

        if ((pDriverServices == nullptr) || (pGtpinEvents == nullptr)) {
            return GTPIN_DI_SUCCESS;
        }
    }
    if ((pDriverServices == nullptr) || (pGtpinEvents == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    if ((pGtpinEvents->onContextCreate == nullptr) ||
        (pGtpinEvents->onContextDestroy == nullptr) ||
        (pGtpinEvents->onKernelCreate == nullptr) ||
        (pGtpinEvents->onKernelSubmit == nullptr) ||
        (pGtpinEvents->onCommandBufferCreate == nullptr) ||
        (pGtpinEvents->onCommandBufferComplete == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }

    pDriverServices->bufferAllocate = OCLRT::gtpinCreateBuffer;
    pDriverServices->bufferDeallocate = OCLRT::gtpinFreeBuffer;
    pDriverServices->bufferMap = OCLRT::gtpinMapBuffer;
    pDriverServices->bufferUnMap = OCLRT::gtpinUnmapBuffer;

    GTPinCallbacks = *pGtpinEvents;
    isGTPinInitialized = true;

    return GTPIN_DI_SUCCESS;
}
