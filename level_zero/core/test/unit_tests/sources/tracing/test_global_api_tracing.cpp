/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingInitTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.Global.pfnInit = [](ze_init_flags_t flags) { return ZE_RESULT_SUCCESS; };

    prologCbs.Global.pfnInitCb = genericPrologCallbackPtr;
    epilogCbs.Global.pfnInitCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeInitTracing(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
