/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

namespace NEO {

class ExecutionEnvironment;
struct UltHwConfig {
    const char *aubTestName = nullptr;
    ExecutionEnvironment *sourceExecutionEnvironment = nullptr;

    bool mockedPrepareDeviceEnvironmentsFuncResult = true;
    bool useHwCsr = false;
    bool useMockedPrepareDeviceEnvironmentsFunc = true;
    bool forceOsAgnosticMemoryManager = true;
    bool useinitBuiltinsAsyncEnabled = false;
    bool useGemCloseWorker = false;
    bool useWaitForTimestamps = false;
    bool useBlitSplit = false;
    bool useFirstSubmissionInitDevice = false;

    bool csrFailInitDirectSubmission = false;
    bool csrBaseCallDirectSubmissionAvailable = false;
    bool csrSuperBaseCallDirectSubmissionAvailable = false;

    bool csrBaseCallBlitterDirectSubmissionAvailable = false;
    bool csrSuperBaseCallBlitterDirectSubmissionAvailable = false;

    bool csrBaseCallCreatePreemption = true;
    bool csrCreatePreemptionReturnValue = true;
};

extern UltHwConfig ultHwConfig;
} // namespace NEO
