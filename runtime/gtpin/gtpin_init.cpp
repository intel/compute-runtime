/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtpin_init.h"

#include "core/device/device.h"
#include "runtime/device/device_info.h"
#include "runtime/platform/platform.h"

#include "CL/cl.h"
#include "gtpin_helpers.h"

using namespace gtpin;
using namespace NEO;

namespace NEO {
bool isGTPinInitialized = false;
gtpin::ocl::gtpin_events_t GTPinCallbacks = {0};
} // namespace NEO

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

    pDriverServices->bufferAllocate = NEO::gtpinCreateBuffer;
    pDriverServices->bufferDeallocate = NEO::gtpinFreeBuffer;
    pDriverServices->bufferMap = NEO::gtpinMapBuffer;
    pDriverServices->bufferUnMap = NEO::gtpinUnmapBuffer;

    GTPinCallbacks = *pGtpinEvents;
    isGTPinInitialized = true;

    return GTPIN_DI_SUCCESS;
}
