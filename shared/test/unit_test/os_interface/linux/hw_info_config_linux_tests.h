/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

using namespace NEO;

struct HwInfoConfigTestLinux : public HwInfoConfigTest {
    static void mockCpuidex(int *cpuInfo, int functionId, int subfunctionId) {
        if (subfunctionId == 0) {
            cpuInfo[0] = 0x7F;
        }
        if (subfunctionId == 1) {
            cpuInfo[0] = 0x1F;
        }
        if (subfunctionId == 2) {
            cpuInfo[0] = 0;
        }
    }

    void SetUp() override {
        HwInfoConfigTest::SetUp();
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());

        osInterface = new OSInterface();
        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

        drm->storedEUVal = pInHwInfo.gtSystemInfo.EUCount;
        drm->storedSSVal = pInHwInfo.gtSystemInfo.SubSliceCount;

        rt_cpuidex_func = CpuInfo::cpuidexFunc;
        CpuInfo::cpuidexFunc = mockCpuidex;
    }

    void TearDown() override {
        CpuInfo::cpuidexFunc = rt_cpuidex_func;

        delete osInterface;

        HwInfoConfigTest::TearDown();
    }

    OSInterface *osInterface;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    DrmMock *drm;

    void (*rt_cpuidex_func)(int *, int, int);
};
