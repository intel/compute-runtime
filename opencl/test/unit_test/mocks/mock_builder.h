/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/dispatch_info_builder.h"

using namespace NEO;

namespace NEO {
class Kernel;
}

struct MockBuilder : BuiltinDispatchInfoBuilder {
    using BuiltinDispatchInfoBuilder::BuiltinDispatchInfoBuilder;
    bool buildDispatchInfos(MultiDispatchInfo &d) const override;
    bool buildDispatchInfos(MultiDispatchInfo &d, Kernel *kernel,
                            const uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset) const override;

    mutable bool wasBuildDispatchInfosWithBuiltinOpParamsCalled = false;
    mutable bool wasBuildDispatchInfosWithKernelParamsCalled = false;
    struct Params {
        MultiDispatchInfo multiDispatchInfo;
        Kernel *kernel = nullptr;
        Vec3<size_t> gws = Vec3<size_t>{0, 0, 0};
        Vec3<size_t> elws = Vec3<size_t>{0, 0, 0};
        Vec3<size_t> offset = Vec3<size_t>{0, 0, 0};
    };

    mutable Params paramsReceived;
    Params paramsToUse;
};
