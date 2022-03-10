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
#include "shared/test/common/test_macros/test.h"

namespace NEO {

class CommandEncodeStatesFixture : public DeviceFixture {
  public:
    class MyMockCommandContainer : public CommandContainer {
      public:
        using CommandContainer::dirtyHeaps;
    };

    void SetUp() {
        DeviceFixture::SetUp();
        cmdContainer = std::make_unique<MyMockCommandContainer>();
        cmdContainer->initialize(pDevice, nullptr);
        cmdContainer->setDirtyStateForAllHeaps(false);
    }
    void TearDown() {
        cmdContainer.reset();
        DeviceFixture::TearDown();
    }
    std::unique_ptr<MyMockCommandContainer> cmdContainer;
    KernelDescriptor descriptor;

    EncodeDispatchKernelArgs createDefaultDispatchKernelArgs(Device *device,
                                                             DispatchKernelEncoderI *dispatchInterface,
                                                             const void *pThreadGroupDimensions,
                                                             bool requiresUncachedMocs) {
        EncodeDispatchKernelArgs args{
            0,                        //eventAddress
            device,                   //device
            dispatchInterface,        //dispatchInterface
            pThreadGroupDimensions,   //pThreadGroupDimensions
            PreemptionMode::Disabled, //preemptionMode
            1,                        //partitionCount
            false,                    //isIndirect
            false,                    //isPredicate
            false,                    //isTimestampEvent
            false,                    //L3FlushEnable
            requiresUncachedMocs,     //requiresUncachedMocs
            false,                    //useGlobalAtomics
            false,                    //isInternal
            false                     //isCooperative
        };

        return args;
    }
};

} // namespace NEO

struct WalkerThreadFixture {
    void SetUp() {
        startWorkGroup[0] = startWorkGroup[1] = startWorkGroup[2] = 0u;
        numWorkGroups[0] = numWorkGroups[1] = numWorkGroups[2] = 1u;
        workGroupSizes[0] = 32u;
        workGroupSizes[1] = workGroupSizes[2] = 1u;
        simd = 32u;
        localIdDimensions = 3u;
        requiredWorkGroupOrder = 0u;
    }
    void TearDown() {}

    uint32_t startWorkGroup[3];
    uint32_t numWorkGroups[3];
    uint32_t workGroupSizes[3];
    uint32_t simd;
    uint32_t localIdDimensions;
    uint32_t requiredWorkGroupOrder;
};

using WalkerThreadTest = Test<WalkerThreadFixture>;
