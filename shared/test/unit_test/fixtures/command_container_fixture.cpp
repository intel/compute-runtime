/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/fixtures/command_container_fixture.h"

#include "shared/source/indirect_heap/heap_size.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

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

EncodeWalkerArgs CommandEncodeStatesFixture::createDefaultEncodeWalkerArgs(const KernelDescriptor &kernelDescriptor) {

    EncodeWalkerArgs args{
        .kernelExecutionType = KernelExecutionType::defaultType,
        .requiredDispatchWalkOrder = RequiredDispatchWalkOrder::none,
        .localRegionSize = 0,
        .maxFrontEndThreads = 0,
        .requiredSystemFence = false,
        .hasSample = kernelDescriptor.kernelAttributes.flags.hasSample};

    return args;
}

void ScratchProgrammingFixture::setUp() {
    NEO::DeviceFixture::setUp();
    size_t sizeStream = 512;
    size_t alignmentStream = 0x1000;
    ssh = new IndirectHeap{nullptr};
    sshBuffer = alignedMalloc(sizeStream, alignmentStream);
    ASSERT_NE(nullptr, sshBuffer);
    ssh->replaceBuffer(sshBuffer, sizeStream);
    auto graphicsAllocation = new MockGraphicsAllocation(sshBuffer, sizeStream);
    ssh->replaceGraphicsAllocation(graphicsAllocation);
}

void ScratchProgrammingFixture::tearDown() {
    delete ssh->getGraphicsAllocation();
    delete ssh;
    alignedFree(sshBuffer);
    NEO::DeviceFixture::tearDown();
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
