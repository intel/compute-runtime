/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

using namespace NEO;

class MyMockCommandStreamReceiver : public MockCommandStreamReceiver {
  public:
    using CommandStreamReceiver::scratchSpaceController;
    MyMockCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : MockCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {}
    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap *dsh,
        const IndirectHeap *ioh,
        const IndirectHeap *ssh,
        TaskCountType taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override {
        disableEuFusionPassed = dispatchFlags.disableEUFusion;
        return MockCommandStreamReceiver::flushTask(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    }
    bool disableEuFusionPassed = false;
};
template <typename GfxFamily>
class MockCmdQueueOverrideCsr : public MockCommandQueueHw<GfxFamily> {
  public:
    MockCmdQueueOverrideCsr(Context *context,
                            ClDevice *device,
                            MyMockCommandStreamReceiver *csr) : MockCommandQueueHw<GfxFamily>(context, device, nullptr) {
        this->csr = csr;
    }
    CommandStreamReceiver &getGpgpuCommandStreamReceiver() const override { return *csr; }
    MyMockCommandStreamReceiver *csr = nullptr;
};

DG2TEST_F(CommandQueueHwTest, GivenKernelWithDpasAndOddWorkGroupWhenenqueueNonBlockedCalledThenDisableEuFusionPassedToFlushTask) {
    auto hardwareInfo = *defaultHwInfo;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    std::unique_ptr<OsContext> osContext(OsContext::create(mockDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), mockDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    auto csr = std::make_unique<MyMockCommandStreamReceiver>(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr->setupContext(*osContext);
    auto scratchController = new ScratchSpaceControllerBase(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *csr->getInternalAllocationStorage());
    csr->scratchSpaceController.reset(scratchController);
    MockCmdQueueOverrideCsr<FamilyType> cmdQ(pContext, mockDevice.get(), csr.get());
    MockKernelWithInternals mockKernelWithInternals(*mockDevice.get());
    auto pKernel = mockKernelWithInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(mockDevice.get(), pKernel);
    BlitPropertiesContainer blitPropertiesContainer;
    const EnqueueProperties enqueueProperties(false, true, false, false, false, false, &blitPropertiesContainer);
    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    LinearStream commandStream;
    DispatchInfo &dispatchInfo = *multiDispatchInfo.begin();
    dispatchInfo.setLWS({3, 7, 1});
    dispatchInfo.setNumberOfWorkgroups({5, 1, 1});

    bool blocking = false;
    pKernel->setSystolicPipelineSelectMode(true);
    cmdQ.enqueueNonBlocked(nullptr, 0, commandStream, commandStream.getUsed(), blocking, true,
                           multiDispatchInfo, enqueueProperties, timestampPacketDependencies, eventsRequest, eventBuilder, 0, nullptr, false, CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_TRUE(csr->disableEuFusionPassed);
}

DG2TEST_F(CommandQueueHwTest, GivenKernelWithDpasAndNotOddWorkGroupWhenenqueueNonBlockedCalledThenDisableEuFusionNotPassedToFlushTask) {
    auto hardwareInfo = *defaultHwInfo;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    std::unique_ptr<OsContext> osContext(OsContext::create(mockDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), mockDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    auto csr = std::make_unique<MyMockCommandStreamReceiver>(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr->setupContext(*osContext);
    auto scratchController = new ScratchSpaceControllerBase(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *csr->getInternalAllocationStorage());
    csr->scratchSpaceController.reset(scratchController);
    MockCmdQueueOverrideCsr<FamilyType> cmdQ(pContext, mockDevice.get(), csr.get());
    MockKernelWithInternals mockKernelWithInternals(*mockDevice.get());
    auto pKernel = mockKernelWithInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(mockDevice.get(), pKernel);
    BlitPropertiesContainer blitPropertiesContainer;
    const EnqueueProperties enqueueProperties(false, true, false, false, false, false, &blitPropertiesContainer);
    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    LinearStream commandStream;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).payloadMappings.dispatchTraits.localWorkSize[0] = 0;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).payloadMappings.dispatchTraits.localWorkSize[1] = 4;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).payloadMappings.dispatchTraits.localWorkSize[2] = 8;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).payloadMappings.dispatchTraits.numWorkGroups[0] = 12;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).payloadMappings.dispatchTraits.numWorkGroups[1] = 16;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).payloadMappings.dispatchTraits.numWorkGroups[2] = 20;

