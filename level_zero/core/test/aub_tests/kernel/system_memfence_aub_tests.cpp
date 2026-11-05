/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/aub_tests/kernel/kernel_aub_fixture.h"

#include <limits>
#include <optional>
#include <vector>

extern std::optional<uint32_t> blitterMaskOverride;

namespace L0 {
namespace ult {

struct SystemMemFenceFixtureL0 : public KernelAubFixtureL0 {
    void setUp(int32_t fenceAsMiMemFence, int32_t fenceAsPostSync, int32_t fenceAsKernelInstruction, const NEO::HardwareInfo *hardwareInfo) {
        NEO::debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(fenceAsMiMemFence);
        NEO::debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(fenceAsPostSync);
        NEO::debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(fenceAsKernelInstruction);
        AUBFixtureL0::setUp(hardwareInfo, false);
    }

    void copyAndExecute(void *dst, const void *src, size_t size) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListReset(this->commandList->toHandle()));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(this->commandList->toHandle(), dst, src, size, nullptr, 0, nullptr));
        this->executeAndSynchronize();
    }

    template <typename FamilyType, typename CopyToHostT>
    void copyToDeviceAndBackToHostAndExpect(CopyToHostT &&copyToHost) {
        const size_t bufferSize = MemoryConstants::kiloByte;
        std::vector<char> buffer(bufferSize, 0x11);

        auto deviceMemAlloc = this->allocateDeviceMemory(bufferSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, deviceMemAlloc);
        this->copyAndExecute(deviceMemAlloc, buffer.data(), bufferSize);
        this->expectMemory<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize);

        auto hostMemAlloc = this->allocateHostMemory(bufferSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, hostMemAlloc);
        copyToHost(hostMemAlloc, deviceMemAlloc, bufferSize);
        this->expectMemory<FamilyType>(this->getGpuPointer(hostMemAlloc), buffer.data(), bufferSize);
    }

    template <typename FamilyType>
    void copyToDeviceAndBackToHostAndExpect() {
        this->copyToDeviceAndBackToHostAndExpect<FamilyType>([this](void *dst, const void *src, size_t size) {
            this->copyAndExecute(dst, src, size);
        });
    }
};

template <int32_t fenceAsMiMemFence, int32_t fenceAsPostSync, int32_t fenceAsKernelInstruction>
struct SystemMemFenceWithFlagsFixtureL0 : public SystemMemFenceFixtureL0 {
    void setUp() {
        SystemMemFenceFixtureL0::setUp(fenceAsMiMemFence, fenceAsPostSync, fenceAsKernelInstruction, NEO::defaultHwInfo.get());
    }
};

using SystemMemFenceMiMemFenceL0 = Test<SystemMemFenceWithFlagsFixtureL0<1, 0, 0>>;

HWTEST2_F(SystemMemFenceMiMemFenceL0, WhenGeneratedAsMiMemFenceCommandInCommandStreamThenWritesToSystemMemoryAreGloballyObservable, IsAtLeastXeHpcCore) {
    this->copyToDeviceAndBackToHostAndExpect<FamilyType>();
}

using SystemMemFenceWalkerL0 = Test<SystemMemFenceWithFlagsFixtureL0<0, 1, 0>>;

HWTEST2_F(SystemMemFenceWalkerL0, WhenGeneratedAsPostSyncOperationInWalkerThenWritesToSystemMemoryAreGloballyObservable, IsAtLeastXeHpcCore) {
    this->copyToDeviceAndBackToHostAndExpect<FamilyType>();
}

