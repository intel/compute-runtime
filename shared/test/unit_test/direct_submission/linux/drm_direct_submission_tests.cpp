/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"
#include "shared/test/unit_test/helpers/variable_backup.h"
#include "shared/test/unit_test/mocks/mock_device.h"

#include "opencl/test/unit_test/os_interface/linux/drm_memory_manager_tests.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

#include <memory>

struct DrmDirectSubmissionTest : public DrmMemoryManagerBasic {
    void SetUp() override {
        backupUlt = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);

        DrmMemoryManagerBasic::SetUp();
        executionEnvironment.incRefInternal();

        ultHwConfig.forceOsAgnosticMemoryManager = false;
        device.reset(MockDevice::create<MockDevice>(&executionEnvironment, 0u));
        osContext = std::make_unique<OsContextLinux>(*executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->getDrm(),
                                                     0u, device->getDeviceBitfield(), aub_stream::ENGINE_RCS, PreemptionMode::ThreadGroup,
                                                     false, false, false);
    }

    void TearDown() override {
        DrmMemoryManagerBasic::TearDown();
    }

    std::unique_ptr<OsContextLinux> osContext;
    std::unique_ptr<MockDevice> device;

    std::unique_ptr<VariableBackup<UltHwConfig>> backupUlt;
};

template <typename GfxFamily, typename Dispatcher>
struct MockDrmDirectSubmission : public DrmDirectSubmission<GfxFamily, Dispatcher> {
    using BaseClass = DrmDirectSubmission<GfxFamily, Dispatcher>;
    using BaseClass::allocateResources;
    using BaseClass::currentTagData;
    using BaseClass::DrmDirectSubmission;
    using BaseClass::getTagAddressValue;
    using BaseClass::handleResidency;
    using BaseClass::submit;
    using BaseClass::switchRingBuffers;
    using BaseClass::tagAddress;
    using BaseClass::updateTagValue;
};

using namespace NEO;

HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenCallingLinuxImplementationThenExpectInitialImplementationValues) {
    MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>> drmDirectSubmission(*device.get(),
                                                                                          *osContext.get());

    EXPECT_TRUE(drmDirectSubmission.allocateResources());

    uint64_t gpuAddress = 0x1000;
    size_t size = 0x1000;
    EXPECT_TRUE(drmDirectSubmission.submit(gpuAddress, size));

    EXPECT_TRUE(drmDirectSubmission.handleResidency());

    EXPECT_NE(0ull, drmDirectSubmission.switchRingBuffers());

    EXPECT_EQ(0ull, drmDirectSubmission.updateTagValue());

    TagData tagData = {1ull, 1ull};
    drmDirectSubmission.getTagAddressValue(tagData);
    EXPECT_EQ(drmDirectSubmission.currentTagData.tagAddress, tagData.tagAddress);
    EXPECT_EQ(drmDirectSubmission.currentTagData.tagValue + 1, tagData.tagValue);
}

HWTEST_F(DrmDirectSubmissionTest, whenCreateDirectSubmissionThenValidObjectIsReturned) {
    auto directSubmission = DirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>::create(*device.get(),
                                                                                                 *osContext.get());
    EXPECT_NE(directSubmission.get(), nullptr);
}

HWTEST_F(DrmDirectSubmissionTest, givenDrmDirectSubmissionWhenDestructObjectThenIoctlIsCalled) {
    auto drmDirectSubmission = std::make_unique<MockDrmDirectSubmission<FamilyType, RenderDispatcher<FamilyType>>>(*device.get(),
                                                                                                                   *osContext.get());
    auto drm = static_cast<DrmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->getDrm());
    drm->ioctlCallsCount = 0u;
    drmDirectSubmission->initialize(true);
    drmDirectSubmission.reset();

    EXPECT_EQ(drm->ioctlCallsCount, 11u);
}