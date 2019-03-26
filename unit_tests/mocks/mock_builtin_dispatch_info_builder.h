/*
 * Copyright (C) 2017-2019 Intel Corporation
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

  protected:
    mutable BuiltinOpParams builtinOpParams;
    mutable MultiDispatchInfo multiDispatchInfo;
    BuiltinDispatchInfoBuilder *originalBuilder;
};
