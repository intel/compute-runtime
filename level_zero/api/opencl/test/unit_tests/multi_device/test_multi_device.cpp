/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture_extended.h"

#include "CL/cl.h"

#include <limits>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct MultiDeviceOclFixture : public L0::ult::MultiDeviceFixture {
    void setUp() {
        L0::ult::MultiDeviceFixture::setUp();
        platform = std::make_unique<Platform>(driverHandle->toHandle());
    }

    void tearDown() {
        platform.reset();
        L0::ult::MultiDeviceFixture::tearDown();
    }

    std::vector<cl_device_id> getClDeviceIds() {
        std::vector<cl_device_id> devices;
        for (auto &clDevice : platform->getDevices()) {
            devices.push_back(clDevice.get());
        }
        return devices;
    }

    std::unique_ptr<Platform> platform;
};

using LeoMultiDevicePlatformTests = Test<MultiDeviceOclFixture>;

TEST_F(LeoMultiDevicePlatformTests, givenMultiRootDeviceDriverWhenCreatingPlatformThenAllRootDevicesAreExposed) {
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(platform->getDevices().size()));
}

TEST_F(LeoMultiDevicePlatformTests, givenMultiRootDeviceDriverWhenCreatingPlatformThenRootDeviceIndicesAndBitfieldsAreGrouped) {
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(platform->getRootDeviceIndices().size()));
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(platform->getDeviceBitfields().size()));

    for (auto &clDevice : platform->getDevices()) {
        EXPECT_FALSE(clDevice->getIsSubdevice());
        EXPECT_TRUE(platform->getDeviceBitfields().contains(clDevice->getRootDeviceIndex()));
    }
}

TEST_F(LeoMultiDevicePlatformTests, givenMultiDevicePlatformWhenClGetDeviceIDsThenAllRootDevicesAreReturned) {
    cl_uint numDevices = 0;
    auto retVal = clGetDeviceIDs(platform.get(), CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numRootDevices, numDevices);

    std::vector<cl_device_id> devices(numDevices, nullptr);
    retVal = clGetDeviceIDs(platform.get(), CL_DEVICE_TYPE_GPU, numDevices, devices.data(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto device : devices) {
        EXPECT_NE(nullptr, device);
    }
}

TEST_F(LeoMultiDevicePlatformTests, givenDeviceTypeBitfieldContainingGpuWhenClGetDeviceIDsThenAllRootDevicesAreReturned) {
    const cl_device_type deviceTypes[] = {
        CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR,
        CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_DEFAULT,
        CL_DEVICE_TYPE_ALL};

    auto expectedDevices = getClDeviceIds();
    ASSERT_EQ(numRootDevices, static_cast<uint32_t>(expectedDevices.size()));

    for (auto deviceType : deviceTypes) {
        cl_uint numDevices = 0;
        auto retVal = clGetDeviceIDs(platform.get(), deviceType, 0, nullptr, &numDevices);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(numRootDevices, numDevices);

        std::vector<cl_device_id> devices(numRootDevices, nullptr);
        retVal = clGetDeviceIDs(platform.get(), deviceType, numRootDevices, devices.data(), nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(expectedDevices, devices);
    }
}

TEST_F(LeoMultiDevicePlatformTests, givenDeviceTypeDefaultOnlyWhenClGetDeviceIDsThenOnlyDefaultDeviceIsReturned) {
    ASSERT_GT(numRootDevices, 1u);
    auto dummyDevice = reinterpret_cast<cl_device_id>(0x1357);

    const cl_device_type deviceTypes[] = {
        CL_DEVICE_TYPE_DEFAULT,
        CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_DEFAULT};

    for (auto deviceType : deviceTypes) {
        cl_uint numDevices = 0;
        std::vector<cl_device_id> devices(numRootDevices, dummyDevice);

        auto retVal = clGetDeviceIDs(platform.get(), deviceType, numRootDevices, devices.data(), &numDevices);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(1u, numDevices);
        EXPECT_EQ(platform->getDevices()[0].get(), devices[0]);
        for (auto i = 1u; i < numRootDevices; i++) {
            EXPECT_EQ(dummyDevice, devices[i]);
        }
    }
}

using LeoMultiDeviceContextTests = Test<MultiDeviceOclFixture>;

TEST_F(LeoMultiDeviceContextTests, givenMultipleDevicesWhenCreatingContextThenItIsNotSingleDeviceAndGroupsAllRootDevices) {
    auto clDevices = getClDeviceIds();
    Context clContext(nullptr, nullptr, static_cast<cl_uint>(clDevices.size()), clDevices.data(), true);

    EXPECT_FALSE(clContext.isSingleDeviceContext());
    EXPECT_EQ(clDevices.size(), clContext.getClDevices().size());
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(clContext.getRootDeviceIndices().size()));
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(clContext.getDeviceBitfields().size()));
}

