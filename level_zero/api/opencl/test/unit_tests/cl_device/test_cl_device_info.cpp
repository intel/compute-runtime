/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct ClDeviceInfoTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
    }

    cl_int querySize(cl_device_info paramName, size_t &retSize) {
        retSize = 0;
        return clDevice->getDeviceInfo(paramName, 0, nullptr, &retSize);
    }

    ClDevice *clDevice = nullptr;
    std::array<uint8_t, 8192> storage{};
};

const cl_device_info unconditionalParams[] = {
    CL_DEVICE_ADDRESS_BITS,
    CL_DEVICE_ATOMIC_FENCE_CAPABILITIES,
    CL_DEVICE_ATOMIC_MEMORY_CAPABILITIES,
    CL_DEVICE_AVAILABLE,
    CL_DEVICE_AVC_ME_SUPPORTS_PREEMPTION_INTEL,
    CL_DEVICE_AVC_ME_SUPPORTS_TEXTURE_SAMPLER_USE_INTEL,
    CL_DEVICE_AVC_ME_VERSION_INTEL,
    CL_DEVICE_BUILT_IN_KERNELS,
    CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION,
    CL_DEVICE_COMPILER_AVAILABLE,
    CL_DEVICE_CROSS_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
    CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES,
    CL_DEVICE_DEVICE_MEM_CAPABILITIES_INTEL,
    CL_DEVICE_DOUBLE_FP_ATOMIC_CAPABILITIES_EXT,
    CL_DEVICE_DOUBLE_FP_CONFIG,
    CL_DEVICE_DRIVER_VERSION_INTEL,
    CL_DEVICE_ENDIAN_LITTLE,
    CL_DEVICE_ERROR_CORRECTION_SUPPORT,
    CL_DEVICE_EU_THREAD_COUNTS_INTEL,
    CL_DEVICE_EXECUTION_CAPABILITIES,
    CL_DEVICE_EXTENSIONS,
    CL_DEVICE_EXTENSIONS_WITH_VERSION,
    CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR,
    CL_DEVICE_FEATURE_CAPABILITIES_INTEL,
    CL_DEVICE_GENERIC_ADDRESS_SPACE_SUPPORT,
    CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,
    CL_DEVICE_GLOBAL_MEM_CACHE_SIZE,
    CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,
    CL_DEVICE_GLOBAL_MEM_SIZE,
    CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE,
    CL_DEVICE_HALF_FP_ATOMIC_CAPABILITIES_EXT,
    CL_DEVICE_HALF_FP_CONFIG,
    CL_DEVICE_HOST_MEM_CAPABILITIES_INTEL,
    CL_DEVICE_HOST_UNIFIED_MEMORY,
    CL_DEVICE_ID_INTEL,
    CL_DEVICE_IL_VERSION,
    CL_DEVICE_ILS_WITH_VERSION,
    CL_DEVICE_IMAGE_SUPPORT,
    CL_DEVICE_INTEGER_DOT_PRODUCT_ACCELERATION_PROPERTIES_4x8BIT_PACKED_KHR,
    CL_DEVICE_INTEGER_DOT_PRODUCT_ACCELERATION_PROPERTIES_8BIT_KHR,
    CL_DEVICE_INTEGER_DOT_PRODUCT_CAPABILITIES_KHR,
    CL_DEVICE_IP_VERSION_INTEL,
    CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED,
    CL_DEVICE_LINKER_AVAILABLE,
    CL_DEVICE_LOCAL_MEM_SIZE,
    CL_DEVICE_LOCAL_MEM_TYPE,
    CL_DEVICE_MAX_CLOCK_FREQUENCY,
    CL_DEVICE_MAX_COMPUTE_UNITS,
    CL_DEVICE_MAX_CONSTANT_ARGS,
    CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,
    CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE,
    CL_DEVICE_MAX_MEM_ALLOC_SIZE,
    CL_DEVICE_MAX_NUM_SUB_GROUPS,
    CL_DEVICE_MAX_ON_DEVICE_EVENTS,
    CL_DEVICE_MAX_ON_DEVICE_QUEUES,
    CL_DEVICE_MAX_PARAMETER_SIZE,
    CL_DEVICE_MAX_PIPE_ARGS,
    CL_DEVICE_MAX_SAMPLERS,
    CL_DEVICE_MAX_WORK_GROUP_SIZE,
    CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
    CL_DEVICE_MAX_WORK_ITEM_SIZES,
    CL_DEVICE_MEM_BASE_ADDR_ALIGN,
    CL_DEVICE_ME_VERSION_INTEL,
    CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,
    CL_DEVICE_NAME,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_INT,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG,
    CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT,
    CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT,
    CL_DEVICE_NUMERIC_VERSION,
    CL_DEVICE_NUM_EUS_PER_SUB_SLICE_INTEL,
    CL_DEVICE_NUM_SLICES_INTEL,
    CL_DEVICE_NUM_SUB_SLICES_PER_SLICE_INTEL,
    CL_DEVICE_NUM_THREADS_PER_EU_INTEL,
    CL_DEVICE_OPENCL_C_ALL_VERSIONS,
    CL_DEVICE_OPENCL_C_FEATURES,
    CL_DEVICE_OPENCL_C_VERSION,
    CL_DEVICE_PARENT_DEVICE,
    CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
    CL_DEVICE_PARTITION_MAX_SUB_DEVICES,
    CL_DEVICE_PARTITION_PROPERTIES,
    CL_DEVICE_PARTITION_TYPE,
    CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS,
    CL_DEVICE_PIPE_MAX_PACKET_SIZE,
    CL_DEVICE_PIPE_SUPPORT,
    CL_DEVICE_PLATFORM,
    CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_INTEROP_USER_SYNC,
    CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
    CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
    CL_DEVICE_PRINTF_BUFFER_SIZE,
    CL_DEVICE_PROFILE,
    CL_DEVICE_PROFILING_TIMER_RESOLUTION,
    CL_DEVICE_QUEUE_FAMILY_PROPERTIES_INTEL,
    CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE,
    CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,
    CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES,
    CL_DEVICE_QUEUE_ON_HOST_PROPERTIES,
    CL_DEVICE_REFERENCE_COUNT,
    CL_DEVICE_SHARED_SYSTEM_MEM_CAPABILITIES_INTEL,
    CL_DEVICE_SINGLE_DEVICE_SHARED_MEM_CAPABILITIES_INTEL,
    CL_DEVICE_SINGLE_FP_ATOMIC_CAPABILITIES_EXT,
    CL_DEVICE_SINGLE_FP_CONFIG,
    CL_DEVICE_SLICE_COUNT_INTEL,
    CL_DEVICE_SPIRV_CAPABILITIES_KHR,
    CL_DEVICE_SPIRV_EXTENDED_INSTRUCTION_SETS_KHR,
    CL_DEVICE_SPIRV_EXTENSIONS_KHR,
    CL_DEVICE_SPIR_VERSIONS,
    CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS,
    CL_DEVICE_SUB_GROUP_SIZES_INTEL,
    CL_DEVICE_SUPPORTED_THREAD_ARBITRATION_POLICY_INTEL,
    CL_DEVICE_SVM_CAPABILITIES,
    CL_DEVICE_TYPE,
    CL_DEVICE_UUID_KHR,
    CL_DEVICE_VENDOR,
    CL_DEVICE_VENDOR_ID,
    CL_DEVICE_VERSION,
    CL_DEVICE_WORK_GROUP_COLLECTIVE_FUNCTIONS_SUPPORT,
    CL_DRIVER_UUID_KHR,
    CL_DRIVER_VERSION,
    CL_L0_DEVICE_HANDLE};

