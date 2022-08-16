/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

class CommandEncodeStatesFixture : public DeviceFixture {
  public:
    class MyMockCommandContainer : public CommandContainer {
      public:
        using CommandContainer::dirtyHeaps;
    };

    void setUp() {
        DeviceFixture::setUp();
        cmdContainer = std::make_unique<MyMockCommandContainer>();
        cmdContainer->initialize(pDevice, nullptr, true);
        cmdContainer->setDirtyStateForAllHeaps(false);
    }
    void tearDown() {
        cmdContainer.reset();
        DeviceFixture::tearDown();
    }
    std::unique_ptr<MyMockCommandContainer> cmdContainer;
    KernelDescriptor descriptor;

    EncodeDispatchKernelArgs createDefaultDispatchKernelArgs(Device *device,
                                                             DispatchKernelEncoderI *dispatchInterface,
                                                             const void *threadGroupDimensions,
                                                             bool requiresUncachedMocs) {
        EncodeDispatchKernelArgs args{
            0,                        // eventAddress
            device,                   // device
            dispatchInterface,        // dispatchInterface
            threadGroupDimensions,    // threadGroupDimensions
            PreemptionMode::Disabled, // preemptionMode
            1,                        // partitionCount
            false,                    // isIndirect
            false,                    // isPredicate
            false,                    // isTimestampEvent
            requiresUncachedMocs,     // requiresUncachedMocs
            false,                    // useGlobalAtomics
            false,                    // isInternal
            false,                    // isCooperative
            false,                    // isHostScopeSignalEvent
            false,                    // isKernelUsingSystemAllocation
            false                     // isKernelDispatchedFromImmediateCmdList
        };

        return args;
    }
};

} // namespace NEO

struct WalkerThreadFixture {
    void setUp() {
        startWorkGroup[0] = startWorkGroup[1] = startWorkGroup[2] = 0u;
        numWorkGroups[0] = numWorkGroups[1] = numWorkGroups[2] = 1u;
        workGroupSizes[0] = 32u;
        workGroupSizes[1] = workGroupSizes[2] = 1u;
        simd = 32u;
        localIdDimensions = 3u;
        requiredWorkGroupOrder = 0u;
    }
    void tearDown() {}

    uint32_t startWorkGroup[3];
    uint32_t numWorkGroups[3];
    uint32_t workGroupSizes[3];
    uint32_t simd;
    uint32_t localIdDimensions;
    uint32_t requiredWorkGroupOrder;
};

using WalkerThreadTest = Test<WalkerThreadFixture>;
