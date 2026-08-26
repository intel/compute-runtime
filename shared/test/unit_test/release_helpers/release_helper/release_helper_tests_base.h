/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_ip_version.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {
class ReleaseHelper;
}

using namespace NEO;

struct ReleaseHelperTestsBase : public ::testing::Test {

    ReleaseHelperTestsBase();
    ~ReleaseHelperTestsBase() override;
    void whenGettingSupportedNumGrfsThenValue128Returned();
    void whenGettingSupportedNumGrfsThenValues128And256Returned();
    void whenGettingThreadsPerEuConfigsThen4And8AreReturned();
    void whenGettingTotalMemBankSizeThenReturn32GB();
    void whenGettingPreferredSlmSizeThenAllEntriesEmpty();
    void whenGettingSupportedNumGrfsThenValuesUpTo256Returned();
    void whenGettingThreadsPerEuConfigsThenCorrectValueIsReturnedBasedOnNumThreadPerEu();
    void whenCallingAdjustMaxThreadsPerEuCountThenCorrectValueIsReturned();
    void whenShouldQueryPeerAccessCalledThenFalseReturned();
    void whenShouldQueryPeerAccessCalledThenTrueReturned();
    void whenIsSingleDispatchRequiredForMultiCCSCalledThenFalseReturned();
    void whenIsSingleDispatchRequiredForMultiCCSCalledThenTrueReturned();
    void whenIsStateCacheInvalidationWaRequiredCalledThenFalseReturned();
    void whenIsStateCacheInvalidationWaRequiredCalledThenTrueReturned();
    void whenIsStateCacheInvalidationWaRequiredCalledThenTrueOnlyForImmediateAndImageOrSampler();
    void whenIsStateCacheInvalidationWaRequiredCalledWithDebugFlagSetThenCorrectValueReturned();
    void whenGettingSupportedNumGrfsThenValuesUpTo512Returned();
    virtual std::vector<uint32_t> getRevisions() = 0;

    std::unique_ptr<ReleaseHelper> releaseHelper;
    HardwareIpVersion ipVersion{};
};

template <uint32_t architecture, uint32_t release>
struct ReleaseHelperTests : public ReleaseHelperTestsBase {
    void SetUp() override {
        ipVersion.architecture = architecture;
        ipVersion.release = release;
    }
};
