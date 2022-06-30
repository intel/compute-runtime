/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(DrmVersionTest, givenDrmVersionI915WhenCheckingDrmSupportThenSuccessIsReturned) {
    int fileDescriptor = 123;
    VariableBackup<const char *> backup(&SysCalls::drmVersion);
    SysCalls::drmVersion = "i915";
    EXPECT_TRUE(Drm::isDrmSupported(fileDescriptor));
}

TEST(DrmVersionTest, givenInvalidDrmVersionWhenCheckingDrmSupportThenFalseIsReturned) {
    int fileDescriptor = 123;
    VariableBackup<const char *> backup(&SysCalls::drmVersion);
    SysCalls::drmVersion = "unknown";
    EXPECT_FALSE(Drm::isDrmSupported(fileDescriptor));
}
