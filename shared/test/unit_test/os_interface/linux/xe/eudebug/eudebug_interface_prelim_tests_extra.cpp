/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface_prelim.h"
#include "shared/source/os_interface/linux/xe/xedrm_prelim.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(EuDebugInterfacePrelimTest, whenCheckingIfisExecQueuePageFaultEnableSupportedThenCorrectValueIsReturned) {
    EuDebugInterfacePrelim euDebugInterface{};
    EXPECT_FALSE(euDebugInterface.isExecQueuePageFaultEnableSupported());
}
