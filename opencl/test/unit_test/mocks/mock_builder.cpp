/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_builder.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

bool MockBuilder::buildDispatchInfos(MultiDispatchInfo &d) const {
    wasBuildDispatchInfosWithBuiltinOpParamsCalled = true;
    paramsReceived.multiDispatchInfo.setBuiltinOpParams(d.peekBuiltinOpParams());
    return true;
}

bool MockBuilder::buildDispatchInfos(MultiDispatchInfo &d, Kernel *kernel,
                                     const uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset) const {
    paramsReceived.kernel = kernel;
    paramsReceived.gws = gws;
    paramsReceived.elws = elws;
    paramsReceived.offset = offset;
    wasBuildDispatchInfosWithKernelParamsCalled = true;

    DispatchInfoBuilder<NEO::SplitDispatch::Dim::d3D, NEO::SplitDispatch::SplitMode::noSplit> dispatchInfoBuilder(clDevice);
    dispatchInfoBuilder.setKernel(paramsToUse.kernel);
    dispatchInfoBuilder.setDispatchGeometry(dim, paramsToUse.gws, paramsToUse.elws, paramsToUse.offset);
    dispatchInfoBuilder.bake(d);

    cloneMdi(paramsReceived.multiDispatchInfo, d);
    return true;
}