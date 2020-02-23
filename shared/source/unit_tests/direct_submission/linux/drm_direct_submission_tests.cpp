/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

#include <memory>

struct DrmDirectSubmissionFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();

        osContext = std::make_unique<OsContextLinux>(drmMock, 0u, 0u, aub_stream::ENGINE_RCS, PreemptionMode::ThreadGroup, false);
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<OsContextLinux> osContext;
    DrmMock drmMock;
};

template <typename GfxFamily>
struct MockDrmDirectSubmission : public DrmDirectSubmission<GfxFamily> {
    MockDrmDirectSubmission(Device &device,
                            std::unique_ptr<Dispatcher> cmdDispatcher,
                            OsContext &osContext)
        : DrmDirectSubmission<GfxFamily>(device, std::move(cmdDispatcher), osContext) {
    }
    using BaseClass = DrmDirectSubmission<GfxFamily>;
    using BaseClass::allocateOsResources;
    using BaseClass::getTagAddressValue;
    using BaseClass::handleResidency;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::updateTagValue;
};

using DrmDirectSubmissionTest = Test<DrmDirectSubmissionFixture>;

using namespace NEO;

HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenCallingLinuxImplementationThenExpectAllFailAsNotImplemented) {
    MockDrmDirectSubmission<FamilyType> drmDirectSubmission(*pDevice,
                                                            std::make_unique<RenderDispatcher<FamilyType>>(),
                                                            *osContext.get());

    DirectSubmissionAllocations allocations;
    EXPECT_FALSE(drmDirectSubmission.allocateOsResources(allocations));

    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    EXPECT_FALSE(drmDirectSubmission.submit(gpuAddress, size));

    EXPECT_FALSE(drmDirectSubmission.handleResidency());

    EXPECT_EQ(0ull, drmDirectSubmission.switchRingBuffers());

    EXPECT_EQ(0ull, drmDirectSubmission.updateTagValue());

    TagData tagData = {1ull, 1ull};
    drmDirectSubmission.getTagAddressValue(tagData);
    EXPECT_EQ(0ull, tagData.tagAddress);
    EXPECT_EQ(0ull, tagData.tagValue);
}
