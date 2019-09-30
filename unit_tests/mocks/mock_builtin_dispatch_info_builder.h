/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/helpers/dispatch_info.h"

using namespace NEO;

class MockBuiltinDispatchInfoBuilder : public BuiltinDispatchInfoBuilder {
  public:
    MockBuiltinDispatchInfoBuilder(BuiltIns &kernelLib, BuiltinDispatchInfoBuilder *origBuilder)
        : BuiltinDispatchInfoBuilder(kernelLib), originalBuilder(origBuilder) {
    }

    virtual void validateInput(const BuiltinOpParams &conf) const {};

    bool buildDispatchInfos(MultiDispatchInfo &mdi, const BuiltinOpParams &conf) const override {
        validateInput(conf);
        builtinOpParams = conf;
        originalBuilder->buildDispatchInfos(mdi, conf);
        for (auto &di : mdi) {
            multiDispatchInfo.push(di);
        }
        return true;
    }

    const BuiltinOpParams *getBuiltinOpParams() const {
        return &builtinOpParams;
    };
    const MultiDispatchInfo *getMultiDispatchInfo() const {
        return &multiDispatchInfo;
    };

    void setFailingArgIndex(uint32_t index) {
        withFailureInjection = true;
        failingArgIndex = index;
    }

    virtual bool setExplicitArg(uint32_t argIndex, size_t argSize, const void *argVal, cl_int &err) const override {
        err = (withFailureInjection && argIndex == failingArgIndex) ? CL_INVALID_ARG_VALUE : CL_SUCCESS;
        return false;
    }

  protected:
    mutable BuiltinOpParams builtinOpParams;
    mutable MultiDispatchInfo multiDispatchInfo;
    BuiltinDispatchInfoBuilder *originalBuilder;
    bool withFailureInjection = false;
    uint32_t failingArgIndex = 0;
};
