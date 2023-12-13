/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"

namespace NEO {
template <typename... KernelsDescArgsT>
void BuiltinDispatchInfoBuilder::populate(EBuiltInOps::Type op, ConstStringRef options, KernelsDescArgsT &&...desc) {
    auto src = kernelsLib.getBuiltinsLib().getBuiltinCode(op, BuiltinCode::ECodeType::any, clDevice.getDevice());
    ClDeviceVector deviceVector;
    deviceVector.push_back(&clDevice);
    prog.reset(BuiltinDispatchInfoBuilder::createProgramFromCode(src, deviceVector).release());
    prog->build(deviceVector, options.data());
    grabKernels(std::forward<KernelsDescArgsT>(desc)...);
}
} // namespace NEO
