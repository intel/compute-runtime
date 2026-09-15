/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include <level_zero/ze_api.h>

#include <CL/cl_ext.h>

#include <limits>
#include <map>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

TEST(KernelIndirectAccessFlagToL0Tests, givenDeviceAccessFlagWhenConvertToL0ThenReturnsDeviceFlag) {
    EXPECT_EQ(ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE, Kernel::indirectAccessFlagToL0(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL));
}

TEST(KernelIndirectAccessFlagToL0Tests, givenHostAccessFlagWhenConvertToL0ThenReturnsHostFlag) {
    EXPECT_EQ(ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST, Kernel::indirectAccessFlagToL0(CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL));
}

TEST(KernelIndirectAccessFlagToL0Tests, givenSharedAccessFlagWhenConvertToL0ThenReturnsSharedFlag) {
    EXPECT_EQ(ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED, Kernel::indirectAccessFlagToL0(CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenRoundRobinPolicyWhenConvertToL0ThenReturnsRoundRobinFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN, Kernel::schedulingHintToL0(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenOldestFirstPolicyWhenConvertToL0ThenReturnsOldestFirstFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST, Kernel::schedulingHintToL0(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenStallBasedRoundRobinPolicyWhenConvertToL0ThenReturnsStallBasedFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN, Kernel::schedulingHintToL0(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenAfterDependencyRoundRobinPolicyWhenConvertToL0ThenReturnsStallBasedFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN, Kernel::schedulingHintToL0(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenUnknownPolicyWhenConvertToL0ThenReturnsStallBasedFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN, Kernel::schedulingHintToL0(0xDEADu));
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN, Kernel::schedulingHintToL0(0u));
}

struct MockL0KernelForLeoKernel : public L0::ult::Mock<L0::KernelImp> {
    ze_result_t destroy() override {
        destroyCalled++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setArgumentValue(uint32_t argIndex, size_t argSize, const void *pArgValue) override {
        setArgumentValueCalled++;
        lastArgIndex = argIndex;
        lastArgSize = argSize;
        lastArgValue.clear();
        if (pArgValue != nullptr) {
            const auto bytes = static_cast<const uint8_t *>(pArgValue);
            lastArgValue.assign(bytes, bytes + argSize);
        }
        return setArgumentValueResult;
    }

    ze_result_t setIndirectAccess(ze_kernel_indirect_access_flags_t flags) override {
        setIndirectAccessCalled++;
        lastIndirectAccessFlags = flags;
        return setIndirectAccessResult;
    }

    ze_result_t setSchedulingHintExp(ze_scheduling_hint_exp_desc_t *pHint) override {
        setSchedulingHintExpCalled++;
        lastHint = *pHint;
        return setSchedulingHintExpResult;
    }

    uint32_t destroyCalled = 0u;
    uint32_t setArgumentValueCalled = 0u;
    uint32_t setIndirectAccessCalled = 0u;
    uint32_t setSchedulingHintExpCalled = 0u;

    ze_result_t setArgumentValueResult = ZE_RESULT_SUCCESS;
    ze_result_t setIndirectAccessResult = ZE_RESULT_SUCCESS;
    ze_result_t setSchedulingHintExpResult = ZE_RESULT_SUCCESS;

    uint32_t lastArgIndex = std::numeric_limits<uint32_t>::max();
    size_t lastArgSize = 0u;
    std::vector<uint8_t> lastArgValue{};
    ze_kernel_indirect_access_flags_t lastIndirectAccessFlags = 0u;
    ze_scheduling_hint_exp_desc_t lastHint{};
};

struct LeoKernelFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        context = std::make_unique<Context>(nullptr, nullptr, 1, &clDeviceId, true);
        program = std::make_unique<Program>(context.get());
    }

    void TearDown() override {
        kernel.reset();
        l0Kernels.clear();
        program.reset();
        context.reset();
        Test<OclFixture>::TearDown();
    }

    MockL0KernelForLeoKernel *addL0Kernel(size_t numArgs) {
        l0Kernels.push_back(std::make_unique<MockL0KernelForLeoKernel>());
        l0Kernels.back()->privateState.kernelArgHandlers.resize(numArgs);
        return l0Kernels.back().get();
    }

    Kernel *createKernel(size_t numArgs = 2u, uint32_t numDevices = 1u) {
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{};
        for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numDevices; rootDeviceIndex++) {
            kernelHandles[rootDeviceIndex] = addL0Kernel(numArgs)->toHandle();
        }
        kernel = std::make_unique<Kernel>(std::move(kernelHandles), program.get());
        return kernel.get();
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<Context> context;
    std::unique_ptr<Program> program;
    std::vector<std::unique_ptr<MockL0KernelForLeoKernel>> l0Kernels{};
    std::unique_ptr<Kernel> kernel{};
};

TEST_F(LeoKernelFixture, givenL0KernelWithArgHandlersWhenKernelIsCreatedThenArgCountMatches) {
    auto kernel = createKernel(3u);
    EXPECT_EQ(3u, kernel->getNumArgs());
}

TEST_F(LeoKernelFixture, givenKernelWithoutArgsWhenCheckingAllArgsSetThenReturnsTrue) {
    auto kernel = createKernel(0u);

    EXPECT_EQ(0u, kernel->getNumArgs());
    EXPECT_TRUE(kernel->areAllArgsSet());
}

TEST_F(LeoKernelFixture, givenNoArgsMarkedWhenCheckingAllArgsSetThenReturnsFalse) {
    auto kernel = createKernel(2u);
    EXPECT_FALSE(kernel->areAllArgsSet());
}

TEST_F(LeoKernelFixture, givenOnlySomeArgsMarkedWhenCheckingAllArgsSetThenReturnsFalse) {
    auto kernel = createKernel(3u);

    kernel->markArgAsSet(0);
    EXPECT_FALSE(kernel->areAllArgsSet());
    kernel->markArgAsSet(2);
    EXPECT_FALSE(kernel->areAllArgsSet());
}

TEST_F(LeoKernelFixture, givenEveryArgMarkedWhenCheckingAllArgsSetThenReturnsTrue) {
    auto kernel = createKernel(3u);

    for (cl_uint argIndex = 0; argIndex < 3u; argIndex++) {
        kernel->markArgAsSet(argIndex);
    }
    EXPECT_TRUE(kernel->areAllArgsSet());
}

TEST_F(LeoKernelFixture, givenSameArgMarkedTwiceWhenCheckingAllArgsSetThenTheOtherArgIsStillMissing) {
    auto kernel = createKernel(2u);

    kernel->markArgAsSet(1);
    kernel->markArgAsSet(1);
    EXPECT_FALSE(kernel->areAllArgsSet());

    kernel->markArgAsSet(0);
    EXPECT_TRUE(kernel->areAllArgsSet());
}

TEST_F(LeoKernelFixture, givenSingleDeviceKernelWhenQueryingHandlesThenTheOnlyHandleIsReturned) {
    auto kernel = createKernel(1u);

    EXPECT_EQ(l0Kernels[0]->toHandle(), kernel->getL0Handle());
    EXPECT_EQ(l0Kernels[0]->toHandle(), kernel->getL0Handle(0));
    EXPECT_EQ(l0Kernels[0].get(), kernel->getL0Object());
    EXPECT_EQ(1u, kernel->getKernelHandles().size());
}

TEST_F(LeoKernelFixture, givenMultiDeviceKernelWhenQueryingHandleByRootDeviceIndexThenMatchingHandleIsReturned) {
    auto kernel = createKernel(1u, 3u);

    EXPECT_EQ(l0Kernels[0]->toHandle(), kernel->getL0Handle(0));
    EXPECT_EQ(l0Kernels[1]->toHandle(), kernel->getL0Handle(1));
    EXPECT_EQ(l0Kernels[2]->toHandle(), kernel->getL0Handle(2));
    EXPECT_EQ(l0Kernels[1].get(), kernel->getL0Object(1));
    EXPECT_EQ(3u, kernel->getKernelHandles().size());
}

TEST_F(LeoKernelFixture, givenMultiDeviceKernelWhenQueryingUnknownRootDeviceIndexThenFirstHandleIsReturned) {
    auto kernel = createKernel(1u, 2u);

    EXPECT_EQ(l0Kernels[0]->toHandle(), kernel->getL0Handle(42));
    EXPECT_EQ(l0Kernels[0]->toHandle(), kernel->getL0Handle());
    EXPECT_EQ(l0Kernels[0].get(), kernel->getL0Object(42));
}

TEST_F(LeoKernelFixture, givenMultiDeviceKernelWhenSettingArgumentValueThenEveryL0KernelReceivesIt) {
    auto kernel = createKernel(1u, 3u);
    uint64_t argValue = 0x1234567890ABCDEFull;

    EXPECT_EQ(CL_SUCCESS, kernel->setArgumentValue(0, sizeof(argValue), &argValue));

    for (const auto &l0Kernel : l0Kernels) {
        EXPECT_EQ(1u, l0Kernel->setArgumentValueCalled);
        EXPECT_EQ(0u, l0Kernel->lastArgIndex);
        EXPECT_EQ(sizeof(argValue), l0Kernel->lastArgSize);
        ASSERT_EQ(sizeof(argValue), l0Kernel->lastArgValue.size());
        EXPECT_EQ(argValue, *reinterpret_cast<uint64_t *>(l0Kernel->lastArgValue.data()));
    }
}

TEST_F(LeoKernelFixture, givenOneFailingL0KernelWhenSettingArgumentValueThenTheFailureIsReportedAndAllAreStillCalled) {
    auto kernel = createKernel(1u, 3u);
    l0Kernels[1]->setArgumentValueResult = ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE;
    uint32_t argValue = 0x42u;

    EXPECT_EQ(CL_INVALID_ARG_SIZE, kernel->setArgumentValue(0, sizeof(argValue), &argValue));

    for (const auto &l0Kernel : l0Kernels) {
        EXPECT_EQ(1u, l0Kernel->setArgumentValueCalled);
    }
}

TEST_F(LeoKernelFixture, givenNullArgValueWhenSettingArgumentValueThenItIsForwardedAsIs) {
    auto kernel = createKernel(1u);

    EXPECT_EQ(CL_SUCCESS, kernel->setArgumentValue(0, 8u, nullptr));
    EXPECT_EQ(1u, l0Kernels[0]->setArgumentValueCalled);
    EXPECT_EQ(8u, l0Kernels[0]->lastArgSize);
    EXPECT_TRUE(l0Kernels[0]->lastArgValue.empty());
}

TEST_F(LeoKernelFixture, givenNewKernelThenExecutionTypeIsDefault) {
    auto kernel = createKernel();
    EXPECT_EQ(NEO::KernelExecutionType::defaultType, kernel->getExecutionType());
}

TEST_F(LeoKernelFixture, givenConcurrentTypeWhenSettingExecutionTypeThenItIsStored) {
    auto kernel = createKernel();

    EXPECT_EQ(CL_SUCCESS, kernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL));
    EXPECT_EQ(NEO::KernelExecutionType::concurrent, kernel->getExecutionType());
}

TEST_F(LeoKernelFixture, givenDefaultTypeWhenSettingExecutionTypeBackThenItIsRestored) {
    auto kernel = createKernel();
    ASSERT_EQ(CL_SUCCESS, kernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL));

    EXPECT_EQ(CL_SUCCESS, kernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL));
    EXPECT_EQ(NEO::KernelExecutionType::defaultType, kernel->getExecutionType());
}

