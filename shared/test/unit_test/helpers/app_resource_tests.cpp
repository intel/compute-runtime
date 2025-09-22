/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/app_resource_defines.h"
#include "shared/source/helpers/app_resource_helper.h"
#include "shared/source/memory_manager/definitions/storage_info.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using MockExecutionEnvironmentTagTest = Test<NEO::MockExecutionEnvironmentGmmFixture>;

using namespace NEO;

struct AppResourceTests : public MockExecutionEnvironmentTagTest {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::setUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        localPlatformDevice = rootDeviceEnvironment->getMutableHardwareInfo();
    }

    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    HardwareInfo *localPlatformDevice = nullptr;
    const DeviceBitfield singleTileMask{static_cast<uint32_t>(1u << 2)};
};

TEST_F(AppResourceTests, givenIncorrectGraphicsAllocationTypeWhenGettingResourceTagThenNOTFOUNDIsReturned) {
    auto tag = AppResourceHelper::getResourceTagStr(static_cast<AllocationType>(999)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_STREQ(tag, "NOTFOUND");
}

TEST_F(AppResourceTests, givenGraphicsAllocationTypeWhenGettingResourceTagThenForEveryDefinedTypeProperTagExist) {
    auto firstTypeIdx = static_cast<int>(AllocationType::unknown);
    auto lastTypeIdx = static_cast<int>(AllocationType::count);

    for (int typeIdx = firstTypeIdx; typeIdx < lastTypeIdx; typeIdx++) {
        auto allocationType = static_cast<AllocationType>(typeIdx);
        auto tag = AppResourceHelper::getResourceTagStr(allocationType);

        EXPECT_LE(strlen(tag), AppResourceDefines::maxStrLen);
        EXPECT_STRNE(tag, "NOTFOUND");
    }
}

struct AllocationTypeTagTestCase {
    AllocationType type;
    const char *str;
};

AllocationTypeTagTestCase allocationTypeTagValues[static_cast<int>(AllocationType::count)] = {
    {AllocationType::buffer, "BUFFER"},
    {AllocationType::bufferHostMemory, "BFHSTMEM"},
    {AllocationType::commandBuffer, "CMNDBUFF"},
    {AllocationType::constantSurface, "CSNTSRFC"},
    {AllocationType::externalHostPtr, "EXHSTPTR"},
    {AllocationType::fillPattern, "FILPATRN"},
    {AllocationType::globalSurface, "GLBLSRFC"},
    {AllocationType::image, "IMAGE"},
    {AllocationType::indirectObjectHeap, "INOBHEAP"},
    {AllocationType::instructionHeap, "INSTHEAP"},
    {AllocationType::internalHeap, "INTLHEAP"},
    {AllocationType::internalHostMemory, "INHSTMEM"},
    {AllocationType::kernelArgsBuffer, "KARGBUF"},
    {AllocationType::kernelIsa, "KERNLISA"},
    {AllocationType::kernelIsaInternal, "KRLISAIN"},
    {AllocationType::linearStream, "LINRSTRM"},
    {AllocationType::mapAllocation, "MAPALLOC"},
    {AllocationType::mcs, "MCS"},
    {AllocationType::preemption, "PRMPTION"},
    {AllocationType::printfSurface, "PRNTSRFC"},
    {AllocationType::privateSurface, "PRVTSRFC"},
    {AllocationType::profilingTagBuffer, "PROFTGBF"},
    {AllocationType::scratchSurface, "SCRHSRFC"},
    {AllocationType::workPartitionSurface, "WRPRTSRF"},
    {AllocationType::sharedBuffer, "SHRDBUFF"},
    {AllocationType::sharedImage, "SHERDIMG"},
    {AllocationType::sharedResourceCopy, "SRDRSCCP"},
    {AllocationType::surfaceStateHeap, "SRFCSTHP"},
    {AllocationType::svmCpu, "SVM_CPU"},
    {AllocationType::svmGpu, "SVM_GPU"},
    {AllocationType::svmZeroCopy, "SVM0COPY"},
    {AllocationType::syncBuffer, "SYNCBUFF"},
    {AllocationType::tagBuffer, "TAGBUFER"},
    {AllocationType::globalFence, "GLBLFENC"},
    {AllocationType::timestampPacketTagBuffer, "TSPKTGBF"},
    {AllocationType::unknown, "UNKNOWN"},
    {AllocationType::writeCombined, "WRTCMBND"},
    {AllocationType::ringBuffer, "RINGBUFF"},
    {AllocationType::semaphoreBuffer, "SMPHRBUF"},
    {AllocationType::debugContextSaveArea, "DBCXSVAR"},
    {AllocationType::debugSbaTrackingBuffer, "DBSBATRB"},
    {AllocationType::debugModuleArea, "DBMDLARE"},
    {AllocationType::unifiedSharedMemory, "USHRDMEM"},
    {AllocationType::gpuTimestampDeviceBuffer, "GPUTSDBF"},
    {AllocationType::swTagBuffer, "SWTAGBF"},
    {AllocationType::deferredTasksList, "TSKLIST"},
    {AllocationType::assertBuffer, "ASSRTBUF"},
    {AllocationType::syncDispatchToken, "SYNCTOK"},
    {AllocationType::hostFunction, "HOSTFUNC"}};
class AllocationTypeTagString : public ::testing::TestWithParam<AllocationTypeTagTestCase> {};

TEST_P(AllocationTypeTagString, givenGraphicsAllocationTypeWhenCopyTagToStorageInfoThenCorrectTagIsReturned) {
    DebugManagerStateRestore restorer;
    StorageInfo storageInfo = {};
    auto input = GetParam();

    debugManager.flags.EnableResourceTags.set(true);
    AppResourceHelper::copyResourceTagStr(storageInfo.resourceTag, input.type,
                                          sizeof(storageInfo.resourceTag));
    EXPECT_STREQ(storageInfo.resourceTag, input.str);
}

INSTANTIATE_TEST_SUITE_P(AllAllocationTypesTag, AllocationTypeTagString, ::testing::ValuesIn(allocationTypeTagValues));
