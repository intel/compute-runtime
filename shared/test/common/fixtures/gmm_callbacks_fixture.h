/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/test/common/helpers/execution_environment_helper.h"

using namespace NEO;

struct GmmCallbacksFixture {
    void SetUp() {
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
        executionEnvironment->incRefInternal();
    }

    void TearDown() {
        executionEnvironment->decRefInternal();
    }

    ExecutionEnvironment *executionEnvironment = nullptr;
    HardwareInfo *hwInfo = nullptr;
};
