/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"

#include "gtest/gtest.h"

namespace NEO {

extern GMM_INIT_IN_ARGS passedInputArgs;
extern bool copyInputArgs;

TEST(GmmHelperTest, whenCreateGmmHelperWithoutOsInterfaceThenPassedAdapterBDFIsZeroed) {
    std::unique_ptr<GmmHelper> gmmHelper;
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    ADAPTER_BDF expectedAdapterBDF{};

    gmmHelper.reset(new GmmHelper(nullptr, platformDevices[0]));
    EXPECT_EQ(0, memcmp(&expectedAdapterBDF, &passedInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
}

} // namespace NEO
