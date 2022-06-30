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
    auto tag = AppResourceHelper::getResourceTagStr(static_cast<AllocationType>(999));
    EXPECT_STREQ(tag, "NOTFOUND");
}

TEST_F(AppResourceTests, givenGraphicsAllocationTypeWhenGettingResourceTagThenForEveryDefinedTypeProperTagExist) {
    auto firstTypeIdx = static_cast<int>(AllocationType::UNKNOWN);
    auto lastTypeIdx = static_cast<int>(AllocationType::COUNT);

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

AllocationTypeTagTestCase allocationTypeTagValues[static_cast<int>(AllocationType::COUNT)] = {
    {AllocationType::BUFFER, "BUFFER"},
    {AllocationType::BUFFER_HOST_MEMORY, "BFHSTMEM"},
    {AllocationType::COMMAND_BUFFER, "CMNDBUFF"},
    {AllocationType::CONSTANT_SURFACE, "CSNTSRFC"},
    {AllocationType::EXTERNAL_HOST_PTR, "EXHSTPTR"},
    {AllocationType::FILL_PATTERN, "FILPATRN"},
    {AllocationType::GLOBAL_SURFACE, "GLBLSRFC"},
    {AllocationType::IMAGE, "IMAGE"},
    {AllocationType::INDIRECT_OBJECT_HEAP, "INOBHEAP"},
    {AllocationType::INSTRUCTION_HEAP, "INSTHEAP"},
    {AllocationType::INTERNAL_HEAP, "INTLHEAP"},
    {AllocationType::INTERNAL_HOST_MEMORY, "INHSTMEM"},
    {AllocationType::KERNEL_ISA, "KERNLISA"},
    {AllocationType::KERNEL_ISA_INTERNAL, "KRLISAIN"},
    {AllocationType::LINEAR_STREAM, "LINRSTRM"},
    {AllocationType::MAP_ALLOCATION, "MAPALLOC"},
    {AllocationType::MCS, "MCS"},
    {AllocationType::PIPE, "PIPE"},
    {AllocationType::PREEMPTION, "PRMPTION"},
    {AllocationType::PRINTF_SURFACE, "PRNTSRFC"},
    {AllocationType::PRIVATE_SURFACE, "PRVTSRFC"},
    {AllocationType::PROFILING_TAG_BUFFER, "PROFTGBF"},
    {AllocationType::SCRATCH_SURFACE, "SCRHSRFC"},
    {AllocationType::WORK_PARTITION_SURFACE, "WRPRTSRF"},
    {AllocationType::SHARED_BUFFER, "SHRDBUFF"},
    {AllocationType::SHARED_CONTEXT_IMAGE, "SRDCXIMG"},
    {AllocationType::SHARED_IMAGE, "SHERDIMG"},
    {AllocationType::SHARED_RESOURCE_COPY, "SRDRSCCP"},
    {AllocationType::SURFACE_STATE_HEAP, "SRFCSTHP"},
    {AllocationType::SVM_CPU, "SVM_CPU"},
    {AllocationType::SVM_GPU, "SVM_GPU"},
    {AllocationType::SVM_ZERO_COPY, "SVM0COPY"},
    {AllocationType::TAG_BUFFER, "TAGBUFER"},
    {AllocationType::GLOBAL_FENCE, "GLBLFENC"},
    {AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, "TSPKTGBF"},
    {AllocationType::UNKNOWN, "UNKNOWN"},
    {AllocationType::WRITE_COMBINED, "WRTCMBND"},
    {AllocationType::RING_BUFFER, "RINGBUFF"},
    {AllocationType::SEMAPHORE_BUFFER, "SMPHRBUF"},
    {AllocationType::DEBUG_CONTEXT_SAVE_AREA, "DBCXSVAR"},
    {AllocationType::DEBUG_SBA_TRACKING_BUFFER, "DBSBATRB"},
    {AllocationType::DEBUG_MODULE_AREA, "DBMDLARE"},
    {AllocationType::UNIFIED_SHARED_MEMORY, "USHRDMEM"},
    {AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER, "GPUTSDBF"},
    {AllocationType::SW_TAG_BUFFER, "SWTAGBF"}};
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
