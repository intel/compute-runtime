/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"

#include <memory>

namespace NEO {
class MockBuiltins : public BuiltIns {
  public:
    const SipKernel &getSipKernel(SipKernelType type, Device &device) override {
        if (sipKernelsOverride.find(type) != sipKernelsOverride.end()) {
            return *sipKernelsOverride[type];
        }
        getSipKernelCalled = true;
        getSipKernelType = type;
        MockSipData::mockSipKernel->type = type;
        return *MockSipData::mockSipKernel;
    }

    void overrideSipKernel(std::unique_ptr<MockSipKernel> kernel) {
        sipKernelsOverride[kernel->getType()] = std::move(kernel);
    }
    std::unique_ptr<BuiltinDispatchInfoBuilder> setBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, Context &context, Device &device, std::unique_ptr<BuiltinDispatchInfoBuilder> builder);
    std::map<SipKernelType, std::unique_ptr<MockSipKernel>> sipKernelsOverride;
    bool getSipKernelCalled = false;
    SipKernelType getSipKernelType = SipKernelType::COUNT;
};
} // namespace NEO