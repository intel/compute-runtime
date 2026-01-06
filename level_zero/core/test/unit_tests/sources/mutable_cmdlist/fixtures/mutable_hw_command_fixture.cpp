/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/test/common/mocks/mock_device.h"

namespace L0 {
namespace ult {

void MutableHwCommandFixture::setUp() {
    DeviceFixture::setUp();

    this->isHeapless = neoDevice->getCompilerProductHelper().isHeaplessModeEnabled(neoDevice->getHardwareInfo());

    constexpr size_t bufferSize = 4096;
    constexpr size_t elemSize = bufferSize / sizeof(uint64_t);
    this->bufferStorage = std::make_unique<uint64_t[]>(elemSize);

    this->cmdBufferGpuPtr = this->bufferStorage.get();

    constexpr uint64_t gpuAddr = 0x1000;
    this->mockAllocation = std::make_unique<NEO::MockGraphicsAllocation>(this->cmdBufferGpuPtr, gpuAddr, bufferSize);

    this->commandStream.replaceGraphicsAllocation(this->mockAllocation.get());
    this->commandStream.replaceBuffer(this->cmdBufferGpuPtr, bufferSize);
}

void MutableHwCommandFixture::tearDown() {
    DeviceFixture::tearDown();
}

} // namespace ult
} // namespace L0
