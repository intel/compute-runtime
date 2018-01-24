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
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/device/device_info.h"
#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/kernel/kernel.h"
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

void gtpinNotifyKernelCreate(cl_kernel kernel) {
    if (isGTPinInitialized) {
        auto pKernel = castToObject<Kernel>(kernel);
        Context *pContext = &(pKernel->getContext());
        cl_context context = (cl_context)pContext;
        const KernelInfo &kInfo = pKernel->getKernelInfo();
        instrument_params_in_t paramsIn;
        paramsIn.kernel_type = GTPIN_KERNEL_TYPE_CS;
        paramsIn.simd = (GTPIN_SIMD_WIDTH)kInfo.getMaxSimdSize();
        paramsIn.orig_kernel_binary = (uint8_t *)pKernel->getKernelHeap();
        paramsIn.orig_kernel_size = static_cast<uint32_t>(pKernel->getKernelHeapSize());
        paramsIn.buffer_type = GTPIN_BUFFER_BINDFULL;
        paramsIn.buffer_desc.BTI = kInfo.patchInfo.bindingTableState->Count;
        paramsIn.igc_hash_id = kInfo.heapInfo.pKernelHeader->ShaderHashCode;
        paramsIn.kernel_name = (char *)kInfo.name.c_str();
        paramsIn.igc_info = nullptr;
        instrument_params_out_t paramsOut = {0};
        (*GTPinCallbacks.onKernelCreate)((context_handle_t)(cl_context)context, &paramsIn, &paramsOut);
        pKernel->substituteKernelHeap(paramsOut.inst_kernel_binary, paramsOut.inst_kernel_size);
        pKernel->setKernelId(paramsOut.kernel_id);
    }
}
}
