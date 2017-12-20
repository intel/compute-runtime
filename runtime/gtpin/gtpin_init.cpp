/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
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
}

GTPIN_DI_STATUS GTPin_Init(gtpin::dx11::gtpin_events_t *pGtpinEvents, driver_services_t *pDriverServices,
                           uint32_t *pDriverVersion) {
    char ver[128] = {'\0'};
    uint32_t driverVersion = 0;

    if (isGTPinInitialized) {
        return GTPIN_DI_ERROR_INSTANCE_ALREADY_CREATED;
    }
    if (pDriverVersion != nullptr) {
        // GT-Pin is asking to obtain driver version
        auto pPlatform = platform();
        Device *pDevice = pPlatform->getDevice(0);
        pDevice->getDeviceInfo(CL_DRIVER_VERSION, sizeof(ver), &ver[0], nullptr);
        uint32_t nums[3] = {0};
        int numVerSegments = sscanf(&ver[0], "%u.%u.%u", &nums[0], &nums[1], &nums[2]);
        for (int verSeg = 0; verSeg < numVerSegments; verSeg++) {
            driverVersion <<= 8;
            driverVersion |= nums[verSeg];
        }
        *pDriverVersion = driverVersion;

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
        (pGtpinEvents->onDraw == nullptr) ||
        (pGtpinEvents->onKernelSubmit == nullptr) ||
        (pGtpinEvents->onCommandBufferCreate == nullptr) ||
        (pGtpinEvents->onCommandBufferSubmit == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }

    pDriverServices->bufferAllocate = OCLRT::gtpinCreateBuffer;
    pDriverServices->bufferDeallocate = nullptr;
    pDriverServices->bufferMap = nullptr;
    pDriverServices->bufferUnMap = nullptr;

    isGTPinInitialized = true;

    return GTPIN_DI_SUCCESS;
}
