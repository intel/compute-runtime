/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"

#include "opencl/source/helpers/dispatch_info.h"

using namespace NEO;

class MockBuiltInDispatchInfoBuilder : public BuiltIn::DispatchInfoBuilder {
  public:
    MockBuiltInDispatchInfoBuilder(BuiltIns &kernelLib, ClDevice &clDevice, BuiltIn::DispatchInfoBuilder *origBuilder)
        : BuiltIn::DispatchInfoBuilder(kernelLib, clDevice), originalBuilder(origBuilder) {
    }

    virtual void validateInput(const BuiltIn::OpParams &conf) const {};

    bool buildDispatchInfos(MultiDispatchInfo &mdi) const override {
        validateInput(mdi.peekBuiltinOpParams());

        originalBuilder->buildDispatchInfos(mdi);
        for (auto &di : mdi) {
            multiDispatchInfo.push(di);
        }
        multiDispatchInfo.setBuiltinOpParams(mdi.peekBuiltinOpParams());
        return true;
    }

    const BuiltIn::OpParams *getBuiltinOpParams() const {
        return &multiDispatchInfo.peekBuiltinOpParams();
    };
    const MultiDispatchInfo *getMultiDispatchInfo() const {
        return &multiDispatchInfo;
    };

    void setFailingArgIndex(uint32_t index) {
        withFailureInjection = true;
        failingArgIndex = index;
    }

    bool setExplicitArg(uint32_t argIndex, size_t argSize, const void *argVal, cl_int &err) const override {
        err = (withFailureInjection && argIndex == failingArgIndex) ? CL_INVALID_ARG_VALUE : CL_SUCCESS;
        return false;
    }

  protected:
    mutable MultiDispatchInfo multiDispatchInfo;
    BuiltIn::DispatchInfoBuilder *originalBuilder;
    bool withFailureInjection = false;
    uint32_t failingArgIndex = 0;
};