struct SystemMemFenceBlitterFixtureL0 : public SystemMemFenceFixtureL0 {
    void setUp() {
        NEO::MockExecutionEnvironment mockExecutionEnvironment{};
        auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
        if (!productHelper.obtainBlitterPreference(*NEO::defaultHwInfo.get())) {
            GTEST_SKIP();
        }

        auto hwInfo = *NEO::defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = true;
        if (blitterMaskOverride.has_value()) {
            hwInfo.featureTable.ftrBcsInfo = blitterMaskOverride.value();
        }
        SystemMemFenceFixtureL0::setUp(1, 0, 0, &hwInfo);

        uint32_t groupCount = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetCommandQueueGroupProperties(this->device->toHandle(), &groupCount, nullptr));
        std::vector<ze_command_queue_group_properties_t> groupProperties(groupCount, {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES});
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeDeviceGetCommandQueueGroupProperties(this->device->toHandle(), &groupCount, groupProperties.data()));

        uint32_t copyOrdinal = std::numeric_limits<uint32_t>::max();
        for (uint32_t ordinal = 0; ordinal < groupCount; ordinal++) {
            const auto flags = groupProperties[ordinal].flags;
            if ((flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) && !(flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
                copyOrdinal = ordinal;
                break;
            }
        }
        ASSERT_NE(std::numeric_limits<uint32_t>::max(), copyOrdinal) << "no copy-only command queue group";

        ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.ordinal = copyOrdinal;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueCreate(this->context->toHandle(), this->device->toHandle(), &queueDesc, &this->copyQueue));

        ze_command_list_desc_t commandListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
        commandListDesc.commandQueueGroupOrdinal = copyOrdinal;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(this->context->toHandle(), this->device->toHandle(), &commandListDesc, &this->copyCommandList));
    }

    void tearDown() {
        if (this->copyCommandList != nullptr) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(this->copyCommandList));
        }
        if (this->copyQueue != nullptr) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueDestroy(this->copyQueue));
        }
        SystemMemFenceFixtureL0::tearDown();
    }

    void copyOnCopyEngine(void *dst, const void *src, size_t size) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListReset(this->copyCommandList));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(this->copyCommandList, dst, src, size, nullptr, 0, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListClose(this->copyCommandList));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueExecuteCommandLists(this->copyQueue, 1, &this->copyCommandList, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueSynchronize(this->copyQueue, std::numeric_limits<uint64_t>::max()));
    }

    NEO::CommandStreamReceiver *getCopyQueueCsr() {
        return static_cast<WhiteBox<::L0::CommandQueue> *>(L0::CommandQueue::fromHandle(this->copyQueue))->csr;
    }

    ze_command_queue_handle_t copyQueue = nullptr;
    ze_command_list_handle_t copyCommandList = nullptr;
};

using SystemMemFenceBcsL0 = Test<SystemMemFenceBlitterFixtureL0>;

HWTEST2_F(SystemMemFenceBcsL0, givenSystemMemFenceWhenGeneratedAsMiMemFenceCmdInBCSThenWritesToSystemMemoryAreGloballyObservable, IsAtLeastXeHpcCore) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);
    auto copyQueueCsr = this->getCopyQueueCsr();

    auto deviceMemAlloc = this->allocateDeviceMemory(bufferSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, deviceMemAlloc);
    this->copyOnCopyEngine(deviceMemAlloc, buffer.data(), bufferSize);

    if (!this->neoDevice->getHardwareInfo().caps.ftrXe2Compression) {
        EXPECT_TRUE(copyQueueCsr->expectMemory(deviceMemAlloc, buffer.data(), bufferSize, aub_stream::CompareOperationValues::CompareEqual));
    }

    auto hostMemAlloc = this->allocateHostMemory(bufferSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, hostMemAlloc);
    this->copyOnCopyEngine(hostMemAlloc, deviceMemAlloc, bufferSize);
    EXPECT_TRUE(copyQueueCsr->expectMemory(this->getGpuPointer(hostMemAlloc), buffer.data(), bufferSize, aub_stream::CompareOperationValues::CompareEqual));
}

using SystemMemFenceKernelL0 = Test<SystemMemFenceWithFlagsFixtureL0<0, 0, 1>>;

HWTEST2_F(SystemMemFenceKernelL0, givenSystemMemFenceWhenKernelInstructionThenWritesToSystemMemoryAreGloballyObservable, IsXeHpcCore) {
    auto kernel = this->createKernel("system_memfence", "SystemMemFence", true);
    ASSERT_NE(nullptr, kernel);

    this->copyToDeviceAndBackToHostAndExpect<FamilyType>([&](void *dst, const void *src, size_t size) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &src));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 1, sizeof(void *), &dst));

        const uint32_t globalWorkSize = static_cast<uint32_t>(size / sizeof(uint32_t));
        uint32_t groupSize[3] = {1, 1, 1};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSuggestGroupSize(kernel, globalWorkSize, 1, 1, &groupSize[0], &groupSize[1], &groupSize[2]));
        const uint32_t groupCount[3] = {globalWorkSize / groupSize[0], 1, 1};

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListReset(this->commandList->toHandle()));
        this->appendLaunch(kernel, groupSize, groupCount);
        this->executeAndSynchronize();
    });
}

} // namespace ult
} // namespace L0
