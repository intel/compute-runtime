/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include <gtest/gtest.h>

template <typename GfxFamily>
class SourceLevelDebuggerPreambleTest {
  public:
    SourceLevelDebuggerPreambleTest() {}

    static void givenMidThreadPreemptionAndDebuggingActiveWhenStateSipIsProgrammedThenCorrectSipKernelIsUsedTest();
    static void givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsProgrammedThenCorrectSipKernelIsUsedTest();
    static void givenPreemptionDisabledAndDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelIsUsedTest();
    static void givenMidThreadPreemptionAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
    static void givenPreemptionDisabledAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
    static void givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
    static void givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
    static void givenKernelDebuggingActiveAndDisabledPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAreInlcudedTest();
};
