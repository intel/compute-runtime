/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/command_stream/command_stream_aub_fixture.h"

#include <cstdint>

namespace L0 {
namespace ult {

struct MiAtomicAubFixture : public CommandStreamAubFixture {
    void setUp() {
        CommandStreamAubFixture::setUp();
        this->deviceSurface = this->allocate(MemoryConstants::pageSize, NEO::AllocationType::buffer);
        ASSERT_NE(nullptr, this->deviceSurface);
        this->systemSurface = this->allocate(MemoryConstants::pageSize, NEO::AllocationType::svmCpu);
        ASSERT_NE(nullptr, this->systemSurface);
    }

    NEO::GraphicsAllocation *deviceSurface = nullptr;
    NEO::GraphicsAllocation *systemSurface = nullptr;
};

using MiAtomicAubTest = Test<MiAtomicAubFixture>;

HWTEST_F(MiAtomicAubTest, WhenDispatchingAtomicMoveOperationThenExpectCorrectEndValues) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    auto atomicAddress = this->deviceSurface->getGpuAddress();

    auto expectedGpuAddressDwordOp1 = atomicAddress;
    auto expectedGpuAddressDwordOp2 = expectedGpuAddressDwordOp1 + sizeof(uint32_t);
    auto expectedGpuAddressQwordOp3 = expectedGpuAddressDwordOp2 + sizeof(uint32_t);

    uint32_t operation1dword0 = 0x10;
    NEO::EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, expectedGpuAddressDwordOp1,
                                                   MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                                   MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD,
                                                   0, 0, operation1dword0, 0u);

    uint32_t operation2dword0 = 0x22;
    NEO::EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, expectedGpuAddressDwordOp2,
                                                   MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                                   MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD,
                                                   0, 0, operation2dword0, 0u);

    uint32_t operation3dword0 = 0xF0;
    uint32_t operation3dword1 = 0x1F;
    uint64_t operation3qword = (static_cast<uint64_t>(operation3dword1) << 32) | operation3dword0;
    NEO::EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, expectedGpuAddressQwordOp3,
                                                   MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_MOVE,
                                                   MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD,
                                                   0, 0, operation3qword, 0);

    this->flushStream();

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressDwordOp1), &operation1dword0, sizeof(operation1dword0));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressDwordOp2), &operation2dword0, sizeof(operation2dword0));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressQwordOp3), &operation3qword, sizeof(operation3qword));
}

HWTEST2_F(MiAtomicAubTest, GivenSystemMemoryWhenDispatchingAtomicMove4BytesOperationThenExpectCorrectEndValues, IsNotXeHpcCore) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    auto atomicAddress = this->systemSurface->getGpuAddress();

    auto expectedGpuAddressDwordOp1 = atomicAddress;
    auto expectedGpuAddressDwordOp2 = expectedGpuAddressDwordOp1 + sizeof(uint32_t);

    uint32_t operation1dword0 = 0x15;
    NEO::EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, expectedGpuAddressDwordOp1,
                                                   MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                                   MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD,
                                                   0, 0, operation1dword0, 0u);

    uint32_t operation2dword0 = 0xFF;
    NEO::EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, expectedGpuAddressDwordOp2,
                                                   MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                                   MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD,
                                                   0, 0, operation2dword0, 0u);

    this->flushStream();

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressDwordOp1), &operation1dword0, sizeof(operation1dword0));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddressDwordOp2), &operation2dword0, sizeof(operation2dword0));
}

} // namespace ult
} // namespace L0
