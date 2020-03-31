/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

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
