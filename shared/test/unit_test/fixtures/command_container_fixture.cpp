/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/fixtures/command_container_fixture.h"

#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

void CommandEncodeStatesFixture::setUp() {
    DeviceFixture::setUp();
    cmdContainer = std::make_unique<MyMockCommandContainer>();
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);
    const auto &hwInfo = pDevice->getHardwareInfo();
    cmdContainer->systolicModeSupport = HwInfoConfig::get(hwInfo.platform.eProductFamily)->isSystolicModeConfigurable(hwInfo);
}

void CommandEncodeStatesFixture::tearDown() {
    cmdContainer.reset();
    DeviceFixture::tearDown();
}

EncodeDispatchKernelArgs CommandEncodeStatesFixture::createDefaultDispatchKernelArgs(Device *device,
                                                                                     DispatchKernelEncoderI *dispatchInterface,
                                                                                     const void *threadGroupDimensions,
                                                                                     bool requiresUncachedMocs) {
    EncodeDispatchKernelArgs args{
        0,                        // eventAddress
        device,                   // device
        dispatchInterface,        // dispatchInterface
        threadGroupDimensions,    // threadGroupDimensions
        nullptr,                  // additionalCommands
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
        false,                    // isKernelDispatchedFromImmediateCmdList
        false,                    // isRcs
        false                     // dcFlushEnable
    };

    return args;
}

} // namespace NEO

void WalkerThreadFixture::setUp() {
    startWorkGroup[0] = startWorkGroup[1] = startWorkGroup[2] = 0u;
    numWorkGroups[0] = numWorkGroups[1] = numWorkGroups[2] = 1u;
    workGroupSizes[0] = 32u;
    workGroupSizes[1] = workGroupSizes[2] = 1u;
    simd = 32u;
    localIdDimensions = 3u;
    requiredWorkGroupOrder = 0u;
}
