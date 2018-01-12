/*
 * Copyright (c) 2018, Intel Corporation
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
#include "gtpin_ocl_interface.h"
#include "CL/cl.h"
#include "runtime/device/device.h"
#include "runtime/device/device_info.h"
#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/platform/platform.h"

using namespace gtpin;

namespace OCLRT {
extern bool isGTPinInitialized;
extern gtpin::ocl::gtpin_events_t GTPinCallbacks;

igc_init_t *pIgcInfo = nullptr;

void gtpinNotifyContextCreate(cl_context context) {
    if (isGTPinInitialized) {
        platform_info_t gtpinPlatformInfo;
        auto pPlatform = platform();
        auto pDevice = pPlatform->getDevice(0);
        GFXCORE_FAMILY genFamily = pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily;
        GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(genFamily);
        gtpinPlatformInfo.gen_version = (gtpin::GTPIN_GEN_VERSION)gtpinHelper.getGenVersion();
        gtpinPlatformInfo.device_id = static_cast<uint32_t>(pDevice->getHardwareInfo().pPlatform->usDeviceID);

        (*GTPinCallbacks.onContextCreate)((context_handle_t)context, &gtpinPlatformInfo, &pIgcInfo);
    }
}

void gtpinNotifyContextDestroy(cl_context context) {
    if (isGTPinInitialized) {
        (*GTPinCallbacks.onContextDestroy)((context_handle_t)context);
    }
}
}