const cl_device_info imageParams[] = {
    CL_DEVICE_IMAGE2D_MAX_HEIGHT,
    CL_DEVICE_IMAGE2D_MAX_WIDTH,
    CL_DEVICE_IMAGE3D_MAX_DEPTH,
    CL_DEVICE_IMAGE3D_MAX_HEIGHT,
    CL_DEVICE_IMAGE3D_MAX_WIDTH,
    CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT,
    CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,
    CL_DEVICE_IMAGE_MAX_BUFFER_SIZE,
    CL_DEVICE_IMAGE_PITCH_ALIGNMENT,
    CL_DEVICE_MAX_READ_IMAGE_ARGS,
    CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS,
    CL_DEVICE_MAX_WRITE_IMAGE_ARGS};

TEST_F(ClDeviceInfoTest, givenUnconditionalParamsWhenQueryingSizeThenAllSucceed) {
    for (auto paramName : unconditionalParams) {
        size_t retSize = 0;
        EXPECT_EQ(CL_SUCCESS, querySize(paramName, retSize)) << "size query failed for param 0x" << std::hex << paramName;
    }
}

TEST_F(ClDeviceInfoTest, givenUnconditionalParamsWhenQueryingValueWithExactSizeThenAllSucceed) {
    for (auto paramName : unconditionalParams) {
        size_t retSize = 0;
        ASSERT_EQ(CL_SUCCESS, querySize(paramName, retSize));
        if (retSize == 0) {
            continue;
        }
        ASSERT_LE(retSize, storage.size()) << "param 0x" << std::hex << paramName << " needs a bigger buffer";
        EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(paramName, retSize, storage.data(), nullptr))
            << "value query failed for param 0x" << std::hex << paramName;
    }
}

