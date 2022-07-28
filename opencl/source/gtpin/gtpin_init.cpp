/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtpin_init.h"

#include "gtpin_helpers.h"

using namespace gtpin;
using namespace NEO;

namespace NEO {
bool isGTPinInitialized = false;
gtpin::ocl::gtpin_events_t GTPinCallbacks = {0};
} // namespace NEO

// Do not change this code, needed to avoid compiler optimization that breaks GTPin_Init
// Beginning
void passCreateBuffer(BufferAllocateFPTR src, BufferAllocateFPTR &dst) {
    dst = src;
}
void passFreeBuffer(BufferDeallocateFPTR src, BufferDeallocateFPTR &dst) {
    dst = src;
}
void passMapBuffer(BufferMapFPTR src, BufferMapFPTR &dst) {
    dst = src;
}
void passUnMapBuffer(BufferUnMapFPTR src, BufferUnMapFPTR &dst) {
    dst = src;
}
// End of WA
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

    // Do not change this code, needed to avoid compiler optimization that breaks GTPin_Init
    // Beginning
    auto createBuffer = NEO::gtpinCreateBuffer;
    auto freeBuffer = NEO::gtpinFreeBuffer;
    auto mapBuffer = NEO::gtpinMapBuffer;
    auto unMapBuffer = NEO::gtpinUnmapBuffer;

    passCreateBuffer(createBuffer, pDriverServices->bufferAllocate);
    passFreeBuffer(freeBuffer, pDriverServices->bufferDeallocate);
    passMapBuffer(mapBuffer, pDriverServices->bufferMap);
    passUnMapBuffer(unMapBuffer, pDriverServices->bufferUnMap);
    // End of WA
    GTPinCallbacks = *pGtpinEvents;
    isGTPinInitialized = true;

    return GTPIN_DI_SUCCESS;
}
