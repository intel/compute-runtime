/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/helpers/hw_helper.h"
#include "test.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device.h"

using namespace NEO;

struct ComputeModeRequirements : public ::testing::Test {
    template <typename FamilyType>
    struct myCsr : public CommandStreamReceiverHw<FamilyType> {
        using CommandStreamReceiver::commandStream;
        using CommandStreamReceiverHw<FamilyType>::requiredThreadArbitrationPolicy;
        using CommandStreamReceiverHw<FamilyType>::lastSentThreadArbitrationPolicy;
        myCsr(ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<FamilyType>(executionEnvironment, 0){};
        CsrSizeRequestFlags *getCsrRequestFlags() { return &this->csrSizeRequestFlags; }
    };
    void makeResidentSharedAlloc() {
        csr->getResidencyAllocations().push_back(alloc);
    }

    template <typename FamilyType>
    void overrideComputeModeRequest(bool reqestChanged, bool requireCoherency, bool hasSharedHandles) {
        overrideComputeModeRequest<FamilyType>(reqestChanged, requireCoherency, hasSharedHandles, false, 128u, false);
    }

    template <typename FamilyType>
    void overrideComputeModeRequest(bool reqestChanged, bool requireCoherency, bool hasSharedHandles, bool modifyThreadArbitrationPolicy) {
        overrideComputeModeRequest<FamilyType>(reqestChanged, requireCoherency, hasSharedHandles, false, 128u, false);
        if (modifyThreadArbitrationPolicy) {
            getCsrHw<FamilyType>()->lastSentThreadArbitrationPolicy = getCsrHw<FamilyType>()->requiredThreadArbitrationPolicy;
        }
    }

    template <typename FamilyType>
    void overrideComputeModeRequest(bool coherencyRequestChanged,
                                    bool requireCoherency,
                                    bool hasSharedHandles,
                                    bool numGrfRequiredChanged,
                                    uint32_t numGrfRequired,
                                    bool multiEngineQueue) {
        auto csrHw = getCsrHw<FamilyType>();
        csrHw->getCsrRequestFlags()->coherencyRequestChanged = coherencyRequestChanged;
        csrHw->getCsrRequestFlags()->hasSharedHandles = hasSharedHandles;
        csrHw->getCsrRequestFlags()->numGrfRequiredChanged = numGrfRequiredChanged;
        flags.requiresCoherency = requireCoherency;
        flags.numGrfRequired = numGrfRequired;
        flags.multiEngineQueue = multiEngineQueue;
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
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        csr = new myCsr<FamilyType>(*device->executionEnvironment);
        device->resetCommandStreamReceiver(csr);
        AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

        alloc = device->getMemoryManager()->createGraphicsAllocationFromSharedHandle((osHandle)123, properties, false);
    }

    void TearDown() override {
        device->getMemoryManager()->freeGraphicsMemory(alloc);
    }

    CommandStreamReceiver *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags{{}, nullptr, {}, nullptr, QueueThrottle::MEDIUM, PreemptionMode::Disabled, GrfConfig::DefaultGrfNumber, L3CachingSettings::l3CacheOn, QueueSliceCount::defaultSliceCount, false, false, false, false, false, false, false, false, false, false, false};
    GraphicsAllocation *alloc = nullptr;
};