TEST_F(LeoKernelFixture, givenUnknownTypeWhenSettingExecutionTypeThenReturnsInvalidValueAndTypeIsUnchanged) {
    auto kernel = createKernel();
    ASSERT_EQ(CL_SUCCESS, kernel->setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL));

    EXPECT_EQ(CL_INVALID_VALUE, kernel->setKernelExecutionType(0xDEADu));
    EXPECT_EQ(NEO::KernelExecutionType::concurrent, kernel->getExecutionType());
}

TEST_F(LeoKernelFixture, givenIndirectAccessDisabledWhenSettingItThenL0IsNotCalled) {
    auto kernel = createKernel(1u, 2u);

    EXPECT_EQ(CL_SUCCESS, kernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, CL_FALSE));

    for (const auto &l0Kernel : l0Kernels) {
        EXPECT_EQ(0u, l0Kernel->setIndirectAccessCalled);
    }
}

TEST_F(LeoKernelFixture, givenIndirectAccessEnabledWhenSettingItThenEveryL0KernelGetsTheMappedFlag) {
    auto kernel = createKernel(1u, 2u);

    EXPECT_EQ(CL_SUCCESS, kernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, CL_TRUE));

    for (const auto &l0Kernel : l0Kernels) {
        EXPECT_EQ(1u, l0Kernel->setIndirectAccessCalled);
        EXPECT_EQ(static_cast<ze_kernel_indirect_access_flags_t>(ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST), l0Kernel->lastIndirectAccessFlags);
    }
}

