/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} //namespace NEO
using namespace NEO;

template <typename GfxFamily>
struct MockDrmCsr : public DrmCommandStreamReceiver<GfxFamily> {
    using DrmCommandStreamReceiver<GfxFamily>::DrmCommandStreamReceiver;
    using DrmCommandStreamReceiver<GfxFamily>::dispatchMode;
};

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenL0ApiConfigWhenCreatingDrmCsrThenEnableImmediateDispatch) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    MockDrmCsr<FamilyType> csr(executionEnvironment, 0, 1, gemCloseWorkerMode::gemCloseWorkerInactive);
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, whenGettingCompletionValueThenTaskCountOfAllocationIsReturned) {
    MockGraphicsAllocation allocation{};
    uint32_t expectedValue = 0x1234;
    allocation.updateTaskCount(expectedValue, osContext->getContextId());
    EXPECT_EQ(expectedValue, csr->getCompletionValue(allocation));
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, whenGettingCompletionAddressThenOffsettedTagAddressIsReturned) {
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAddress());
    uint64_t tagAddress = castToUint64(const_cast<uint32_t *>(csr->getTagAddress()));
    auto expectedAddress = tagAddress + Drm::completionFenceOffset;
    EXPECT_EQ(expectedAddress, csr->getCompletionAddress());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenNoTagAddressWhenGettingCompletionAddressThenZeroIsReturned) {
    EXPECT_EQ(nullptr, csr->getTagAddress());
    EXPECT_EQ(0u, csr->getCompletionAddress());
}
