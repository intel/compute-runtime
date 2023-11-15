/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
    void whenGettingMediaFrequencyTileIndexThenFalseIsReturned();
    void whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned();
    void whenGettingMaxPreferredSlmSizeThenSizeIsNotModified();
    void whenGettingMediaFrequencyTileIndexThenOneIsReturned();
    void whenCheckPreferredAllocationMethodThenAllocateByKmdIsReturnedExceptTagBufferAndTimestampPacketTagBuffer();
    void whenShouldAdjustCalledThenTrueReturned();
    void whenShouldAdjustCalledThenFalseReturned();
    void whenGettingSupportedNumGrfsThenValues128And256Returned();

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