TEST_F(ClDeviceInfoTest, givenImageParamsWhenQueryingThenAllSucceed) {
    for (auto paramName : imageParams) {
        size_t retSize = 0;
        ASSERT_EQ(CL_SUCCESS, querySize(paramName, retSize)) << "size query failed for param 0x" << std::hex << paramName;
        EXPECT_NE(0u, retSize) << "param 0x" << std::hex << paramName;
        EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(paramName, retSize, storage.data(), nullptr))
            << "value query failed for param 0x" << std::hex << paramName;
    }
}

TEST_F(ClDeviceInfoTest, givenUnknownParamWhenGetDeviceInfoThenReturnsCLInvalidValue) {
    size_t retSize = 123u;
    EXPECT_EQ(CL_INVALID_VALUE, clDevice->getDeviceInfo(0xDEAD0000u, 0, nullptr, &retSize));
    EXPECT_EQ(123u, retSize);
}

TEST_F(ClDeviceInfoTest, givenTooSmallBufferWhenGetDeviceInfoThenReturnsCLInvalidValue) {
    const cl_device_info params[] = {CL_DEVICE_MAX_COMPUTE_UNITS, CL_DEVICE_GLOBAL_MEM_SIZE,
                                     CL_DEVICE_MAX_WORK_ITEM_SIZES, CL_DEVICE_NAME, CL_DEVICE_UUID_KHR};

    for (auto paramName : params) {
        size_t retSize = 0;
        ASSERT_EQ(CL_SUCCESS, querySize(paramName, retSize));
        ASSERT_GT(retSize, 0u);
        EXPECT_EQ(CL_INVALID_VALUE, clDevice->getDeviceInfo(paramName, retSize - 1, storage.data(), nullptr))
            << "param 0x" << std::hex << paramName;
    }
}

TEST_F(ClDeviceInfoTest, givenPlatformParamWhenGetDeviceInfoThenReturnsOwningPlatform) {
    cl_platform_id queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_PLATFORM, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(cl_platform_id), retSize);
    EXPECT_EQ(static_cast<cl_platform_id>(platform), queried);
}

TEST_F(ClDeviceInfoTest, givenL0HandleParamWhenGetDeviceInfoThenReturnsUnderlyingL0Device) {
    ze_device_handle_t queried = nullptr;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_L0_DEVICE_HANDLE, sizeof(queried), &queried, nullptr));
    EXPECT_EQ(clDevice->getL0Handle(), queried);
}

TEST_F(ClDeviceInfoTest, givenReferenceCountParamWhenGetDeviceInfoThenReturnsOne) {
    cl_uint refCount = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_REFERENCE_COUNT, sizeof(refCount), &refCount, nullptr));
    EXPECT_EQ(1u, refCount);
}

