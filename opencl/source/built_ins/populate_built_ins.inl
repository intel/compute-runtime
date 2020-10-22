/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"

namespace NEO {
template <typename... KernelsDescArgsT>
void BuiltinDispatchInfoBuilder::populate(ClDevice &device, EBuiltInOps::Type op, ConstStringRef options, KernelsDescArgsT &&... desc) {
    auto src = kernelsLib.getBuiltinsLib().getBuiltinCode(op, BuiltinCode::ECodeType::Any, device.getDevice());
    ClDeviceVector deviceVector;
    deviceVector.push_back(&device);
    prog.reset(BuiltinDispatchInfoBuilder::createProgramFromCode(src, deviceVector).release());
    prog->build(0, nullptr, options.data(), nullptr, nullptr, kernelsLib.isCacheingEnabled());
    grabKernels(std::forward<KernelsDescArgsT>(desc)...);
}
} // namespace NEO
