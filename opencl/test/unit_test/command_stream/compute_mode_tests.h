/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "test.h"

using namespace NEO;

struct ComputeModeRequirements : public ::testing::Test {
    template <typename FamilyType>
    struct myCsr : public UltCommandStreamReceiver<FamilyType> {
        using CommandStreamReceiver::commandStream;
        using CommandStreamReceiverHw<FamilyType>::lastSentThreadArbitrationPolicy;
        using CommandStreamReceiverHw<FamilyType>::requiredThreadArbitrationPolicy;
        myCsr(ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
            : UltCommandStreamReceiver<FamilyType>(executionEnvironment, 0, deviceBitfield){};
        CsrSizeRequestFlags *getCsrRequestFlags() { return &this->csrSizeRequestFlags; }
    };
    void makeResidentSharedAlloc() {
        csr->getResidencyAllocations().push_back(alloc);
    }

    template <typename FamilyType>
    void overrideComputeModeRequest(bool reqestChanged, bool requireCoherency, bool hasSharedHandles, bool modifyThreadArbitrationPolicy = false) {
        overrideComputeModeRequest<FamilyType>(reqestChanged, requireCoherency, hasSharedHandles, false, 128u);
        if (modifyThreadArbitrationPolicy) {
            getCsrHw<FamilyType>()->lastSentThreadArbitrationPolicy = getCsrHw<FamilyType>()->requiredThreadArbitrationPolicy;
        }
    }

    template <typename FamilyType>
    void overrideComputeModeRequest(bool coherencyRequestChanged,
                                    bool requireCoherency,
                                    bool hasSharedHandles,
                                    bool numGrfRequiredChanged,
                                    uint32_t numGrfRequired) {
        auto csrHw = getCsrHw<FamilyType>();
        csrHw->getCsrRequestFlags()->coherencyRequestChanged = coherencyRequestChanged;
        csrHw->getCsrRequestFlags()->hasSharedHandles = hasSharedHandles;
        csrHw->getCsrRequestFlags()->numGrfRequiredChanged = numGrfRequiredChanged;
        flags.requiresCoherency = requireCoherency;
        flags.numGrfRequired = numGrfRequired;
        if (hasSharedHandles) {
            makeResidentSharedAlloc();
        }
    }

    template <typename FamilyType>
    myCsr<FamilyType> *getCsrHw() {
        return static_cast<myCsr<FamilyType> *>(csr);
    }

    template <typename FamilyType>
    void SetUpImpl() {
        SetUpImpl<FamilyType>(defaultHwInfo.get());
    }

    template <typename FamilyType>
    void SetUpImpl(const NEO::HardwareInfo *hardwareInfo) {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo));
        csr = new myCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());
        device->resetCommandStreamReceiver(csr);
        AllocationProperties properties(device->getRootDeviceIndex(), false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, {});

        alloc = device->getMemoryManager()->createGraphicsAllocationFromSharedHandle((osHandle)123, properties, false, false);
    }

    void TearDown() override {
        device->getMemoryManager()->freeGraphicsMemory(alloc);
    }

    CommandStreamReceiver *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags{{}, nullptr, {}, nullptr, QueueThrottle::MEDIUM, PreemptionMode::Disabled, GrfConfig::DefaultGrfNumber, L3CachingSettings::l3CacheOn, ThreadArbitrationPolicy::NotPresent, AdditionalKernelExecInfo::NotApplicable, KernelExecutionType::NotApplicable, MemoryCompressionState::NotApplicable, QueueSliceCount::defaultSliceCount, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
    GraphicsAllocation *alloc = nullptr;
};