TEST_F(LeoMultiDeviceContextTests, givenMultiDeviceContextWhenGettingClDeviceByRootDeviceIndexThenMatchingDeviceIsReturned) {
    auto clDevices = getClDeviceIds();
    Context clContext(nullptr, nullptr, static_cast<cl_uint>(clDevices.size()), clDevices.data(), true);

    for (auto clDeviceId : clDevices) {
        auto clDevice = castToObject<ClDevice>(clDeviceId);
        EXPECT_EQ(clDevice, clContext.getClDeviceByRootDeviceIndex(clDevice->getRootDeviceIndex()));
    }
    EXPECT_EQ(nullptr, clContext.getClDeviceByRootDeviceIndex(std::numeric_limits<uint32_t>::max()));
}

TEST_F(LeoMultiDeviceContextTests, givenSingleDeviceWhenCreatingContextThenItIsSingleDeviceContextWithThatDeviceAsDefault) {
    auto clDevices = getClDeviceIds();
    Context clContext(nullptr, nullptr, 1u, clDevices.data(), true);

    EXPECT_TRUE(clContext.isSingleDeviceContext());
    EXPECT_EQ(1u, clContext.getRootDeviceIndices().size());
    EXPECT_EQ(clContext.getClDevices()[0]->getRootDeviceIndex(), clContext.getDefaultRootDeviceIndex());
}

TEST_F(LeoMultiDevicePlatformTests, givenDeviceHandleListWithDeviceOutsideContextWhenParsingMemoryPropertiesThenReturnsFalse) {
    auto clDeviceIds = getClDeviceIds();
    ASSERT_LE(2u, clDeviceIds.size());
    auto leoContext = std::make_unique<Context>(nullptr, context->toHandle(), 1, clDeviceIds.data(), true);
    cl_mem_properties_intel properties[] = {CL_MEM_DEVICE_HANDLE_LIST_KHR, reinterpret_cast<cl_mem_properties_intel>(clDeviceIds[1]),
                                            CL_MEM_DEVICE_HANDLE_LIST_END_KHR, 0};
    MemoryProperties memoryProperties{};
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocFlags = 0;

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocFlags,
                                                                 ClMemoryPropertiesHelper::ObjType::buffer, *leoContext));
}

using LeoMultiDevicePeerInfoTests = Test<MultiDeviceOclFixture>;

struct PeerInfoResult {
    cl_int retVal;
    cl_bool access;
    cl_bool atomics;
};

PeerInfoResult queryAccessAndAtomics(cl_device_id device, cl_device_id peerDevice) {
    PeerInfoResult result{CL_SUCCESS, CL_FALSE, CL_FALSE};
    auto clDevice = castToObject<ClDevice>(device);
    auto clPeerDevice = castToObject<ClDevice>(peerDevice);
    auto accessRetVal = clDevice->getPeerInfo(*clPeerDevice, CL_DEVICE_PEER_ACCESS_SUPPORTED_INTEL, sizeof(cl_bool), &result.access, nullptr);
    auto atomicsRetVal = clDevice->getPeerInfo(*clPeerDevice, CL_DEVICE_PEER_ATOMICS_SUPPORTED_INTEL, sizeof(cl_bool), &result.atomics, nullptr);
    result.retVal = (accessRetVal != CL_SUCCESS) ? accessRetVal : atomicsRetVal;
    return result;
}

TEST_F(LeoMultiDevicePeerInfoTests, givenPeerAccessForcedOnAndNoFabricWhenQueryingCrossRootPairThenAccessIsSupportedAndAtomicsAreNot) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(1);
    auto clDevices = getClDeviceIds();
    ASSERT_LE(2u, clDevices.size());

    for (size_t i = 0; i < clDevices.size(); i++) {
        for (size_t j = 0; j < clDevices.size(); j++) {
            if (i == j) {
                continue;
            }
            auto result = queryAccessAndAtomics(clDevices[i], clDevices[j]);
            EXPECT_EQ(CL_SUCCESS, result.retVal);
            EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), result.access) << i << " -> " << j;
            EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), result.atomics) << i << " -> " << j;
        }
    }
}

TEST_F(LeoMultiDevicePeerInfoTests, givenPeerAccessForcedOffWhenQueryingCrossRootPairThenNeitherAccessNorAtomicsAreSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(0);
    auto clDevices = getClDeviceIds();
    ASSERT_LE(2u, clDevices.size());

    auto result = queryAccessAndAtomics(clDevices[0], clDevices[1]);
    EXPECT_EQ(CL_SUCCESS, result.retVal);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), result.access);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), result.atomics);

    result = queryAccessAndAtomics(clDevices[1], clDevices[0]);
    EXPECT_EQ(CL_SUCCESS, result.retVal);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), result.access);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), result.atomics);
}

