/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

TEST(SubDevicesTest, givenDefaultConfigWhenCreateRootDeviceThenItDoesntContainSubDevices) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(0u, device->getNumGenericSubDevices());
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetWhenCreateRootDeviceThenItsSubdevicesHaveProperRootIdSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(2u, device->getNumSubDevices());
    EXPECT_EQ(0u, device->getRootDeviceIndex());

    EXPECT_EQ(0u, device->subdevices.at(0)->getRootDeviceIndex());
    EXPECT_EQ(0u, device->subdevices.at(0)->getSubDeviceIndex());

    EXPECT_EQ(0u, device->subdevices.at(1)->getRootDeviceIndex());
    EXPECT_EQ(1u, device->subdevices.at(1)->getSubDeviceIndex());
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetWhenCreateRootDeviceThenItContainsSubDevices) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(2u, device->getNumSubDevices());

    EXPECT_EQ(2u, device->getNumGenericSubDevices());
    EXPECT_EQ(0u, device->subdevices.at(0)->getNumGenericSubDevices());
    EXPECT_EQ(0u, device->subdevices.at(1)->getNumGenericSubDevices());
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenSubDeviceApiRefCountsAreChangedThenChangeIsPropagatedToRootDevice) {
    DebugManagerStateRestore restorer;
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    debugManager.flags.CreateMultipleSubDevices.set(2);
    UnitTestSetter::disableHeaplessStateInit(restorer);

    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    initPlatform();
    MockExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment();
    auto nonDefaultPlatform = std::make_unique<MockPlatform>(*executionEnvironment);

    nonDefaultPlatform->initializeWithNewDevices();
    auto device = nonDefaultPlatform->getClDevice(0);
    auto defaultDevice = platform()->getClDevice(0);

    auto subDevice = device->getSubDevice(1);
    auto baseDeviceApiRefCount = device->getRefApiCount();
    auto baseDeviceInternalRefCount = device->getRefInternalCount();
    auto baseSubDeviceApiRefCount = subDevice->getRefApiCount();
    auto baseSubDeviceInternalRefCount = subDevice->getRefInternalCount();
    auto baseDefaultDeviceApiRefCount = defaultDevice->getRefApiCount();
    auto baseDefaultDeviceInternalRefCount = defaultDevice->getRefInternalCount();

    subDevice->retainApi();
    EXPECT_EQ(baseDeviceApiRefCount, device->getRefApiCount());
    EXPECT_EQ(baseDeviceInternalRefCount + 1, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceApiRefCount + 1, subDevice->getRefApiCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount + 1, subDevice->getRefInternalCount());
    EXPECT_EQ(baseDefaultDeviceApiRefCount, defaultDevice->getRefApiCount());
    EXPECT_EQ(baseDefaultDeviceInternalRefCount, defaultDevice->getRefInternalCount());

    subDevice->releaseApi();
    EXPECT_EQ(baseDeviceApiRefCount, device->getRefApiCount());
    EXPECT_EQ(baseDeviceInternalRefCount, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceApiRefCount, subDevice->getRefApiCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());
    EXPECT_EQ(baseDefaultDeviceApiRefCount, defaultDevice->getRefApiCount());
    EXPECT_EQ(baseDefaultDeviceInternalRefCount, defaultDevice->getRefInternalCount());
}

