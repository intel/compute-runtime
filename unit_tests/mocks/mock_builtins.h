/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/sip.h"
#include "runtime/program/program.h"

#include <memory>

class MockBuiltins : public NEO::BuiltIns {
  public:
    const NEO::SipKernel &getSipKernel(NEO::SipKernelType type, NEO::Device &device) override {
        if (sipKernelsOverride.find(type) != sipKernelsOverride.end()) {
            return *sipKernelsOverride[type];
        }
        getSipKernelCalled = true;
        getSipKernelType = type;
        return BuiltIns::getSipKernel(type, device);
    }

    void overrideSipKernel(std::unique_ptr<NEO::SipKernel> kernel) {
        sipKernelsOverride[kernel->getType()] = std::move(kernel);
    }

    NEO::BuiltIns *originalGlobalBuiltins = nullptr;
    std::map<NEO::SipKernelType, std::unique_ptr<NEO::SipKernel>> sipKernelsOverride;
    bool getSipKernelCalled = false;
    NEO::SipKernelType getSipKernelType = NEO::SipKernelType::COUNT;
};
