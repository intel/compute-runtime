/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <typename... KernelsDescArgsT>
void BuiltinDispatchInfoBuilder::populate(Device &device, EBuiltInOps::Type op, ConstStringRef options, KernelsDescArgsT &&... desc) {
    auto src = kernelsLib.getBuiltinsLib().getBuiltinCode(op, BuiltinCode::ECodeType::Any, device);
    prog.reset(BuiltinsLib::createProgramFromCode(src, device).release());
    prog->build(0, nullptr, options.data(), nullptr, nullptr, kernelsLib.isCacheingEnabled());
    grabKernels(std::forward<KernelsDescArgsT>(desc)...);
}
} // namespace NEO
