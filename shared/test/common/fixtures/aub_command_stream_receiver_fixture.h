/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_device.h"

namespace NEO {
struct AubCommandStreamReceiverFixture : public DeviceFixture, MockAubCenterFixture {
    bool useAubManager = true;

    virtual void setUp(bool createAubManager) {
        DeviceFixture::setUp();
        MockAubCenterFixture::setUp();
        setMockAubCenter(*pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    }

    virtual void setUp() {
        setUp(true);
    }

    virtual void tearDown() {
        MockAubCenterFixture::tearDown();
        DeviceFixture::tearDown();
    }

    template <typename CsrType>
    std::unique_ptr<AubExecutionEnvironment> getEnvironment(bool createTagAllocation, bool allocateCommandBuffer, bool standalone) {
        std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
        DeviceBitfield deviceBitfield(1);
        executionEnvironment->prepareRootDeviceEnvironments(1);
        uint32_t rootDeviceIndex = 0u;
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();

        setMockAubCenter(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex], CommandStreamReceiverType::aub, this->useAubManager);

        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setDummyBlitProperties(rootDeviceIndex);
        executionEnvironment->initializeMemoryManager();

        auto commandStreamReceiver = std::make_unique<CsrType>("", standalone, *executionEnvironment, rootDeviceIndex, deviceBitfield);
        auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                         EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::regular},
                                                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
        commandStreamReceiver->setupContext(*osContext);

        if (createTagAllocation) {
            commandStreamReceiver->initializeTagAllocation();
        }
        commandStreamReceiver->createGlobalFenceAllocation();

        std::unique_ptr<AubExecutionEnvironment> aubExecutionEnvironment(new AubExecutionEnvironment);
        if (allocateCommandBuffer) {
            aubExecutionEnvironment->commandBuffer = executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
        }
        aubExecutionEnvironment->executionEnvironment = std::move(executionEnvironment);
        aubExecutionEnvironment->commandStreamReceiver = std::move(commandStreamReceiver);
        return aubExecutionEnvironment;
    }
}; // namespace NEO

struct AubCommandStreamReceiverWithoutAubStreamFixture : public AubCommandStreamReceiverFixture {
    DebugManagerStateRestore stateRestore;

    void setUp() override {
        debugManager.flags.UseAubStream.set(false);
        AubCommandStreamReceiverFixture::setUp(false);
    }

    void tearDown() override {
        AubCommandStreamReceiverFixture::tearDown();
    }
};
} // namespace NEO
