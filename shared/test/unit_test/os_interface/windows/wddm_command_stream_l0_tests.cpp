/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/windows/wddm_direct_submission.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm_device_command_stream.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"
#include "shared/test/common/mocks/mock_wddm_interface23.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/windows/mock_wddm_direct_submission.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} // namespace NEO
using namespace NEO;

template <typename GfxFamily>
struct MockWddmCsrL0 : public WddmCommandStreamReceiver<GfxFamily> {
    using CommandStreamReceiver::clearColorAllocation;
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::dispatchMode;
    using CommandStreamReceiver::getCS;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;
    using CommandStreamReceiverHw<GfxFamily>::blitterDirectSubmission;
    using CommandStreamReceiverHw<GfxFamily>::directSubmission;
    using WddmCommandStreamReceiver<GfxFamily>::commandBufferHeader;
    using WddmCommandStreamReceiver<GfxFamily>::initDirectSubmission;
    using WddmCommandStreamReceiver<GfxFamily>::WddmCommandStreamReceiver;

    void overrideDispatchPolicy(DispatchMode overrideValue) {
        this->dispatchMode = overrideValue;
    }

    SubmissionAggregator *peekSubmissionAggregator() {
        return this->submissionAggregator.get();
    }

    void overrideSubmissionAggregator(SubmissionAggregator *newSubmissionsAggregator) {
        this->submissionAggregator.reset(newSubmissionsAggregator);
    }

    void overrideRecorededCommandBuffer(Device &device) {
        recordedCommandBuffer = std::unique_ptr<CommandBuffer>(new CommandBuffer(device));
    }

    bool initDirectSubmission() override {
        if (callParentInitDirectSubmission) {
            return WddmCommandStreamReceiver<GfxFamily>::initDirectSubmission();
        }
        bool ret = true;
        if (debugManager.flags.EnableDirectSubmission.get() == 1) {
            if (!initBlitterDirectSubmission) {
                directSubmission = std::make_unique<
                    MockWddmDirectSubmission<GfxFamily, RenderDispatcher<GfxFamily>>>(*this);
                ret = directSubmission->initialize(true);
                this->dispatchMode = DispatchMode::immediateDispatch;
            } else {
                blitterDirectSubmission = std::make_unique<
                    MockWddmDirectSubmission<GfxFamily, BlitterDispatcher<GfxFamily>>>(*this);
                blitterDirectSubmission->initialize(true);
            }
        }
        return ret;
    }

    uint32_t flushCalledCount = 0;
    std::unique_ptr<CommandBuffer> recordedCommandBuffer = nullptr;

    bool callParentInitDirectSubmission = true;
    bool initBlitterDirectSubmission = false;
};

using WddmSimpleTestL0 = ::testing::Test;
HWTEST_F(WddmSimpleTestL0, givenL0ApiAndDefaultWddmCsrWhenItIsCreatedThenImmediateDispatchIsTurnedOn) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment *executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    std::unique_ptr<MockDevice> device(Device::create<MockDevice>(executionEnvironment, 0u));
    {
        std::unique_ptr<MockWddmCsrL0<FamilyType>> mockCsr(new MockWddmCsrL0<FamilyType>(*executionEnvironment, 0, 1));
        EXPECT_EQ(DispatchMode::immediateDispatch, mockCsr->dispatchMode);
    }
}