TEST(SubDevicesTest, givenDeviceWithFlatOrCombinedHierarchyWhenSubDeviceApiRefCountsAreChangedThenChangeIsNotPropagatedToRootDevice) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    UnitTestSetter::disableHeaplessStateInit(restorer);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

    DeviceHierarchyMode deviceHierarchyModes[] = {DeviceHierarchyMode::flat, DeviceHierarchyMode::combined};
    for (auto deviceHierarchyMode : deviceHierarchyModes) {
        initPlatform();
        MockExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment();
        executionEnvironment->setDeviceHierarchyMode(deviceHierarchyMode);

        auto nonDefaultPlatform = std::make_unique<MockPlatform>(*executionEnvironment);
        nonDefaultPlatform->initializeWithNewDevices();
        auto device = nonDefaultPlatform->getClDevice(0);
        auto defaultDevice = platform()->getClDevice(0);

        auto subDevice = device->getSubDevice(1);
        auto baseDeviceApiRefCount = device->getRefApiCount();
        auto baseDeviceInternalRefCount = device->getRefInternalCount();
        auto baseSubDeviceApiRefCount = subDevice->getRefApiCount();
        auto baseSubDeviceInternalRefCount = subDevice->getRefInternalCount();
        auto baseDefaultDeviceApiRefCount = defaultDevice->getRefApiCount();
        auto baseDefaultDeviceInternalRefCount = defaultDevice->getRefInternalCount();

        subDevice->retainApi();
        EXPECT_EQ(baseDeviceApiRefCount, device->getRefApiCount());
        EXPECT_EQ(baseDeviceInternalRefCount, device->getRefInternalCount());
        EXPECT_EQ(baseSubDeviceApiRefCount, subDevice->getRefApiCount());
        EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());
        EXPECT_EQ(baseDefaultDeviceApiRefCount, defaultDevice->getRefApiCount());
        EXPECT_EQ(baseDefaultDeviceInternalRefCount, defaultDevice->getRefInternalCount());

        subDevice->releaseApi();
        EXPECT_EQ(baseDeviceApiRefCount, device->getRefApiCount());
        EXPECT_EQ(baseDeviceInternalRefCount, device->getRefInternalCount());
        EXPECT_EQ(baseSubDeviceApiRefCount, subDevice->getRefApiCount());
        EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());
        EXPECT_EQ(baseDefaultDeviceApiRefCount, defaultDevice->getRefApiCount());
        EXPECT_EQ(baseDefaultDeviceInternalRefCount, defaultDevice->getRefInternalCount());
    }
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenSubDeviceInternalRefCountsAreChangedThenChangeIsPropagatedToRootDevice) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);

    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->incRefInternal();
    auto subDevice = device->getSubDevice(0);

    auto baseDeviceInternalRefCount = device->getRefInternalCount();
    auto baseSubDeviceInternalRefCount = subDevice->getRefInternalCount();

    subDevice->incRefInternal();
    EXPECT_EQ(baseDeviceInternalRefCount + 1, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());

    device->incRefInternal();
    EXPECT_EQ(baseDeviceInternalRefCount + 2, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());

    subDevice->decRefInternal();
    EXPECT_EQ(baseDeviceInternalRefCount + 1, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());

    device->decRefInternal();
    EXPECT_EQ(baseDeviceInternalRefCount, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());
}

TEST(SubDevicesTest, givenClDeviceWithSubDevicesWhenSubDeviceInternalRefCountsAreChangedThenChangeIsPropagatedToRootDevice) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->incRefInternal();
    auto &subDevice = device->subDevices[0];

    auto baseDeviceInternalRefCount = device->getRefInternalCount();
    auto baseSubDeviceInternalRefCount = subDevice->getRefInternalCount();

    subDevice->incRefInternal();
    EXPECT_EQ(baseDeviceInternalRefCount + 1, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());

    device->incRefInternal();
    EXPECT_EQ(baseDeviceInternalRefCount + 2, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());

    subDevice->decRefInternal();
    EXPECT_EQ(baseDeviceInternalRefCount + 1, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());

    device->decRefInternal();
    EXPECT_EQ(baseDeviceInternalRefCount, device->getRefInternalCount());
    EXPECT_EQ(baseSubDeviceInternalRefCount, subDevice->getRefInternalCount());
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenSubDeviceCreationFailThenWholeDeviceIsDestroyed) {
    VariableBackup<bool> useMockSip(&MockSipData::useMockSip);
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(4);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.incRefInternal();
    executionEnvironment.memoryManager.reset(new FailMemoryManager(4, executionEnvironment));
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    if (gfxCoreHelper.isSipKernelAsHexadecimalArrayPreferred()) {
        useMockSip = true;
    }
    auto device = Device::create<RootDevice>(&executionEnvironment, 0u);
    EXPECT_EQ(nullptr, device);
}