TEST_F(ClDeviceInfoTest, givenMaxWorkItemSizesParamWhenGetDeviceInfoThenReturnsThreeDimensions) {
    size_t retSize = 0;
    size_t sizes[3] = {};
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(sizes), sizes, &retSize));
    EXPECT_EQ(3u * sizeof(size_t), retSize);
    const auto *expected = clDevice->getSharedDeviceInfo().maxWorkItemSizes;
    EXPECT_EQ(expected[0], sizes[0]);
    EXPECT_EQ(expected[1], sizes[1]);
    EXPECT_EQ(expected[2], sizes[2]);
}

TEST_F(ClDeviceInfoTest, givenSubGroupSizesParamWhenGetDeviceInfoThenSizeMatchesDeviceMaxSubGroups) {
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, querySize(CL_DEVICE_SUB_GROUP_SIZES_INTEL, retSize));
    EXPECT_EQ(clDevice->getSharedDeviceInfo().maxSubGroups.size() * sizeof(size_t), retSize);
}

TEST_F(ClDeviceInfoTest, givenEuThreadCountsParamWhenGetDeviceInfoThenSizeMatchesDeviceConfigs) {
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, querySize(CL_DEVICE_EU_THREAD_COUNTS_INTEL, retSize));
    EXPECT_EQ(clDevice->getSharedDeviceInfo().threadsPerEUConfigs.size() * sizeof(uint32_t), retSize);
}

TEST_F(ClDeviceInfoTest, givenDeviceEnqueueCapabilitiesQueriedAsBoolWhenGetDeviceInfoThenReturnsFalse) {
    cl_bool capabilities = CL_TRUE;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES, sizeof(cl_bool),
                                                  &capabilities, &retSize));
    EXPECT_EQ(sizeof(cl_bool), retSize);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), capabilities);
}

TEST_F(ClDeviceInfoTest, givenDeviceIdParamWhenGetDeviceInfoThenReturnsHardwareDeviceId) {
    cl_uint deviceId = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_ID_INTEL, sizeof(deviceId), &deviceId, nullptr));
    EXPECT_EQ(static_cast<cl_uint>(clDevice->getHardwareInfo().platform.usDeviceID), deviceId);
}

TEST_F(ClDeviceInfoTest, givenEusPerSubSliceParamWhenGetDeviceInfoThenReturnsHardwareValue) {
    cl_uint eus = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_NUM_EUS_PER_SUB_SLICE_INTEL, sizeof(eus), &eus, nullptr));
    EXPECT_EQ(clDevice->getHardwareInfo().gtSystemInfo.MaxEuPerSubSlice, eus);
}

TEST_F(ClDeviceInfoTest, givenThreadsPerEuParamWhenGetDeviceInfoThenReturnsThreadCountOverEuCount) {
    const auto &gtSysInfo = clDevice->getHardwareInfo().gtSystemInfo;
    ASSERT_NE(0u, gtSysInfo.EUCount);

    cl_uint threads = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_NUM_THREADS_PER_EU_INTEL, sizeof(threads), &threads, nullptr));
    EXPECT_EQ(gtSysInfo.ThreadCount / gtSysInfo.EUCount, threads);
}

TEST_F(ClDeviceInfoTest, givenNumSlicesParamWhenGetDeviceInfoThenScalesWithSubDevices) {
    const auto &gtSysInfo = clDevice->getHardwareInfo().gtSystemInfo;
    const auto expected = static_cast<cl_uint>(gtSysInfo.SliceCount * std::max(clDevice->getDevice().getNumGenericSubDevices(), 1u));

    cl_uint slices = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_NUM_SLICES_INTEL, sizeof(slices), &slices, nullptr));
    EXPECT_EQ(expected, slices);
}

TEST_F(ClDeviceInfoTest, givenFeatureCapabilitiesParamWhenGetDeviceInfoThenDp4aIsAlwaysReported) {
    cl_device_feature_capabilities_intel capabilities = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_FEATURE_CAPABILITIES_INTEL, sizeof(capabilities),
                                                  &capabilities, nullptr));
    EXPECT_NE(0u, capabilities & CL_DEVICE_FEATURE_FLAG_DP4A_INTEL);
}