TEST_F(LeoMultiDevicePeerInfoTests, givenPeerAccessForcedOffWhenQueryingDeviceAgainstItselfThenAccessAndAtomicsAreSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(0);

    for (auto clDevice : getClDeviceIds()) {
        auto result = queryAccessAndAtomics(clDevice, clDevice);
        EXPECT_EQ(CL_SUCCESS, result.retVal);
        EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), result.access);
        EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), result.atomics);
    }
}

TEST_F(LeoMultiDevicePeerInfoTests, givenInvalidParamNameOrSizeWhenQueryingPeerInfoThenInvalidValueIsReturned) {
    auto device = castToObject<ClDevice>(getClDeviceIds()[0]);
    auto peerDevice = castToObject<ClDevice>(getClDeviceIds()[1]);
    cl_bool value = CL_FALSE;
    size_t retSize = 0;

    EXPECT_EQ(CL_INVALID_VALUE, device->getPeerInfo(*peerDevice, CL_DEVICE_TYPE, sizeof(value), &value, &retSize));
    EXPECT_EQ(0u, retSize);
    EXPECT_EQ(CL_INVALID_VALUE, device->getPeerInfo(*peerDevice, CL_DEVICE_PEER_ACCESS_SUPPORTED_INTEL, sizeof(value) - 1, &value, &retSize));
    EXPECT_EQ(0u, retSize);
}

TEST_F(LeoMultiDevicePeerInfoTests, givenNullValueWhenQueryingPeerInfoThenOnlySizeIsReturned) {
    auto device = castToObject<ClDevice>(getClDeviceIds()[0]);
    auto peerDevice = castToObject<ClDevice>(getClDeviceIds()[1]);
    size_t retSize = 0;

    EXPECT_EQ(CL_SUCCESS, device->getPeerInfo(*peerDevice, CL_DEVICE_PEER_ATOMICS_SUPPORTED_INTEL, 0, nullptr, &retSize));
    EXPECT_EQ(sizeof(cl_bool), retSize);
}

struct P2PPropertiesStub {
    static ze_result_t getP2PProperties(ze_device_handle_t hDevice, ze_device_handle_t hPeerDevice, ze_device_p2p_properties_t *pP2PProperties) {
        calls++;
        device = hDevice;
        peerDevice = hPeerDevice;
        stypeOnEntry = pP2PProperties->stype;
        flagsOnEntry = pP2PProperties->flags;
        pNextOnEntry = pP2PProperties->pNext;
        return result;
    }

    static inline uint32_t calls = 0;
    static inline ze_device_handle_t device = nullptr;
    static inline ze_device_handle_t peerDevice = nullptr;
    static inline ze_structure_type_t stypeOnEntry = ZE_STRUCTURE_TYPE_FORCE_UINT32;
    static inline ze_device_p2p_property_flags_t flagsOnEntry = 0;
    static inline void *pNextOnEntry = nullptr;
    static inline ze_result_t result = ZE_RESULT_SUCCESS;
};

TEST_F(LeoMultiDevicePeerInfoTests, givenLevelZeroQueryFailsWhenQueryingPeerInfoThenBothHandlesAreForwardedAndResultIsConverted) {
    VariableBackup<decltype(LEO::zeDeviceGetP2PProperties)> entryBackup{&LEO::zeDeviceGetP2PProperties, &P2PPropertiesStub::getP2PProperties};
    VariableBackup<uint32_t> callsBackup{&P2PPropertiesStub::calls, 0u};
    VariableBackup<ze_result_t> resultBackup{&P2PPropertiesStub::result, ZE_RESULT_ERROR_DEVICE_LOST};
    auto device = castToObject<ClDevice>(getClDeviceIds()[0]);
    auto peerDevice = castToObject<ClDevice>(getClDeviceIds()[1]);
    cl_bool value = CL_FALSE;

    EXPECT_EQ(CL_DEVICE_NOT_AVAILABLE, device->getPeerInfo(*peerDevice, CL_DEVICE_PEER_ACCESS_SUPPORTED_INTEL, sizeof(value), &value, nullptr));
    EXPECT_EQ(1u, P2PPropertiesStub::calls);
    EXPECT_EQ(device->getL0Handle(), P2PPropertiesStub::device);
    EXPECT_EQ(peerDevice->getL0Handle(), P2PPropertiesStub::peerDevice);
    EXPECT_EQ(ZE_STRUCTURE_TYPE_DEVICE_P2P_PROPERTIES, P2PPropertiesStub::stypeOnEntry);
    EXPECT_EQ(0u, P2PPropertiesStub::flagsOnEntry);
    EXPECT_EQ(nullptr, P2PPropertiesStub::pNextOnEntry);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