TEST(SubDevicesTest, givenCreateMultipleRootDevicesFlagsEnabledWhenDevicesAreCreatedThenEachHasUniqueDeviceIndex) {

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(2);

    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    initPlatform();
    EXPECT_EQ(0u, platform()->getClDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(1u, platform()->getClDevice(1)->getRootDeviceIndex());
}

TEST(SubDevicesTest, givenRootDeviceWithSubDevicesWhenOsContextIsCreatedThenItsBitfieldBasesOnSubDevicesCount) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(2u, device->getNumSubDevices());

    uint32_t rootDeviceBitfield = 0b11;
    EXPECT_EQ(rootDeviceBitfield, static_cast<uint32_t>(device->getDefaultEngine().osContext->getDeviceBitfield().to_ulong()));
}

TEST(SubDevicesTest, givenSubDeviceWhenOsContextIsCreatedThenItsBitfieldBasesOnSubDeviceId) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(2u, device->getNumSubDevices());

    auto firstSubDevice = static_cast<SubDevice *>(device->subdevices.at(0));
    auto secondSubDevice = static_cast<SubDevice *>(device->subdevices.at(1));
    uint32_t firstSubDeviceMask = (1u << 0);
    uint32_t secondSubDeviceMask = (1u << 1);
    EXPECT_EQ(firstSubDeviceMask, static_cast<uint32_t>(firstSubDevice->getDefaultEngine().osContext->getDeviceBitfield().to_ulong()));
    EXPECT_EQ(secondSubDeviceMask, static_cast<uint32_t>(secondSubDevice->getDefaultEngine().osContext->getDeviceBitfield().to_ulong()));
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenGettingDeviceByIdThenGetCorrectSubDevice) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(2u, device->getNumSubDevices());
    EXPECT_EQ(device->subdevices.at(0), device->getSubDevice(0));
    EXPECT_EQ(device->subdevices.at(1), device->getSubDevice(1));
    EXPECT_THROW(device->getSubDevice(2), std::exception);
}

TEST(SubDevicesTest, givenSubDevicesWhenGettingDeviceByIdZeroThenGetThisSubDevice) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(2u, device->getNumSubDevices());
    auto subDevice = device->subdevices.at(0);

    if (subDevice->getNumSubDevices() > 0) {
        EXPECT_ANY_THROW(subDevice->getSubDevice(0)->getSubDevice(0));
    } else {
        EXPECT_ANY_THROW(subDevice->getSubDevice(0));
    }
}

TEST(RootDevicesTest, givenRootDeviceWithoutSubdevicesWhenCreateEnginesThenDeviceCreatesCorrectNumberOfEngines) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    auto executionEnvironment = new MockExecutionEnvironment;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockDevice device(executionEnvironment, 0);
    auto &gfxCoreHelper = device.getGfxCoreHelper();
    auto &gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(device.getRootDeviceEnvironment());

    EXPECT_EQ(0u, device.allEngines.size());
    device.createEngines();
    EXPECT_EQ(gpgpuEngines.size(), device.allEngines.size());
}

TEST(RootDevicesTest, givenRootDeviceWithSubdevicesWhenCreateEnginesThenDeviceCreatesSpecialEngine) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

    auto executionEnvironment = new MockExecutionEnvironment;
    MockDevice device(executionEnvironment, 0);
    EXPECT_EQ(0u, device.allEngines.size());
    device.createSubDevices();
    device.createEngines();
    EXPECT_EQ(2u, device.getNumGenericSubDevices());
    EXPECT_EQ(1u, device.allEngines.size());
}

TEST(SubDevicesTest, givenRootDeviceWithSubDevicesAndLocalMemoryWhenGettingGlobalMemorySizeThenSubDevicesReturnReducedAmountOfGlobalMemAllocSize) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.HBMSizePerTileInGigabytes.set(1);

    const uint32_t numSubDevices = 2u;
    UltDeviceFactory deviceFactory{1, numSubDevices};

    auto rootDevice = deviceFactory.rootDevices[0];

    auto totalGlobalMemorySize = rootDevice->getGlobalMemorySize(static_cast<uint32_t>(rootDevice->getDeviceBitfield().to_ulong()));
    auto expectedGlobalMemorySize = totalGlobalMemorySize / numSubDevices;

    for (const auto &subDevice : deviceFactory.subDevices) {
        auto mockSubDevice = static_cast<MockSubDevice *>(subDevice);
        auto subDeviceBitfield = static_cast<uint32_t>(mockSubDevice->getDeviceBitfield().to_ulong());
        EXPECT_EQ(expectedGlobalMemorySize, mockSubDevice->getGlobalMemorySize(subDeviceBitfield));
    }
}

