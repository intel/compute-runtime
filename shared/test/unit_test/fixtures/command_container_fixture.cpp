/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/fixtures/command_container_fixture.h"

#include "shared/source/indirect_heap/heap_size.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/mocks/mock_device.h"

namespace NEO {

void CommandEncodeStatesFixture::setUp() {
    DeviceFixture::setUp();
    cmdContainer = std::make_unique<MyMockCommandContainer>();
    cmdContainer->initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    cmdContainer->setDirtyStateForAllHeaps(false);
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &productHelper = pDevice->getProductHelper();
    cmdContainer->systolicModeSupportRef() = productHelper.isSystolicModeConfigurable(hwInfo);
    cmdContainer->doubleSbaWaRef() = productHelper.isAdditionalStateBaseAddressWARequired(hwInfo);
    this->l1CachePolicyData.init(productHelper);
    cmdContainer->l1CachePolicyDataRef() = &this->l1CachePolicyData;
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
        0,                                          // eventAddress
        0,                                          // postSyncImmValue
        0,                                          // inOrderCounterValue
        device,                                     // device
        nullptr,                                    // inOrderExecInfo
        dispatchInterface,                          // dispatchInterface
        nullptr,                                    // surfaceStateHeap
        nullptr,                                    // dynamicStateHeap
        threadGroupDimensions,                      // threadGroupDimensions
        nullptr,                                    // outWalkerPtr
        nullptr,                                    // additionalCommands
        PreemptionMode::Disabled,                   // preemptionMode
        NEO::RequiredPartitionDim::None,            // requiredPartitionDim
        NEO::RequiredDispatchWalkOrder::None,       // requiredDispatchWalkOrder
        NEO::additionalKernelLaunchSizeParamNotSet, // additionalSizeParam
        1,                                          // partitionCount
        false,                                      // isIndirect
        false,                                      // isPredicate
        false,                                      // isTimestampEvent
        requiresUncachedMocs,                       // requiresUncachedMocs
        false,                                      // useGlobalAtomics
        false,                                      // isInternal
        false,                                      // isCooperative
        false,                                      // isHostScopeSignalEvent
        false,                                      // isKernelUsingSystemAllocation
        false,                                      // isKernelDispatchedFromImmediateCmdList
        false,                                      // isRcs
        false                                       // dcFlushEnable
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
