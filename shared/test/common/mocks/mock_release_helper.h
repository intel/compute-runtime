/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {
class MockReleaseHelper : public ReleaseHelper {
  public:
    MockReleaseHelper() : ReleaseHelper(0) {}
    ADDMETHOD_CONST_NOBASE(isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired, bool, false, (const HardwareInfo &hwInfo, bool isRcs));
    ADDMETHOD_CONST_NOBASE(isResolvingSubDeviceIDNeeded, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getSupportedNumGrfs, const SupportedNumGrfs, {128u}, ());
    ADDMETHOD_CONST_NOBASE(getTotalMemBankSize, uint64_t, 32ull * MemoryConstants::gigaByte, ());
    ADDMETHOD_CONST_NOBASE(getThreadsPerEUConfigs, const ThreadsPerEUConfigs, {}, (uint32_t numThreadsPerEu));
    ADDMETHOD_CONST_NOBASE(getStackSizePerRay, uint32_t, {}, ());
    ADDMETHOD_CONST_NOBASE(computeSlmValues, uint32_t, {}, (uint32_t slmSize));
    ADDMETHOD_CONST_NOBASE(alignSlmSizePerThreadGroup, uint32_t, {}, (uint32_t slmSize));
    ADDMETHOD_CONST_NOBASE(adjustMaxThreadsPerEuCount, uint32_t, 8u, (uint32_t maxThreadsPerEuCount, uint32_t grfCount));
    ADDMETHOD_CONST_NOBASE(isStateCacheInvalidationWaRequired, bool, false, (bool isImmediateCmdList, bool kernelUsesImageOrSampler));
    ADDMETHOD_CONST_NOBASE(getIpVersionForGmm, uint32_t, 0, ());
    ADDMETHOD_CONST_NOBASE(overrideSystemMemoryPatIndexBase, uint64_t, 0, (uint64_t patIndex));

    const SizeToPreferredSlmValueArray &getSizeToPreferredSlmValue() const override {
        static SizeToPreferredSlmValueArray sizeToPreferredSlmValue = {};
        return sizeToPreferredSlmValue;
    }
};
} // namespace NEO
