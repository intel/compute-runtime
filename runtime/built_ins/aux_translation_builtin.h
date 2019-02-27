/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/helpers/dispatch_info_builder.h"

#include <memory>

namespace OCLRT {
template <typename HWFamily>
class BuiltInOp<HWFamily, EBuiltInOps::AuxTranslation> : public BuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device);
    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const override;

  protected:
    void resizeKernelInstances(size_t size) const;

    Kernel *baseKernel = nullptr;
    mutable std::vector<std::unique_ptr<Kernel>> convertToNonAuxKernel;
    mutable std::vector<std::unique_ptr<Kernel>> convertToAuxKernel;
};

} // namespace OCLRT
