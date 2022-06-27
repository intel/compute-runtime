/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

namespace NEO {
extern Drm **pDrmToReturnFromCreateFunc;
}; // namespace NEO

using namespace NEO;

struct DeviceFactoryLinuxTest : public ::testing::Test {
    void SetUp() override {
        pDrm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
        pDrmToReturnFromCreateFunc = reinterpret_cast<Drm **>(&pDrm);
    }

    void TearDown() override {
    }
    VariableBackup<Drm **> drmBackup{&pDrmToReturnFromCreateFunc};
    DrmMock *pDrm;
    MockExecutionEnvironment executionEnvironment;
};