TEST(SubDevicesTest, givenRootDeviceWithSubDevicesWithoutLocalMemoryWhenGettingGlobalMemorySizeThenSubDevicesReturnReducedAmountOfGlobalMemAllocSize) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    debugManager.flags.CreateMultipleSubDevices.set(2);

    const uint32_t numSubDevices = 2u;
    UltDeviceFactory deviceFactory{1, numSubDevices};

    auto rootDevice = deviceFactory.rootDevices[0];

    auto totalGlobalMemorySize = rootDevice->getGlobalMemorySize(static_cast<uint32_t>(rootDevice->getDeviceBitfield().to_ulong()));

    for (const auto &subDevice : deviceFactory.subDevices) {
        auto mockSubDevice = static_cast<MockSubDevice *>(subDevice);
        auto subDeviceBitfield = static_cast<uint32_t>(mockSubDevice->getDeviceBitfield().to_ulong());
        EXPECT_EQ(totalGlobalMemorySize, mockSubDevice->getGlobalMemorySize(subDeviceBitfield));
    }
}

struct DeviceTests : public ::testing::Test {
    bool createDevices(uint32_t numGenericSubDevices, uint32_t numCcs) {
        debugManager.flags.CreateMultipleSubDevices.set(numGenericSubDevices);

        auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0].get();
        auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
        hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = numCcs;
        hwInfo->featureTable.flags.ftrCCSNode = (numCcs > 0);
        hwInfo->capabilityTable.blitterOperationsSupported = true;
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        gfxCoreHelper.adjustDefaultEngineType(hwInfo, productHelper, rootDeviceEnvironment.ailConfiguration.get());

        if (!multiCcsDevice(rootDeviceEnvironment, numCcs)) {
            return false;
        }
        executionEnvironment->parseAffinityMask();
        deviceFactory = std::make_unique<UltDeviceFactory>(1, numGenericSubDevices, *executionEnvironment.release());
        rootDevice = deviceFactory->rootDevices[0];
        EXPECT_NE(nullptr, rootDevice);

        return true;
    }

    bool hasRootCsrOnly(MockDevice *device) {
        return ((device->allEngines.size() == 1) &&
                device->allEngines[0].osContext->isRootDevice());
    }

    template <typename MockDeviceT>
    bool hasAllEngines(MockDeviceT *device) {
        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto &gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());

        for (size_t i = 0; i < gpgpuEngines.size(); i++) {
            if (device->allEngines[i].getEngineType() != gpgpuEngines[i].first) {
                return false;
            }
        }

        return true;
    }

    bool multiCcsDevice(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t expectedNumCcs) {
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        auto gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment);

        uint32_t numCcs = 0;

        for (auto &engine : gpgpuEngines) {
            if (EngineHelpers::isCcs(engine.first) && (engine.second == EngineUsage::regular)) {
                numCcs++;
            }
        }

        return (numCcs == expectedNumCcs);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    MockDevice *rootDevice = nullptr;
};

TEST_F(DeviceTests, givenDebugFlagNotSetAndMoreThanOneCcsWhenCreatingRootDeviceWithoutGenericSubDevicesThenDontCreateSubDevice) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto &hwInfo = rootDevice->getHardwareInfo();

    EXPECT_EQ(ccsCount, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    EXPECT_EQ(0u, rootDevice->getNumSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));
    EXPECT_TRUE(hasAllEngines(rootDevice));
}

TEST_F(DeviceTests, givenDebugFlagSetAndZeroCcsesWhenCreatingRootDeviceWithoutGenericSubDevicesThenCreateSubDevice) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 0;

    EXPECT_TRUE(createDevices(genericDevicesCount, ccsCount));

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));
    EXPECT_TRUE(hasAllEngines(rootDevice));

    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumSubDevices());
    EXPECT_FALSE(rootDevice->getNearestGenericSubDevice(0)->isSubDevice());
}