TEST_F(LeoKernelFixture, givenFailingL0KernelWhenSettingIndirectAccessThenTheFailureIsReported) {
    auto kernel = createKernel(1u, 2u);
    l0Kernels[1]->setIndirectAccessResult = ZE_RESULT_ERROR_UNINITIALIZED;

    EXPECT_NE(CL_SUCCESS, kernel->setIndirectAccess(CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, CL_TRUE));
    EXPECT_EQ(1u, l0Kernels[0]->setIndirectAccessCalled);
    EXPECT_EQ(1u, l0Kernels[1]->setIndirectAccessCalled);
}

TEST_F(LeoKernelFixture, givenThreadArbitrationPolicyWhenSettingItThenEveryL0KernelGetsAFullyInitializedHint) {
    auto kernel = createKernel(1u, 2u);

    EXPECT_EQ(CL_SUCCESS, kernel->setThreadArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL));

    for (const auto &l0Kernel : l0Kernels) {
        EXPECT_EQ(1u, l0Kernel->setSchedulingHintExpCalled);
        EXPECT_EQ(ZE_STRUCTURE_TYPE_SCHEDULING_HINT_EXP_DESC, l0Kernel->lastHint.stype);
        EXPECT_EQ(nullptr, l0Kernel->lastHint.pNext);
        EXPECT_EQ(static_cast<ze_scheduling_hint_exp_flags_t>(ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST), l0Kernel->lastHint.flags);
    }
}

