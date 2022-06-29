/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct ComputeModeRequirements : public ::testing::Test {
    template <typename FamilyType>
    struct myCsr : public UltCommandStreamReceiver<FamilyType> {
        using CommandStreamReceiver::commandStream;
        using CommandStreamReceiver::streamProperties;
        myCsr(ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
            : UltCommandStreamReceiver<FamilyType>(executionEnvironment, 0, deviceBitfield){};
        CsrSizeRequestFlags *getCsrRequestFlags() { return &this->csrSizeRequestFlags; }
        bool hasSharedHandles() override {
            if (hasSharedHandlesReturnValue) {
                return *hasSharedHandlesReturnValue;
            }
            return UltCommandStreamReceiver<FamilyType>::hasSharedHandles();
        };
        std::optional<bool> hasSharedHandlesReturnValue;
    };
    void makeResidentSharedAlloc() {
        csr->getResidencyAllocations().push_back(alloc);
    }

    template <typename FamilyType>
    void overrideComputeModeRequest(bool reqestChanged, bool requireCoherency, bool hasSharedHandles,
                                    bool modifyThreadArbitrationPolicy = false, bool numGrfRequiredChanged = false,
                                    uint32_t numGrfRequired = 128u) {
        overrideComputeModeRequest<FamilyType>(reqestChanged, requireCoherency, hasSharedHandles, numGrfRequiredChanged, numGrfRequired);
        if (modifyThreadArbitrationPolicy) {
            auto &hwHelper = NEO::HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);
            auto csrHw = getCsrHw<FamilyType>();
            csrHw->streamProperties.stateComputeMode.threadArbitrationPolicy.value = hwHelper.getDefaultThreadArbitrationPolicy();
            csrHw->streamProperties.stateComputeMode.threadArbitrationPolicy.isDirty = true;
        }
    }

    template <typename FamilyType>
    void overrideComputeModeRequest(bool coherencyRequestChanged,
                                    bool requireCoherency,
                                    bool hasSharedHandles,
                                    bool numGrfRequiredChanged,
                                    uint32_t numGrfRequired) {
        auto csrHw = getCsrHw<FamilyType>();
        csrHw->hasSharedHandlesReturnValue = hasSharedHandles;
        flags.requiresCoherency = requireCoherency;
        csrHw->streamProperties.stateComputeMode.isCoherencyRequired.value = requireCoherency;
        csrHw->streamProperties.stateComputeMode.isCoherencyRequired.isDirty = coherencyRequestChanged;
        csrHw->streamProperties.stateComputeMode.largeGrfMode.value = (numGrfRequired == GrfConfig::LargeGrfNumber);
        csrHw->streamProperties.stateComputeMode.largeGrfMode.isDirty = numGrfRequiredChanged;
        if (hasSharedHandles) {
            makeResidentSharedAlloc();
        }
    }

    template <typename FamilyType>
    myCsr<FamilyType> *getCsrHw() {
        return static_cast<myCsr<FamilyType> *>(csr);
    }

    template <typename FamilyType>
    void setUpImpl() {
        setUpImpl<FamilyType>(defaultHwInfo.get());
    }

    template <typename FamilyType>
    void setUpImpl(const NEO::HardwareInfo *hardwareInfo) {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo));
        device->executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(hardwareInfo);
        device->executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        csr = new myCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());

        device->resetCommandStreamReceiver(csr);
        AllocationProperties properties(device->getRootDeviceIndex(), false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});

        alloc = device->getMemoryManager()->createGraphicsAllocationFromSharedHandle(static_cast<osHandle>(123), properties, false, false);
    }

    void TearDown() override {
        device->getMemoryManager()->freeGraphicsMemory(alloc);
    }

    CommandStreamReceiver *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags{{}, nullptr, {}, nullptr, QueueThrottle::MEDIUM, PreemptionMode::Disabled, GrfConfig::DefaultGrfNumber, L3CachingSettings::l3CacheOn, ThreadArbitrationPolicy::NotPresent, AdditionalKernelExecInfo::NotApplicable, KernelExecutionType::NotApplicable, MemoryCompressionState::NotApplicable, QueueSliceCount::defaultSliceCount, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
    GraphicsAllocation *alloc = nullptr;
};
