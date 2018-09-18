/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"

namespace OCLRT {

struct GmmEnvironmentFixture {
    virtual void SetUp() {
        executionEnvironment.initGmm(*platformDevices);
    }
    virtual void TearDown(){};
    ExecutionEnvironment executionEnvironment;
};
} // namespace OCLRT