TEST_F(LeoKernelFixture, givenFailingL0KernelWhenSettingThreadArbitrationPolicyThenTheFailureIsReported) {
    auto kernel = createKernel(1u, 2u);
    l0Kernels[0]->setSchedulingHintExpResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    EXPECT_NE(CL_SUCCESS, kernel->setThreadArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL));
    EXPECT_EQ(1u, l0Kernels[0]->setSchedulingHintExpCalled);
    EXPECT_EQ(1u, l0Kernels[1]->setSchedulingHintExpCalled);
}

TEST_F(LeoKernelFixture, givenNewKernelThenNoImageArgsAreTracked) {
    auto kernel = createKernel();
    EXPECT_TRUE(kernel->getImageArgs().empty());
}

TEST_F(LeoKernelFixture, givenImageArgWhenSetThenItIsTrackedUnderItsIndex) {
    auto kernel = createKernel();
    auto fakeImage = reinterpret_cast<Image *>(0x1234u);

    kernel->setImageArg(1, fakeImage);

    const auto &imageArgs = kernel->getImageArgs();
    ASSERT_EQ(1u, imageArgs.size());
    ASSERT_EQ(1u, imageArgs.count(1));
    EXPECT_EQ(fakeImage, imageArgs.at(1));
}

TEST_F(LeoKernelFixture, givenImageArgWhenOverwrittenThenTheNewestValueIsTracked) {
    auto kernel = createKernel();
    auto firstImage = reinterpret_cast<Image *>(0x1234u);
    auto secondImage = reinterpret_cast<Image *>(0x5678u);

    kernel->setImageArg(0, firstImage);
    kernel->setImageArg(0, secondImage);

    const auto &imageArgs = kernel->getImageArgs();
    ASSERT_EQ(1u, imageArgs.size());
    EXPECT_EQ(secondImage, imageArgs.at(0));
}

TEST_F(LeoKernelFixture, givenSeveralImageArgsWhenClearingOneThenTheOthersAreKept) {
    auto kernel = createKernel();
    auto firstImage = reinterpret_cast<Image *>(0x1234u);
    auto secondImage = reinterpret_cast<Image *>(0x5678u);

    kernel->setImageArg(0, firstImage);
    kernel->setImageArg(1, secondImage);
    kernel->clearImageArg(0);

    const auto &imageArgs = kernel->getImageArgs();
    ASSERT_EQ(1u, imageArgs.size());
    EXPECT_EQ(0u, imageArgs.count(0));
    EXPECT_EQ(secondImage, imageArgs.at(1));
}

TEST_F(LeoKernelFixture, givenNoImageArgWhenClearingItThenNothingHappens) {
    auto kernel = createKernel();
    kernel->setImageArg(0, reinterpret_cast<Image *>(0x1234u));

    kernel->clearImageArg(5);

    EXPECT_EQ(1u, kernel->getImageArgs().size());
}

TEST_F(LeoKernelFixture, givenNewKernelThenItIsNotUsingSharedObjArgs) {
    auto kernel = createKernel();
    EXPECT_FALSE(kernel->isUsingSharedObjArgs());
}

TEST_F(LeoKernelFixture, givenNullSuggestedWorkGroupCountWhenQueryingMaxConcurrentWorkGroupCountThenReturnsInvalidValue) {
    auto kernel = createKernel();
    size_t localWorkSize[] = {8, 1, 1};

    EXPECT_EQ(CL_INVALID_VALUE, kernel->getMaxConcurrentWorkGroupCount(1, localWorkSize, nullptr));
}

