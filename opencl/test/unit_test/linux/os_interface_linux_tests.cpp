/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/linux/os_interface.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

namespace NEO {
extern GMM_INIT_IN_ARGS passedInputArgs;
extern bool copyInputArgs;

TEST(OsInterfaceTest, whenOsInterfaceSetupsGmmInputArgsThenProperFileDescriptorIsSet) {
    MockExecutionEnvironment executionEnvironment;
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    auto osInterface = new OSInterface();
    rootDeviceEnvironment->osInterface.reset(osInterface);

    auto drm = new DrmMock(*rootDeviceEnvironment);
    osInterface->get()->setDrm(drm);

    GMM_INIT_IN_ARGS gmmInputArgs = {};
    EXPECT_EQ(0u, gmmInputArgs.FileDescriptor);
    osInterface->setGmmInputArgs(&gmmInputArgs);
    EXPECT_NE(0u, gmmInputArgs.FileDescriptor);

    auto expectedFileDescriptor = drm->getFileDescriptor();
    EXPECT_EQ(static_cast<uint32_t>(expectedFileDescriptor), gmmInputArgs.FileDescriptor);
}

TEST(GmmHelperTest, whenCreateGmmHelperWithoutOsInterfaceThenPassedFileDescriptorIsZeroed) {
    std::unique_ptr<GmmHelper> gmmHelper;
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    uint32_t expectedFileDescriptor = 0u;

    gmmHelper.reset(new GmmHelper(nullptr, defaultHwInfo.get()));
    EXPECT_EQ(expectedFileDescriptor, passedInputArgs.FileDescriptor);
}
} // namespace NEO
