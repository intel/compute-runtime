/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include <limits>

namespace L0 {
namespace ult {

class AppendFillFixture : public DeviceFixture, public ::testing::Test {
  public:
    class MockDriverFillHandle : public L0::DriverHandleImp {
      public:
        bool findAllocationDataForRange(const void *buffer,
                                        size_t size,
                                        NEO::SvmAllocationData **allocData) override {
            mockAllocation.reset(new NEO::MockGraphicsAllocation(const_cast<void *>(buffer), size));
            data.gpuAllocations.addAllocation(mockAllocation.get());
            if (allocData) {
                *allocData = &data;
            }
            return true;
        }
        const uint32_t rootDeviceIndex = 0u;
        std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
        NEO::SvmAllocationData data{rootDeviceIndex};
    };

    template <GFXCORE_FAMILY gfxCoreFamily>
    class MockCommandList : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
      public:
        MockCommandList() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}

        ze_result_t appendLaunchKernelWithParams(ze_kernel_handle_t hKernel,
                                                 const ze_group_count_t *pThreadGroupDimensions,
                                                 ze_event_handle_t hEvent,
                                                 bool isIndirect,
                                                 bool isPredicate,
                                                 bool isCooperative) override {
            if (numberOfCallsToAppendLaunchKernelWithParams == thresholdOfCallsToAppendLaunchKernelWithParamsToFail) {
                return ZE_RESULT_ERROR_UNKNOWN;
            }

            numberOfCallsToAppendLaunchKernelWithParams++;
            return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(hKernel,
                                                                                      pThreadGroupDimensions,
                                                                                      hEvent,
                                                                                      isIndirect,
                                                                                      isPredicate,
                                                                                      isCooperative);
        }

        uint32_t thresholdOfCallsToAppendLaunchKernelWithParamsToFail = std::numeric_limits<uint32_t>::max();
        uint32_t numberOfCallsToAppendLaunchKernelWithParams = 0;
    };

    void SetUp() override {
        dstPtr = new uint8_t[allocSize];
        immediateDstPtr = new uint8_t[allocSize];

        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<MockDriverFillHandle>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() override {
        delete[] immediateDstPtr;
        delete[] dstPtr;
    }

    std::unique_ptr<Mock<MockDriverFillHandle>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    static constexpr size_t allocSize = 70;
    static constexpr size_t patternSize = 8;
    uint8_t *dstPtr = nullptr;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    static constexpr size_t immediateAllocSize = 106;
    uint8_t immediatePattern = 4;
    uint8_t *immediateDstPtr = nullptr;
};

using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(AppendFillFixture,
          givenCallToAppendMemoryFillWithImmediateValueThenSuccessIsReturned, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    auto result = commandList->appendMemoryFill(immediateDstPtr, &immediatePattern,
                                                sizeof(immediatePattern),
                                                immediateAllocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillFixture,
          givenCallToAppendMemoryFillThenSuccessIsReturned, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillFixture,
          givenCallToAppendMemoryFillWithAppendLaunchKernelFailureThenSuccessIsNotReturned, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 0;

    auto result = commandList->appendMemoryFill(dstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(AppendFillFixture,
          givenTwoCallsToAppendMemoryFillWithSamePatternThenAllocationIsAddedtoHostPtrMapOnlyOnce, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, 4, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t hostPtrMapSize = commandList->getHostPtrMap().size();
    EXPECT_EQ(hostPtrMapSize, 1u);

    uint8_t *newDstPtr = new uint8_t[allocSize];
    result = commandList->appendMemoryFill(newDstPtr, pattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newHostPtrMapSize = commandList->getHostPtrMap().size();

    EXPECT_EQ(hostPtrMapSize, newHostPtrMapSize);

    delete[] newDstPtr;
}

HWTEST2_F(AppendFillFixture,
          givenTwoCallsToAppendMemoryFillWithDifferentPatternsThenHostPtrSizeIncrementsByOne, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    ze_result_t result = commandList->appendMemoryFill(dstPtr, pattern, 4, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t hostPtrMapSize = commandList->getHostPtrMap().size();
    EXPECT_EQ(hostPtrMapSize, 1u);

    uint8_t newPattern[patternSize] = {1, 2, 3, 4};
    result = commandList->appendMemoryFill(dstPtr, newPattern, patternSize, allocSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t newHostPtrMapSize = commandList->getHostPtrMap().size();

    EXPECT_EQ(hostPtrMapSize + 1u, newHostPtrMapSize);
}

HWTEST2_F(AppendFillFixture,
          givenCallToAppendMemoryFillWithSizeNotMultipleOfPatternSizeThenSuccessIsReturned, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    size_t nonMultipleSize = allocSize + 1;
    uint8_t *nonMultipleDstPtr = new uint8_t[nonMultipleSize];
    auto result = commandList->appendMemoryFill(nonMultipleDstPtr, pattern, 4, nonMultipleSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    delete[] nonMultipleDstPtr;
}

HWTEST2_F(AppendFillFixture,
          givenCallToAppendMemoryFillWithSizeNotMultipleOfPatternSizeAndAppendLaunchKernelFailureOnRemainderThenSuccessIsNotReturned, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<WhiteBox<MockCommandList<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    commandList->thresholdOfCallsToAppendLaunchKernelWithParamsToFail = 1;

    size_t nonMultipleSize = allocSize + 1;
    uint8_t *nonMultipleDstPtr = new uint8_t[nonMultipleSize];
    auto result = commandList->appendMemoryFill(nonMultipleDstPtr, pattern, 4, nonMultipleSize, nullptr, 0, nullptr);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    delete[] nonMultipleDstPtr;
}

} // namespace ult
} // namespace L0
