/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/command_stream/preemption.h"
#include "core/helpers/preamble.h"
#include "runtime/built_ins/built_ins.h"
#include "test.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

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
