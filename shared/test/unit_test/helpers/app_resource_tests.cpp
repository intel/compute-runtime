/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/app_resource_defines.h"
#include "shared/source/helpers/app_resource_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

using MockExecutionEnvironmentTagTest = Test<NEO::MockExecutionEnvironmentGmmFixture>;

using namespace NEO;

struct AppResourceTests : public MockExecutionEnvironmentTagTest {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::SetUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        localPlatformDevice = rootDeviceEnvironment->getMutableHardwareInfo();
    }

    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    HardwareInfo *localPlatformDevice = nullptr;
    const DeviceBitfield singleTileMask{static_cast<uint32_t>(1u << 2)};
};

TEST_F(AppResourceTests, givenIncorrectGraphicsAllocationTypeWhenGettingResourceTagThenNOTFOUNDIsReturned) {
    auto tag = AppResourceHelper::getResourceTagStr(static_cast<GraphicsAllocation::AllocationType>(999));
    EXPECT_STREQ(tag, "NOTFOUND");
}

TEST_F(AppResourceTests, givenGraphicsAllocationTypeWhenGettingResourceTagThenForEveryDefinedTypeProperTagExist) {
    auto firstTypeIdx = static_cast<int>(GraphicsAllocation::AllocationType::UNKNOWN);
    auto lastTypeIdx = static_cast<int>(GraphicsAllocation::AllocationType::COUNT);

    for (int typeIdx = firstTypeIdx; typeIdx < lastTypeIdx; typeIdx++) {
        auto allocationType = static_cast<GraphicsAllocation::AllocationType>(typeIdx);
        auto tag = AppResourceHelper::getResourceTagStr(allocationType);

        EXPECT_LE(strlen(tag), AppResourceDefines::maxStrLen);
        EXPECT_STRNE(tag, "NOTFOUND");
    }
}

struct AllocationTypeTagTestCase {
    GraphicsAllocation::AllocationType type;
    const char *str;
};

AllocationTypeTagTestCase allocationTypeTagValues[static_cast<int>(GraphicsAllocation::AllocationType::COUNT)] = {
    {GraphicsAllocation::AllocationType::BUFFER, "BUFFER"},
    {GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, "BFHSTMEM"},
    {GraphicsAllocation::AllocationType::COMMAND_BUFFER, "CMNDBUFF"},
    {GraphicsAllocation::AllocationType::CONSTANT_SURFACE, "CSNTSRFC"},
    {GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, "EXHSTPTR"},
    {GraphicsAllocation::AllocationType::FILL_PATTERN, "FILPATRN"},
    {GraphicsAllocation::AllocationType::GLOBAL_SURFACE, "GLBLSRFC"},
    {GraphicsAllocation::AllocationType::IMAGE, "IMAGE"},
    {GraphicsAllocation::AllocationType::INDIRECT_OBJECT_HEAP, "INOBHEAP"},
    {GraphicsAllocation::AllocationType::INSTRUCTION_HEAP, "INSTHEAP"},
    {GraphicsAllocation::AllocationType::INTERNAL_HEAP, "INTLHEAP"},
    {GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY, "INHSTMEM"},
    {GraphicsAllocation::AllocationType::KERNEL_ISA, "KERNLISA"},
    {GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL, "KRLISAIN"},
    {GraphicsAllocation::AllocationType::LINEAR_STREAM, "LINRSTRM"},
    {GraphicsAllocation::AllocationType::MAP_ALLOCATION, "MAPALLOC"},
    {GraphicsAllocation::AllocationType::MCS, "MCS"},
    {GraphicsAllocation::AllocationType::PIPE, "PIPE"},
    {GraphicsAllocation::AllocationType::PREEMPTION, "PRMPTION"},
    {GraphicsAllocation::AllocationType::PRINTF_SURFACE, "PRNTSRFC"},
    {GraphicsAllocation::AllocationType::PRIVATE_SURFACE, "PRVTSRFC"},
    {GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER, "PROFTGBF"},
    {GraphicsAllocation::AllocationType::SCRATCH_SURFACE, "SCRHSRFC"},
    {GraphicsAllocation::AllocationType::WORK_PARTITION_SURFACE, "WRPRTSRF"},
    {GraphicsAllocation::AllocationType::SHARED_BUFFER, "SHRDBUFF"},
    {GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE, "SRDCXIMG"},
    {GraphicsAllocation::AllocationType::SHARED_IMAGE, "SHERDIMG"},
    {GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY, "SRDRSCCP"},
    {GraphicsAllocation::AllocationType::SURFACE_STATE_HEAP, "SRFCSTHP"},
    {GraphicsAllocation::AllocationType::SVM_CPU, "SVM_CPU"},
    {GraphicsAllocation::AllocationType::SVM_GPU, "SVM_GPU"},
    {GraphicsAllocation::AllocationType::SVM_ZERO_COPY, "SVM0COPY"},
    {GraphicsAllocation::AllocationType::TAG_BUFFER, "TAGBUFER"},
    {GraphicsAllocation::AllocationType::GLOBAL_FENCE, "GLBLFENC"},
    {GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, "TSPKTGBF"},
    {GraphicsAllocation::AllocationType::UNKNOWN, "UNKNOWN"},
    {GraphicsAllocation::AllocationType::WRITE_COMBINED, "WRTCMBND"},
    {GraphicsAllocation::AllocationType::RING_BUFFER, "RINGBUFF"},
    {GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER, "SMPHRBUF"},
    {GraphicsAllocation::AllocationType::DEBUG_CONTEXT_SAVE_AREA, "DBCXSVAR"},
    {GraphicsAllocation::AllocationType::DEBUG_SBA_TRACKING_BUFFER, "DBSBATRB"},
    {GraphicsAllocation::AllocationType::DEBUG_MODULE_AREA, "DBMDLARE"},
    {GraphicsAllocation::AllocationType::UNIFIED_SHARED_MEMORY, "USHRDMEM"},
    {GraphicsAllocation::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER, "GPUTSDBF"},
    {GraphicsAllocation::AllocationType::SW_TAG_BUFFER, "SWTAGBF"}};
class AllocationTypeTagString : public ::testing::TestWithParam<AllocationTypeTagTestCase> {};

TEST_P(AllocationTypeTagString, givenGraphicsAllocationTypeWhenCopyTagToStorageInfoThenCorrectTagIsReturned) {
    DebugManagerStateRestore restorer;
    StorageInfo storageInfo = {};
    auto input = GetParam();

    DebugManager.flags.EnableResourceTags.set(true);
    AppResourceHelper::copyResourceTagStr(storageInfo.resourceTag, input.type,
                                          sizeof(storageInfo.resourceTag));
    EXPECT_STREQ(storageInfo.resourceTag, input.str);
}

INSTANTIATE_TEST_CASE_P(AllAllocationTypesTag, AllocationTypeTagString, ::testing::ValuesIn(allocationTypeTagValues));
