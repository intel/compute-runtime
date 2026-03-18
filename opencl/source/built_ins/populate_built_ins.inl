/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"

namespace NEO {
template <typename... KernelsDescArgsT>
void BuiltIn::DispatchInfoBuilder::populate(BuiltIn::Group op, ConstStringRef options, KernelsDescArgsT &&...desc) {
    auto src = kernelsLib.getBuiltinsLib().getBuiltinCode(op, BuiltIn::CodeType::any, clDevice.getDevice());
    ClDeviceVector deviceVector;
    deviceVector.push_back(&clDevice);
    prog.reset(BuiltIn::DispatchInfoBuilder::createProgramFromCode(src, deviceVector).release());
    prog->build(deviceVector, options.data());
    grabKernels(std::forward<KernelsDescArgsT>(desc)...);
}
} // namespace NEO