TEST_F(ClDeviceInfoTest, givenDeviceUuidParamWhenGetDeviceInfoThenReturnsUuidSizedValue) {
    size_t retSize = 0;
    std::array<uint8_t, CL_UUID_SIZE_KHR> uuid{};
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_UUID_KHR, uuid.size(), uuid.data(), &retSize));
    EXPECT_EQ(static_cast<size_t>(CL_UUID_SIZE_KHR), retSize);
    EXPECT_TRUE(std::any_of(uuid.begin(), uuid.end(), [](uint8_t byte) { return byte != 0u; }));
}

TEST_F(ClDeviceInfoTest, givenDriverUuidParamWhenGetDeviceInfoThenReturnsUuidSizedValue) {
    size_t retSize = 0;
    std::array<uint8_t, CL_UUID_SIZE_KHR> uuid{};
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DRIVER_UUID_KHR, uuid.size(), uuid.data(), &retSize));
    EXPECT_EQ(static_cast<size_t>(CL_UUID_SIZE_KHR), retSize);
}

TEST_F(ClDeviceInfoTest, givenPartitionPropertiesWhenDeviceIsNotPartitionableThenSizeIsSingleElement) {
    const auto &deviceInfo = clDevice->getDeviceInfo();
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, querySize(CL_DEVICE_PARTITION_PROPERTIES, retSize));

    if (deviceInfo.partitionProperties[0] == 0) {
        EXPECT_EQ(sizeof(deviceInfo.partitionProperties[0]), retSize);
    } else {
        EXPECT_EQ(sizeof(deviceInfo.partitionProperties), retSize);
    }
}

TEST_F(ClDeviceInfoTest, givenPartitionTypeWhenDeviceIsNotPartitionedThenSizeIsSingleElement) {
    const auto &deviceInfo = clDevice->getDeviceInfo();
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, querySize(CL_DEVICE_PARTITION_TYPE, retSize));

    if (deviceInfo.partitionType[0] == 0) {
        EXPECT_EQ(sizeof(deviceInfo.partitionType[0]), retSize);
    } else {
        EXPECT_EQ(sizeof(deviceInfo.partitionType), retSize);
    }
}

TEST_F(ClDeviceInfoTest, givenSimultaneousInteropParamsWhenQueryingThenNumAndListAgree) {
    size_t numSize = 0;
    const auto numResult = clDevice->getDeviceInfo(CL_DEVICE_NUM_SIMULTANEOUS_INTEROPS_INTEL, 0, nullptr, &numSize);
    size_t listSize = 0;
    const auto listResult = clDevice->getDeviceInfo(CL_DEVICE_SIMULTANEOUS_INTEROPS_INTEL, 0, nullptr, &listSize);

    EXPECT_EQ(numResult, listResult);

    if (numResult == CL_SUCCESS) {
        cl_uint numInterops = 0;
        EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_NUM_SIMULTANEOUS_INTEROPS_INTEL, sizeof(numInterops),
                                                      &numInterops, nullptr));
        EXPECT_EQ(1u, numInterops);
        EXPECT_GT(listSize, sizeof(cl_uint));
        EXPECT_EQ(0u, listSize % sizeof(cl_uint));

        ASSERT_LE(listSize, storage.size());
        EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_SIMULTANEOUS_INTEROPS_INTEL, listSize, storage.data(), nullptr));
    } else {
        EXPECT_EQ(CL_INVALID_VALUE, numResult);
        EXPECT_EQ(CL_INVALID_VALUE, listResult);
    }
}