TEST_F(LeoKernelFixture, givenInvalidWorkDimensionWhenQueryingMaxConcurrentWorkGroupCountThenReturnsInvalidWorkDimension) {
    auto kernel = createKernel();
    size_t localWorkSize[] = {8, 1, 1};
    size_t workGroupCount = 0;

    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, kernel->getMaxConcurrentWorkGroupCount(0, localWorkSize, &workGroupCount));
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, kernel->getMaxConcurrentWorkGroupCount(4, localWorkSize, &workGroupCount));
}

TEST_F(LeoKernelFixture, givenNullLocalWorkSizeWhenQueryingMaxConcurrentWorkGroupCountThenReturnsInvalidWorkGroupSize) {
    auto kernel = createKernel();
    size_t workGroupCount = 0;

    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, kernel->getMaxConcurrentWorkGroupCount(1, nullptr, &workGroupCount));
}

TEST_F(LeoKernelFixture, givenZeroedLocalWorkSizeDimensionWhenQueryingMaxConcurrentWorkGroupCountThenReturnsInvalidWorkGroupSize) {
    auto kernel = createKernel();
    size_t workGroupCount = 0;

    size_t firstDimensionZeroed[] = {0, 8, 8};
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, kernel->getMaxConcurrentWorkGroupCount(3, firstDimensionZeroed, &workGroupCount));

    size_t lastDimensionZeroed[] = {8, 8, 0};
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, kernel->getMaxConcurrentWorkGroupCount(3, lastDimensionZeroed, &workGroupCount));
}

TEST_F(LeoKernelFixture, givenNullSuggestedWorkGroupCountAndInvalidWorkDimensionWhenQueryingMaxConcurrentWorkGroupCountThenNullOutputIsReportedFirst) {
    auto kernel = createKernel();
    size_t localWorkSize[] = {8, 1, 1};

    EXPECT_EQ(CL_INVALID_VALUE, kernel->getMaxConcurrentWorkGroupCount(0, localWorkSize, nullptr));
}

TEST_F(LeoKernelFixture, givenInvalidWorkDimensionAndNullLocalWorkSizeWhenQueryingMaxConcurrentWorkGroupCountThenWorkDimensionIsReportedFirst) {
    auto kernel = createKernel();
    size_t workGroupCount = 0;

    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, kernel->getMaxConcurrentWorkGroupCount(4, nullptr, &workGroupCount));
}

TEST_F(LeoKernelFixture, givenKernelWhenCreatedAndDestroyedThenProgramInternalReferenceIsBalanced) {
    const auto refCountBefore = program->getRefInternalCount();

    createKernel();
    EXPECT_EQ(refCountBefore + 1, program->getRefInternalCount());

    kernel.reset();
    EXPECT_EQ(refCountBefore, program->getRefInternalCount());
}

TEST_F(LeoKernelFixture, givenMultiDeviceKernelWhenDestroyedThenEveryL0KernelIsDestroyed) {
    createKernel(1u, 3u);

    kernel.reset();

    for (const auto &l0Kernel : l0Kernels) {
        EXPECT_EQ(1u, l0Kernel->destroyCalled);
    }
}

TEST_F(LeoKernelFixture, givenKernelWhenQueryingContextThenTheProgramContextIsReturned) {
    auto kernel = createKernel();
    EXPECT_EQ(context.get(), kernel->getContext());
    EXPECT_EQ(program->getContext(), kernel->getContext());
}

TEST_F(LeoKernelFixture, givenNewKernelThenReferenceCountIsOne) {
    auto kernel = createKernel();
    EXPECT_EQ(1, kernel->getReference());
}

TEST_F(LeoKernelFixture, givenKernelWhenTakingOwnershipTwiceFromTheSameThreadThenItIsRecursive) {
    auto kernel = createKernel();

    auto firstLock = kernel->takeOwnership();
    auto secondLock = kernel->takeOwnership();
    EXPECT_TRUE(firstLock.owns_lock());
    EXPECT_TRUE(secondLock.owns_lock());
}

TEST_F(LeoKernelFixture, givenKernelWhenCastFromHandleThenObjectIsRecovered) {
    auto kernel = createKernel();

    cl_kernel clKernel = kernel;
    EXPECT_EQ(kernel, castToObject<Kernel>(clKernel));
    EXPECT_EQ(static_cast<cl_ulong>(Kernel::objectMagic), kernel->getMagic() & Kernel::maskMagic);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
