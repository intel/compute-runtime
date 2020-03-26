/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
namespace NEO {
struct UltHwConfig {
    bool mockedPrepareDeviceEnvironmentsFuncResult = true;
    bool useHwCsr = false;
    bool useMockedPrepareDeviceEnvironmentsFunc = true;
    bool forceOsAgnosticMemoryManager = true;

    bool csrFailInitDirectSubmission = false;
    bool csrBaseCallDirectSubmissionAvailable = false;
    bool csrSuperBaseCallDirectSubmissionAvailable = false;

    bool csrBaseCallBlitterDirectSubmissionAvailable = false;
    bool csrSuperBaseCallBlitterDirectSubmissionAvailable = false;
};

extern UltHwConfig ultHwConfig;
} // namespace NEO