TEST_F(ClDeviceInfoTest, givenCommandBufferDisabledWhenQueryingCommandBufferParamsThenReturnsCLInvalidValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableClKhrCommandBuffer.set(0);

    const cl_device_info params[] = {CL_DEVICE_COMMAND_BUFFER_CAPABILITIES_KHR,
                                     CL_DEVICE_COMMAND_BUFFER_SUPPORTED_QUEUE_PROPERTIES_KHR,
                                     CL_DEVICE_COMMAND_BUFFER_REQUIRED_QUEUE_PROPERTIES_KHR};

    for (auto paramName : params) {
        size_t retSize = 0;
        EXPECT_EQ(CL_INVALID_VALUE, clDevice->getDeviceInfo(paramName, 0, nullptr, &retSize))
            << "param 0x" << std::hex << paramName;
    }
}

TEST_F(ClDeviceInfoTest, givenCommandBufferEnabledWhenQueryingCommandBufferParamsThenReturnsEmptyBitfields) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableClKhrCommandBuffer.set(1);

    const cl_device_info params[] = {CL_DEVICE_COMMAND_BUFFER_CAPABILITIES_KHR,
                                     CL_DEVICE_COMMAND_BUFFER_SUPPORTED_QUEUE_PROPERTIES_KHR,
                                     CL_DEVICE_COMMAND_BUFFER_REQUIRED_QUEUE_PROPERTIES_KHR};

    for (auto paramName : params) {
        cl_bitfield bitfield = 0xFFu;
        size_t retSize = 0;
        EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(paramName, sizeof(bitfield), &bitfield, &retSize))
            << "param 0x" << std::hex << paramName;
        EXPECT_EQ(sizeof(cl_bitfield), retSize);
        EXPECT_EQ(0u, bitfield);
    }
}

TEST_F(ClDeviceInfoTest, givenPlanarYuvParamsWhenQueryingThenResultFollowsNv12ExtensionSupport) {
    const auto &deviceInfo = clDevice->getDeviceInfo();

    size_t width = 0;
    size_t height = 0;
    const auto widthResult = clDevice->getDeviceInfo(CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL, sizeof(width), &width, nullptr);
    const auto heightResult = clDevice->getDeviceInfo(CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL, sizeof(height), &height, nullptr);

    if (deviceInfo.nv12Extension) {
        EXPECT_EQ(CL_SUCCESS, widthResult);
        EXPECT_EQ(CL_SUCCESS, heightResult);
        EXPECT_EQ(deviceInfo.planarYuvMaxWidth, width);
        EXPECT_EQ(deviceInfo.planarYuvMaxHeight, height);
    } else {
        EXPECT_EQ(CL_INVALID_VALUE, widthResult);
        EXPECT_EQ(CL_INVALID_VALUE, heightResult);
    }
}

TEST_F(ClDeviceInfoTest, givenNamedStringParamsWhenGetDeviceInfoThenValuesAreNullTerminated) {
    const cl_device_info stringParams[] = {CL_DEVICE_NAME, CL_DEVICE_VENDOR, CL_DEVICE_VERSION, CL_DRIVER_VERSION,
                                           CL_DEVICE_PROFILE, CL_DEVICE_OPENCL_C_VERSION, CL_DEVICE_EXTENSIONS,
                                           CL_DEVICE_IL_VERSION, CL_DEVICE_SPIR_VERSIONS,
                                           CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED, CL_DEVICE_BUILT_IN_KERNELS};

    for (auto paramName : stringParams) {
        size_t retSize = 0;
        ASSERT_EQ(CL_SUCCESS, querySize(paramName, retSize)) << "param 0x" << std::hex << paramName;
        ASSERT_GT(retSize, 0u) << "param 0x" << std::hex << paramName;
        ASSERT_LE(retSize, storage.size());

        std::vector<char> value(retSize, 'x');
        ASSERT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(paramName, retSize, value.data(), nullptr));
        EXPECT_EQ('\0', value[retSize - 1]) << "param 0x" << std::hex << paramName << " is not null terminated";
    }
}

TEST_F(ClDeviceInfoTest, givenDeviceVersionWhenGetDeviceInfoThenLeoMarkerIsReported) {
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, querySize(CL_DEVICE_VERSION, retSize));
    std::vector<char> version(retSize, '\0');
    ASSERT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_VERSION, retSize, version.data(), nullptr));
    EXPECT_NE(nullptr, strstr(version.data(), "LEO"));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