    pKernel->setLocalWorkSizeValues(4, 7, 1);
    pKernel->setNumWorkGroupsValues(5, 1, 1);

    bool blocking = false;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    cmdQ.enqueueNonBlocked(nullptr, 0, commandStream, commandStream.getUsed(), blocking, true,
                           multiDispatchInfo, enqueueProperties, timestampPacketDependencies, eventsRequest, eventBuilder, 0, nullptr, false, CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_FALSE(csr->disableEuFusionPassed);
}
DG2TEST_F(CommandQueueHwTest, GivenKernelWithRequiredDisableEuFusionWhenenqueueNonBlockedCalledThenDisableEuFusionPassedToFlushTask) {
    auto hardwareInfo = *defaultHwInfo;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    std::unique_ptr<OsContext> osContext(OsContext::create(mockDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), mockDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    auto csr = std::make_unique<MyMockCommandStreamReceiver>(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr->setupContext(*osContext);
    auto scratchController = new ScratchSpaceControllerBase(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *csr->getInternalAllocationStorage());
    csr->scratchSpaceController.reset(scratchController);
    MockCmdQueueOverrideCsr<FamilyType> cmdQ(pContext, mockDevice.get(), csr.get());
    MockKernelWithInternals mockKernelWithInternals(*mockDevice.get());
    auto pKernel = mockKernelWithInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(mockDevice.get(), pKernel);
    BlitPropertiesContainer blitPropertiesContainer;
    const EnqueueProperties enqueueProperties(false, true, false, false, false, false, &blitPropertiesContainer);
    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    LinearStream commandStream;

    bool blocking = false;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).kernelAttributes.flags.requiresDisabledEUFusion = true;
    cmdQ.enqueueNonBlocked(nullptr, 0, commandStream, commandStream.getUsed(), blocking, true,
                           multiDispatchInfo, enqueueProperties, timestampPacketDependencies, eventsRequest, eventBuilder, 0, nullptr, false, CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_TRUE(csr->disableEuFusionPassed);
}
DG2TEST_F(CommandQueueHwTest, GivenKernelWithoutRequiredDisableEuFusionWhenenqueueNonBlockedCalledThenDisableEuFusionNotPassedToFlushTask) {
    auto hardwareInfo = *defaultHwInfo;
    auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0));
    std::unique_ptr<OsContext> osContext(OsContext::create(mockDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), mockDevice->getRootDeviceIndex(), 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, mockDevice->getDeviceBitfield())));
    auto csr = std::make_unique<MyMockCommandStreamReceiver>(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr->setupContext(*osContext);
    auto scratchController = new ScratchSpaceControllerBase(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *csr->getInternalAllocationStorage());
    csr->scratchSpaceController.reset(scratchController);
    MockCmdQueueOverrideCsr<FamilyType> cmdQ(pContext, mockDevice.get(), csr.get());
    MockKernelWithInternals mockKernelWithInternals(*mockDevice.get());
    auto pKernel = mockKernelWithInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(mockDevice.get(), pKernel);
    BlitPropertiesContainer blitPropertiesContainer;
    const EnqueueProperties enqueueProperties(false, true, false, false, false, false, &blitPropertiesContainer);
    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    LinearStream commandStream;

    bool blocking = false;
    const_cast<NEO::KernelDescriptor &>(pKernel->getDescriptor()).kernelAttributes.flags.requiresDisabledEUFusion = false;
    cmdQ.enqueueNonBlocked(nullptr, 0, commandStream, commandStream.getUsed(), blocking, true, multiDispatchInfo,
                           enqueueProperties, timestampPacketDependencies, eventsRequest, eventBuilder, 0, nullptr, false, CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_FALSE(csr->disableEuFusionPassed);
}