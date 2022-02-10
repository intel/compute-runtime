/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/cl_local_work_size.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_work_size.h"

#include "opencl/source/context/context.h"
#include "opencl/source/helpers/dispatch_info.h"

#include <cmath>
#include <cstdint>
#include <ctime>

namespace NEO {

Vec3<size_t> computeWorkgroupSize(const DispatchInfo &dispatchInfo) {
    size_t workGroupSize[3] = {};
    auto kernel = dispatchInfo.getKernel();

    if (kernel != nullptr) {
        auto &device = dispatchInfo.getClDevice();
        const auto &hwInfo = device.getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        if (DebugManager.flags.EnableComputeWorkSizeND.get()) {
            WorkSizeInfo wsInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);
            if (wsInfo.slmTotalSize == 0 && !wsInfo.hasBarriers && !wsInfo.imgUsed && hwHelper.preferSmallWorkgroupSizeForKernel(kernel->getKernelInfo().heapInfo.KernelUnpaddedSize, hwInfo) &&
                ((dispatchInfo.getDim() == 1) && (dispatchInfo.getGWS().x % wsInfo.simdSize * 2 == 0))) {
                wsInfo.maxWorkGroupSize = wsInfo.simdSize * 2;
            }

            size_t workItems[3] = {dispatchInfo.getGWS().x, dispatchInfo.getGWS().y, dispatchInfo.getGWS().z};
            computeWorkgroupSizeND(wsInfo, workGroupSize, workItems, dispatchInfo.getDim());
        } else {
            auto maxWorkGroupSize = kernel->getMaxKernelWorkGroupSize();
            auto simd = kernel->getKernelInfo().getMaxSimdSize();
            size_t workItems[3] = {dispatchInfo.getGWS().x, dispatchInfo.getGWS().y, dispatchInfo.getGWS().z};
            if (dispatchInfo.getDim() == 1) {
                computeWorkgroupSize1D(maxWorkGroupSize, workGroupSize, workItems, simd);
            } else if (DebugManager.flags.EnableComputeWorkSizeSquared.get() && dispatchInfo.getDim() == 2) {
                computeWorkgroupSizeSquared(maxWorkGroupSize, workGroupSize, workItems, simd, dispatchInfo.getDim());
            } else {
                computeWorkgroupSize2D(maxWorkGroupSize, workGroupSize, workItems, simd);
            }
        }
    }
    DBG_LOG(PrintLWSSizes, "Input GWS enqueueBlocked", dispatchInfo.getGWS().x, dispatchInfo.getGWS().y, dispatchInfo.getGWS().z,
            " Driver deduced LWS", workGroupSize[0], workGroupSize[1], workGroupSize[2]);
    return {workGroupSize[0], workGroupSize[1], workGroupSize[2]};
}

Vec3<size_t> generateWorkgroupSize(const DispatchInfo &dispatchInfo) {
    return (dispatchInfo.getEnqueuedWorkgroupSize().x == 0) ? computeWorkgroupSize(dispatchInfo) : dispatchInfo.getEnqueuedWorkgroupSize();
}

Vec3<size_t> generateWorkgroupsNumber(const DispatchInfo &dispatchInfo) {
    return generateWorkgroupsNumber(dispatchInfo.getGWS(), dispatchInfo.getLocalWorkgroupSize());
}

void provideLocalWorkGroupSizeHints(Context *context, const DispatchInfo &dispatchInfo) {
    if (context != nullptr && context->isProvidingPerformanceHints() && dispatchInfo.getDim() <= 3) {
        size_t preferredWorkGroupSize[3];

        auto lws = computeWorkgroupSize(dispatchInfo);
        preferredWorkGroupSize[0] = lws.x;
        preferredWorkGroupSize[1] = lws.y;
        preferredWorkGroupSize[2] = lws.z;

        const auto &kernelInfo = dispatchInfo.getKernel()->getKernelInfo();
        if (dispatchInfo.getEnqueuedWorkgroupSize().x == 0) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, NULL_LOCAL_WORKGROUP_SIZE, kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(),
                                            preferredWorkGroupSize[0], preferredWorkGroupSize[1], preferredWorkGroupSize[2]);
        } else {
            size_t localWorkSizesIn[3] = {dispatchInfo.getEnqueuedWorkgroupSize().x, dispatchInfo.getEnqueuedWorkgroupSize().y, dispatchInfo.getEnqueuedWorkgroupSize().z};
            for (auto i = 0u; i < dispatchInfo.getDim(); i++) {
                if (localWorkSizesIn[i] != preferredWorkGroupSize[i]) {
                    context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, BAD_LOCAL_WORKGROUP_SIZE,
                                                    localWorkSizesIn[0], localWorkSizesIn[1], localWorkSizesIn[2],
                                                    kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str(),
                                                    preferredWorkGroupSize[0], preferredWorkGroupSize[1], preferredWorkGroupSize[2]);
                    break;
                }
            }
        }
    }
}

WorkSizeInfo createWorkSizeInfoFromDispatchInfo(const DispatchInfo &dispatchInfo) {
    auto &device = dispatchInfo.getClDevice();
    const auto &kernelInfo = dispatchInfo.getKernel()->getKernelInfo();
    auto numThreadsPerSubSlice = static_cast<uint32_t>(device.getSharedDeviceInfo().maxNumEUsPerSubSlice) *
                                 device.getSharedDeviceInfo().numThreadsPerEU;
    WorkSizeInfo wsInfo(dispatchInfo.getKernel()->getMaxKernelWorkGroupSize(),
                        kernelInfo.kernelDescriptor.kernelAttributes.usesBarriers(),
                        static_cast<uint32_t>(kernelInfo.getMaxSimdSize()),
                        static_cast<uint32_t>(dispatchInfo.getKernel()->getSlmTotalSize()),
                        &device.getHardwareInfo(),
                        numThreadsPerSubSlice,
                        static_cast<uint32_t>(device.getSharedDeviceInfo().localMemSize),
                        false,
                        false,
                        kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresDisabledEUFusion);
    wsInfo.setIfUseImg(kernelInfo);

    return wsInfo;
}

} // namespace NEO
