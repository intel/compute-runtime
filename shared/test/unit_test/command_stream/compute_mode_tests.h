/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"

using namespace NEO;

struct ComputeModeRequirements : public ::testing::Test {
    template <typename FamilyType>
    struct MyCsr : public UltCommandStreamReceiver<FamilyType> {
        using CommandStreamReceiver::commandStream;
        using CommandStreamReceiver::streamProperties;
        MyCsr(ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
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
            auto &gfxCoreHelper = device->getGfxCoreHelper();
            auto csrHw = getCsrHw<FamilyType>();
            csrHw->streamProperties.stateComputeMode.threadArbitrationPolicy.value = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
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
        csrHw->streamProperties.stateComputeMode.isCoherencyRequired.value = requireCoherency;
        csrHw->streamProperties.stateComputeMode.isCoherencyRequired.isDirty = coherencyRequestChanged;
        csrHw->streamProperties.stateComputeMode.largeGrfMode.value = (numGrfRequired == GrfConfig::largeGrfNumber);
        csrHw->streamProperties.stateComputeMode.largeGrfMode.isDirty = numGrfRequiredChanged;
        if (hasSharedHandles) {
            makeResidentSharedAlloc();
        }
    }

    template <typename FamilyType>
    MyCsr<FamilyType> *getCsrHw() {
        return static_cast<MyCsr<FamilyType> *>(csr);
    }

    template <typename FamilyType>
    void setUpImpl() {
        setUpImpl<FamilyType>(defaultHwInfo.get());
    }

    template <typename FamilyType>
    void setUpImpl(const NEO::HardwareInfo *hardwareInfo) {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo));
        device->executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(hardwareInfo);
        device->executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        csr = new MyCsr<FamilyType>(*device->executionEnvironment, device->getDeviceBitfield());

        device->resetCommandStreamReceiver(csr);
        AllocationProperties properties(device->getRootDeviceIndex(), false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});

        MemoryManager::OsHandleData osHandleData{123u};
        alloc = device->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    }

    void TearDown() override {
        device->getMemoryManager()->freeGraphicsMemory(alloc);
    }

    CommandStreamReceiver *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags{nullptr, {}, nullptr, QueueThrottle::MEDIUM, PreemptionMode::Disabled, GrfConfig::defaultGrfNumber, L3CachingSettings::l3CacheOn, ThreadArbitrationPolicy::NotPresent, AdditionalKernelExecInfo::notApplicable, KernelExecutionType::notApplicable, MemoryCompressionState::notApplicable, QueueSliceCount::defaultSliceCount, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
    GraphicsAllocation *alloc = nullptr;
};
