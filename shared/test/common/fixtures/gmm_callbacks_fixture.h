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
    void setUp() {
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
        executionEnvironment->incRefInternal();
    }

    void tearDown() {
        executionEnvironment->decRefInternal();
    }

    ExecutionEnvironment *executionEnvironment = nullptr;
    HardwareInfo *hwInfo = nullptr;
};
