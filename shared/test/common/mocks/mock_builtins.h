/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/test/common/mocks/mock_sip.h"

#include <map>
#include <memory>

namespace NEO {
class MockBuiltins : public BuiltIns {
  public:
    using BuiltIns::perContextSipKernels;
    using BuiltIns::sipKernels;

    const SipKernel &getSipKernel(SipKernelType type, Device &device) override {
        getSipKernelCalled = true;
        getSipKernelType = type;
        if (sipKernelsOverride.find(type) != sipKernelsOverride.end()) {
            return *sipKernelsOverride[type];
        }
        if (callBaseGetSipKernel) {
            return BuiltIns::getSipKernel(type, device);
        }
        MockSipData::mockSipKernel->type = type;
        return *MockSipData::mockSipKernel;
    }

    void overrideSipKernel(std::unique_ptr<MockSipKernel> kernel) {
        sipKernelsOverride[kernel->getType()] = std::move(kernel);
    }
    std::map<SipKernelType, std::unique_ptr<MockSipKernel>> sipKernelsOverride;
    bool getSipKernelCalled = false;
    bool callBaseGetSipKernel = false;
    SipKernelType getSipKernelType = SipKernelType::count;
};
} // namespace NEO
