/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/cl_local_work_size.h"

#include "shared/source/helpers/local_work_size.h"
#include "shared/source/utilities/logger.h"

#include "opencl/source/context/context.h"
#include "opencl/source/helpers/dispatch_info.h"

#include <cstdint>

namespace NEO {

Vec3<size_t> computeWorkgroupSize(const DispatchInfo &dispatchInfo) {
    size_t workGroupSize[3] = {};
    auto kernel = dispatchInfo.getKernel();

    if (kernel != nullptr) {
        size_t workItems[3] = {dispatchInfo.getGWS().x, dispatchInfo.getGWS().y, dispatchInfo.getGWS().z};
        computeWorkgroupSizeForKernel(kernel->getKernelInfo().kernelDescriptor,
                                      kernel->getMaxKernelWorkGroupSize(),
                                      static_cast<uint32_t>(kernel->getSlmTotalSizePerThreadGroup()),
                                      dispatchInfo.getClDevice().getDevice(),
                                      dispatchInfo.getDim(),
                                      workItems,
                                      workGroupSize);
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
    auto kernel = dispatchInfo.getKernel();
    return createWorkSizeInfoForKernel(kernel->getKernelInfo().kernelDescriptor,
                                       kernel->getMaxKernelWorkGroupSize(),
                                       static_cast<uint32_t>(kernel->getSlmTotalSizePerThreadGroup()),
                                       dispatchInfo.getClDevice().getDevice());
}

} // namespace NEO