TEST_F(DeviceTests, givenDebugFlagSetAndSingleCcsWhenCreatingRootDeviceWithoutGenericSubDevicesThenDontCreateSubDevice) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 1;

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));
    EXPECT_TRUE(hasAllEngines(rootDevice));

    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumSubDevices());
    EXPECT_FALSE(rootDevice->getNearestGenericSubDevice(0)->isSubDevice());
}

TEST_F(DeviceTests, givenDebugFlagSetWhenCreatingRootDeviceWithGenericSubDevicesAndZeroCcsesThenDontCreateSubDevice) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 0;

    EXPECT_TRUE(createDevices(genericDevicesCount, ccsCount));

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->allEngines[0].osContext->isRootDevice());
        EXPECT_EQ(0u, subDevice->getNumGenericSubDevices());
        EXPECT_EQ(0u, subDevice->getNumSubDevices());

        EXPECT_TRUE(hasAllEngines(subDevice));
    }
}

TEST_F(DeviceTests, givenDebugFlagSetWhenCreatingRootDeviceWithGenericSubDevicesAndSingleCcsThenDontCreateSubDevice) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 1;

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->allEngines[0].osContext->isRootDevice());
        EXPECT_EQ(0u, subDevice->getNumGenericSubDevices());
        EXPECT_EQ(0u, subDevice->getNumSubDevices());

        EXPECT_TRUE(hasAllEngines(subDevice));
    }
}

TEST_F(DeviceTests, givenSubDeviceWhenEngineCreationFailsThenReturnFalse) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 0;

    EXPECT_TRUE(createDevices(genericDevicesCount, ccsCount));

    auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(0));
    subDevice->failOnCreateEngine = true;

    EXPECT_FALSE(subDevice->createEngines());
}

TEST_F(DeviceTests, givenMultipleClSubDevicesWhenCallingGetSubDeviceThenReturnCorrectObject) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 2;

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto subDevice = rootDevice->subdevices[0];

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);
    auto clSubDevice = std::make_unique<ClDevice>(*subDevice, *clRootDevice, nullptr);

    EXPECT_EQ(clRootDevice->getSubDevice(0), clRootDevice->getNearestGenericSubDevice(0));
    EXPECT_EQ(clSubDevice.get(), clSubDevice->getNearestGenericSubDevice(0));
}

TEST_F(DeviceTests, givenAffinityMaskForSecondLevelOnSingleTileDeviceWithoutDebugFlagWhenCreatingThenDontEnableAllSubDevices) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    debugManager.flags.ZE_AFFINITY_MASK.set("0.0");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));

    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumSubDevices());
}

TEST_F(DeviceTests, givenAffinityMaskWhenCreatingClSubDevicesThenSkipDisabledDevices) {
    constexpr uint32_t genericDevicesCount = 3;
    constexpr uint32_t ccsCount = 1;

    debugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.2");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);

    ASSERT_EQ(2u, clRootDevice->getNumSubDevices());
    EXPECT_EQ(0b1u, clRootDevice->getSubDevice(0)->getDeviceBitfield().to_ulong());
    EXPECT_EQ(0b100u, clRootDevice->getSubDevice(1)->getDeviceBitfield().to_ulong());
}

TEST(SubDevicesTest, whenInitializeRootCsrThenDirectSubmissionIsNotInitialized) {
    auto device = std::make_unique<MockDevice>();
    device->initializeRootCommandStreamReceiver();

    auto csr = device->getEngine(1u).commandStreamReceiver;
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetWhenBindlessHeapHelperCreatedThenSubDeviceReturnRootDeviceMember) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->createBindlessHeapsHelper(device.get(), device->getNumGenericSubDevices() > 1);
    EXPECT_EQ(device->getBindlessHeapsHelper(), device->subdevices.at(0)->getBindlessHeapsHelper());
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagsEnabledWhenDevicesAreCreatedThenRootOsContextContainsAllSubDeviceBitfields) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    initPlatform();
    auto rootDevice = platform()->getClDevice(0);

    EXPECT_EQ(0b11ul, rootDevice->getDefaultEngine().osContext->getDeviceBitfield().to_ulong());
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetToOneWhenCreateRootDeviceThenDontCreateSubDevices) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(1);
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    initPlatform();
    auto rootDevice = static_cast<RootDevice *>(&platform()->getClDevice(0)->getDevice());
    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());
}
