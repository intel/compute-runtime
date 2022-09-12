/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
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
    if (device->getNumSubDevices() > 0) {
        EXPECT_TRUE(device->getSubDevice(0)->isEngineInstanced());
    }
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetWhenCreateRootDeviceThenItsSubdevicesHaveProperRootIdSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
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
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(2u, device->getNumSubDevices());

    EXPECT_EQ(2u, device->getNumGenericSubDevices());
    EXPECT_EQ(0u, device->subdevices.at(0)->getNumGenericSubDevices());
    EXPECT_EQ(0u, device->subdevices.at(1)->getNumGenericSubDevices());
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenSubDeviceApiRefCountsAreChangedThenChangeIsPropagatedToRootDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    initPlatform();
    auto nonDefaultPlatform = std::make_unique<MockPlatform>(*platform()->peekExecutionEnvironment());
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

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenSubDeviceInternalRefCountsAreChangedThenChangeIsPropagatedToRootDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
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
    DebugManager.flags.CreateMultipleSubDevices.set(2);
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
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.incRefInternal();
    executionEnvironment.memoryManager.reset(new FailMemoryManager(4, executionEnvironment));
    auto device = Device::create<RootDevice>(&executionEnvironment, 0u);
    EXPECT_EQ(nullptr, device);
}

TEST(SubDevicesTest, givenCreateMultipleRootDevicesFlagsEnabledWhenDevicesAreCreatedThenEachHasUniqueDeviceIndex) {

    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(2);

    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    initPlatform();
    EXPECT_EQ(0u, platform()->getClDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(1u, platform()->getClDevice(1)->getRootDeviceIndex());
}

TEST(SubDevicesTest, givenRootDeviceWithSubDevicesWhenOsContextIsCreatedThenItsBitfieldBasesOnSubDevicesCount) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(2u, device->getNumSubDevices());

    uint32_t rootDeviceBitfield = 0b11;
    EXPECT_EQ(rootDeviceBitfield, static_cast<uint32_t>(device->getDefaultEngine().osContext->getDeviceBitfield().to_ulong()));
}

TEST(SubDevicesTest, givenSubDeviceWhenOsContextIsCreatedThenItsBitfieldBasesOnSubDeviceId) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
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
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(2u, device->getNumSubDevices());
    EXPECT_EQ(device->subdevices.at(0), device->getSubDevice(0));
    EXPECT_EQ(device->subdevices.at(1), device->getSubDevice(1));
    EXPECT_THROW(device->getSubDevice(2), std::exception);
}

TEST(SubDevicesTest, givenSubDevicesWhenGettingDeviceByIdZeroThenGetThisSubDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
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
    auto &gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

    auto executionEnvironment = new MockExecutionEnvironment;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockDevice device(executionEnvironment, 0);
    EXPECT_EQ(0u, device.allEngines.size());
    device.createEngines();
    EXPECT_EQ(gpgpuEngines.size(), device.allEngines.size());
}

TEST(RootDevicesTest, givenRootDeviceWithSubdevicesWhenCreateEnginesThenDeviceCreatesSpecialEngine) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
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
    DebugManager.flags.EnableLocalMemory.set(1);
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    DebugManager.flags.HBMSizePerTileInGigabytes.set(1);

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
    DebugManager.flags.EnableLocalMemory.set(0);
    DebugManager.flags.CreateMultipleSubDevices.set(2);

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

TEST(SubDevicesTest, whenCreatingEngineInstancedSubDeviceThenSetCorrectSubdeviceIndex) {
    class MyRootDevice : public RootDevice {
      public:
        using RootDevice::createEngineInstancedSubDevice;
        using RootDevice::RootDevice;
    };

    auto executionEnvironment = new MockExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    DeviceFactory::createMemoryManagerFunc(*executionEnvironment);

    auto rootDevice = std::unique_ptr<MyRootDevice>(Device::create<MyRootDevice>(executionEnvironment, 0));

    auto subDevice = std::unique_ptr<SubDevice>(rootDevice->createEngineInstancedSubDevice(1, defaultHwInfo->capabilityTable.defaultEngineType));

    ASSERT_NE(nullptr, subDevice.get());

    EXPECT_EQ(2u, subDevice->getDeviceBitfield().to_ulong());
}

struct EngineInstancedDeviceTests : public ::testing::Test {
    bool createDevices(uint32_t numGenericSubDevices, uint32_t numCcs) {
        DebugManager.flags.CreateMultipleSubDevices.set(numGenericSubDevices);

        auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = numCcs;
        hwInfo->featureTable.flags.ftrCCSNode = (numCcs > 0);
        hwInfo->capabilityTable.blitterOperationsSupported = true;
        HwHelper::get(hwInfo->platform.eRenderCoreFamily).adjustDefaultEngineType(hwInfo);

        if (!multiCcsDevice(*hwInfo, numCcs)) {
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

    bool isEngineInstanced(MockSubDevice *subDevice, aub_stream::EngineType engineType, uint32_t subDeviceIndex, DeviceBitfield deviceBitfield) {
        bool isEngineInstanced = !subDevice->allEngines[0].osContext->isRootDevice();
        isEngineInstanced &= subDevice->engineInstanced;
        isEngineInstanced &= (subDevice->getNumGenericSubDevices() == 0);
        isEngineInstanced &= (subDevice->getNumSubDevices() == 0);
        isEngineInstanced &= (engineType == subDevice->engineInstancedType);
        isEngineInstanced &= (subDeviceIndex == subDevice->getSubDeviceIndex());
        isEngineInstanced &= (deviceBitfield == subDevice->getDeviceBitfield());
        isEngineInstanced &= (subDevice->getAllEngines().size() == 1);

        return isEngineInstanced;
    }

    template <typename MockDeviceT>
    bool hasAllEngines(MockDeviceT *device) {
        auto &hwInfo = device->getHardwareInfo();
        auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

        for (size_t i = 0; i < gpgpuEngines.size(); i++) {
            if (device->allEngines[i].getEngineType() != gpgpuEngines[i].first) {
                return false;
            }
        }

        return true;
    }

    bool multiCcsDevice(const HardwareInfo &hwInfo, uint32_t expectedNumCcs) {
        auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

        uint32_t numCcs = 0;

        for (auto &engine : gpgpuEngines) {
            if (EngineHelpers::isCcs(engine.first) && (engine.second == EngineUsage::Regular)) {
                numCcs++;
            }
        }

        return (numCcs == expectedNumCcs);
    }

    template <typename MockDeviceT>
    bool hasEngineInstancedEngines(MockDeviceT *device, aub_stream::EngineType engineType) {
        if (device->getAllEngines().size() != 1) {
            return false;
        }

        OsContext *defaultOsContext = device->getDefaultEngine().osContext;
        EXPECT_EQ(engineType, defaultOsContext->getEngineType());
        EXPECT_EQ(EngineUsage::Regular, defaultOsContext->getEngineUsage());
        EXPECT_TRUE(defaultOsContext->isDefaultContext());

        auto &engine = device->getAllEngines()[0];

        EXPECT_EQ(engine.getEngineType(), engineType);
        EXPECT_TRUE(engine.osContext->isRegular());

        return true;
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    MockDevice *rootDevice = nullptr;
};

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetAndMoreThanOneCcsWhenCreatingRootDeviceWithoutGenericSubDevicesThenCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.EngineInstancedSubDevices.set(true);
    DebugManager.flags.AllowSingleTileEngineInstancedSubDevices.set(true);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto &hwInfo = rootDevice->getHardwareInfo();

    EXPECT_EQ(ccsCount, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    EXPECT_EQ(ccsCount, rootDevice->getNumSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));
    EXPECT_TRUE(hasAllEngines(rootDevice));

    for (uint32_t i = 0; i < ccsCount; i++) {
        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + i);
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_TRUE(isEngineInstanced(subDevice, engineType, 0, 1));
        EXPECT_TRUE(hasEngineInstancedEngines(subDevice, engineType));
    }
}

TEST_F(EngineInstancedDeviceTests, givenDebugFlagNotSetAndMoreThanOneCcsWhenCreatingRootDeviceWithoutGenericSubDevicesThenDontCreateEngineInstanced) {
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

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetAndZeroCcsesWhenCreatingRootDeviceWithoutGenericSubDevicesThenCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 0;

    EXPECT_TRUE(createDevices(genericDevicesCount, ccsCount));

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));
    EXPECT_TRUE(hasAllEngines(rootDevice));

    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumSubDevices());
    EXPECT_FALSE(rootDevice->getNearestGenericSubDevice(0)->isSubDevice());
}

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetAndSingleCcsWhenCreatingRootDeviceWithoutGenericSubDevicesThenDontCreateEngineInstanced) {
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

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetWhenCreatingRootDeviceWithGenericSubDevicesAndZeroCcsesThenDontCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 0;

    EXPECT_TRUE(createDevices(genericDevicesCount, ccsCount));

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->allEngines[0].osContext->isRootDevice());
        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(0u, subDevice->getNumGenericSubDevices());
        EXPECT_EQ(0u, subDevice->getNumSubDevices());
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_TRUE(hasAllEngines(subDevice));
    }
}

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetWhenCreatingRootDeviceWithGenericSubDevicesAndSingleCcsThenDontCreateEngineInstanced) {
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
        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(0u, subDevice->getNumGenericSubDevices());
        EXPECT_EQ(0u, subDevice->getNumSubDevices());
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_TRUE(hasAllEngines(subDevice));
    }
}

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetWhenCreatingRootDeviceWithGenericSubDevicesThenCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.EngineInstancedSubDevices.set(true);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->allEngines[0].osContext->isRootDevice());
        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(0u, subDevice->getNumGenericSubDevices());
        EXPECT_EQ(ccsCount, subDevice->getNumSubDevices());
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_TRUE(hasAllEngines(subDevice));

        for (uint32_t j = 0; j < ccsCount; j++) {
            auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + j);
            auto engineSubDevice = static_cast<MockSubDevice *>(subDevice->getSubDevice(j));
            ASSERT_NE(nullptr, engineSubDevice);

            EXPECT_TRUE(isEngineInstanced(engineSubDevice, engineType, subDevice->getSubDeviceIndex(), subDevice->getDeviceBitfield()));
            EXPECT_TRUE(hasEngineInstancedEngines(engineSubDevice, engineType));
        }
    }
}

TEST_F(EngineInstancedDeviceTests, givenEngineInstancedSubDeviceWhenEngineCreationFailsThenReturnFalse) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 0;

    EXPECT_TRUE(createDevices(genericDevicesCount, ccsCount));

    auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(0));

    auto &hwInfo = rootDevice->getHardwareInfo();
    auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

    subDevice->engineInstanced = true;
    subDevice->failOnCreateEngine = true;
    subDevice->engineInstancedType = gpgpuEngines[0].first;

    EXPECT_FALSE(subDevice->createEngines());
}

TEST_F(EngineInstancedDeviceTests, givenMultipleSubDevicesWhenCallingGetSubDeviceThenReturnCorrectObject) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.EngineInstancedSubDevices.set(true);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto subDevice0 = rootDevice->subdevices[0];
    auto subDevice1 = rootDevice->subdevices[1];

    auto subSubDevice00 = subDevice0->getSubDevice(0);
    auto subSubDevice01 = subDevice0->getSubDevice(1);

    auto subSubDevice10 = subDevice1->getSubDevice(0);
    auto subSubDevice11 = subDevice1->getSubDevice(1);

    {
        EXPECT_EQ(rootDevice->getSubDevice(0), subDevice0);
        EXPECT_EQ(rootDevice->getNearestGenericSubDevice(0), subDevice0);
        EXPECT_EQ(rootDevice->getSubDevice(1), subDevice1);
        EXPECT_EQ(rootDevice->getNearestGenericSubDevice(1), subDevice1);
    }

    {
        EXPECT_EQ(subDevice0->getNearestGenericSubDevice(0), subDevice0);
        EXPECT_EQ(subDevice0->getNearestGenericSubDevice(1), subDevice0);
        EXPECT_EQ(subDevice1->getNearestGenericSubDevice(0), subDevice1);
        EXPECT_EQ(subDevice1->getNearestGenericSubDevice(1), subDevice1);
    }

    {
        EXPECT_NE(subDevice0, subSubDevice00);
        EXPECT_NE(subDevice0, subSubDevice01);
        EXPECT_NE(subDevice1, subSubDevice10);
        EXPECT_NE(subDevice1, subSubDevice11);
    }

    {
        EXPECT_EQ(subSubDevice00->getNearestGenericSubDevice(0), subDevice0);
        EXPECT_EQ(subSubDevice01->getNearestGenericSubDevice(0), subDevice0);
        EXPECT_EQ(subSubDevice10->getNearestGenericSubDevice(0), subDevice1);
        EXPECT_EQ(subSubDevice11->getNearestGenericSubDevice(0), subDevice1);
    }

    {
        EXPECT_ANY_THROW(subSubDevice00->getSubDevice(0));
        EXPECT_ANY_THROW(subSubDevice01->getSubDevice(0));
        EXPECT_ANY_THROW(subSubDevice10->getSubDevice(0));
        EXPECT_ANY_THROW(subSubDevice11->getSubDevice(0));
    }
}

TEST_F(EngineInstancedDeviceTests, givenMultipleClSubDevicesWhenCallingGetSubDeviceThenReturnCorrectObject) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.EngineInstancedSubDevices.set(true);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto subDevice = rootDevice->subdevices[0];
    auto subSubDevice = subDevice->getSubDevice(0);

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);
    auto clSubDevice = std::make_unique<ClDevice>(*subDevice, *clRootDevice, nullptr);
    auto clSubSubDevice = std::make_unique<ClDevice>(*subSubDevice, *clRootDevice, nullptr);

    EXPECT_EQ(clRootDevice->getSubDevice(0), clRootDevice->getNearestGenericSubDevice(0));
    EXPECT_EQ(clSubDevice.get(), clSubDevice->getNearestGenericSubDevice(0));
    EXPECT_EQ(clRootDevice->getSubDevice(0), clSubSubDevice->getNearestGenericSubDevice(0));
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskSetWhenCreatingDevicesThenFilterMaskedDevices) {
    constexpr uint32_t genericDevicesCount = 3;
    constexpr uint32_t ccsCount = 4;
    constexpr uint32_t engineInstancedPerGeneric[3] = {3, 0, 2};
    constexpr bool supportedGenericSubDevices[3] = {true, false, true};
    constexpr bool supportedEngineDevices[3][4] = {{true, true, true, false},
                                                   {false, false, false, false},
                                                   {false, false, true, true}};

    DebugManager.flags.EngineInstancedSubDevices.set(true);
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0.0, 0.0.1, 0.0.2, 0.2.2, 0.2.3, 0.1.5");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        if (!supportedGenericSubDevices[i]) {
            EXPECT_EQ(nullptr, rootDevice->getSubDevice(i));
            continue;
        }

        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(i));

        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->allEngines[0].osContext->isRootDevice());
        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(engineInstancedPerGeneric[i], subDevice->getNumSubDevices());
        EXPECT_EQ(0u, subDevice->getNumGenericSubDevices());
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_TRUE(hasAllEngines(subDevice));

        for (uint32_t j = 0; j < ccsCount; j++) {
            if (!supportedEngineDevices[i][j]) {
                EXPECT_EQ(nullptr, subDevice->getSubDevice(j));
                continue;
            }
            auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + j);
            auto engineSubDevice = static_cast<MockSubDevice *>(subDevice->getSubDevice(j));
            ASSERT_NE(nullptr, engineSubDevice);

            EXPECT_TRUE(isEngineInstanced(engineSubDevice, engineType, subDevice->getSubDeviceIndex(), subDevice->getDeviceBitfield()));
            EXPECT_TRUE(hasEngineInstancedEngines(engineSubDevice, engineType));
        }
    }
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskForSingle3rdLevelDeviceWhenCreatingDevicesThenCreate2ndLevelAsEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 2;
    constexpr uint32_t create2ndLevelAsEngineInstanced[2] = {false, true};
    constexpr uint32_t engineInstanced2ndLevelEngineIndex = 1;

    DebugManager.flags.EngineInstancedSubDevices.set(true);
    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0, 0.1.1");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(i));

        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->allEngines[0].osContext->isRootDevice());

        if (create2ndLevelAsEngineInstanced[i]) {
            auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + engineInstanced2ndLevelEngineIndex);

            DeviceBitfield deviceBitfield = (1llu << i);
            EXPECT_TRUE(isEngineInstanced(subDevice, engineType, i, deviceBitfield));
            EXPECT_TRUE(hasEngineInstancedEngines(subDevice, engineType));

            EXPECT_EQ(0u, subDevice->getNumGenericSubDevices());
            EXPECT_EQ(0u, subDevice->getNumSubDevices());

            continue;
        }

        EXPECT_TRUE(hasAllEngines(subDevice));

        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_EQ(0u, subDevice->getNumGenericSubDevices());
        EXPECT_EQ(ccsCount, subDevice->getNumSubDevices());

        for (uint32_t j = 0; j < ccsCount; j++) {
            auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + j);
            auto engineSubDevice = static_cast<MockSubDevice *>(subDevice->getSubDevice(j));
            ASSERT_NE(nullptr, engineSubDevice);

            EXPECT_TRUE(isEngineInstanced(engineSubDevice, engineType, subDevice->getSubDeviceIndex(), subDevice->getDeviceBitfield()));
            EXPECT_TRUE(hasEngineInstancedEngines(engineSubDevice, engineType));
        }
    }
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskForSingle3rdLevelDeviceOnlyWhenCreatingDevicesThenCreate1stLevelAsEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 2;
    constexpr uint32_t genericDeviceIndex = 1;
    constexpr uint32_t engineInstancedEngineIndex = 1;

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.1.1");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));

    auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + engineInstancedEngineIndex);

    DeviceBitfield deviceBitfield = (1llu << genericDeviceIndex);

    EXPECT_FALSE(rootDevice->allEngines[0].osContext->isRootDevice());
    EXPECT_TRUE(rootDevice->engineInstanced);
    EXPECT_TRUE(rootDevice->getNumGenericSubDevices() == 0);
    EXPECT_TRUE(rootDevice->getNumSubDevices() == 0);
    EXPECT_TRUE(engineType == rootDevice->engineInstancedType);
    EXPECT_TRUE(deviceBitfield == rootDevice->getDeviceBitfield());
    EXPECT_EQ(1u, rootDevice->getDeviceBitfield().count());

    EXPECT_TRUE(hasEngineInstancedEngines(rootDevice, engineType));
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskForSingle2rdLevelDeviceOnlyWhenCreatingDevicesThenCreate1stLevelAsEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;
    constexpr uint32_t genericDeviceIndex = 0;
    constexpr uint32_t engineInstancedEngineIndex = 1;

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0.1, 0.9");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));

    auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + engineInstancedEngineIndex);

    DeviceBitfield deviceBitfield = (1llu << genericDeviceIndex);

    EXPECT_FALSE(rootDevice->allEngines[0].osContext->isRootDevice());
    EXPECT_TRUE(rootDevice->engineInstanced);
    EXPECT_TRUE(rootDevice->getNumGenericSubDevices() == 0);
    EXPECT_TRUE(rootDevice->getNumSubDevices() == 0);
    EXPECT_TRUE(engineType == rootDevice->engineInstancedType);
    EXPECT_TRUE(deviceBitfield == rootDevice->getDeviceBitfield());
    EXPECT_EQ(1u, rootDevice->getDeviceBitfield().count());

    EXPECT_TRUE(hasEngineInstancedEngines(rootDevice, engineType));
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskForSecondLevelOnSingleTileDeviceWhenCreatingThenEnableAllEngineInstancedDevices) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.EngineInstancedSubDevices.set(true);
    DebugManager.flags.AllowSingleTileEngineInstancedSubDevices.set(true);

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0, 0.4");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));

    EXPECT_TRUE(rootDevice->isEngineInstanced());
    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumSubDevices());
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskForSecondLevelOnSingleTileDeviceSingleEngineWhenCreatingThenDontEnableEngineInstancedDevices) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 1;

    DebugManager.flags.EngineInstancedSubDevices.set(true);
    DebugManager.flags.AllowSingleTileEngineInstancedSubDevices.set(true);

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));

    EXPECT_FALSE(rootDevice->isEngineInstanced());
    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumSubDevices());
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskForSecondLevelOnSingleTileDeviceWithoutDebugFlagWhenCreatingThenDontEnableAllEngineInstancedDevices) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));

    EXPECT_FALSE(rootDevice->isEngineInstanced());
    EXPECT_EQ(0u, rootDevice->getNumGenericSubDevices());
    EXPECT_EQ(0u, rootDevice->getNumSubDevices());
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskWhenCreatingClSubDevicesThenSkipDisabledDevices) {
    constexpr uint32_t genericDevicesCount = 3;
    constexpr uint32_t ccsCount = 1;

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0,0.2");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);

    ASSERT_EQ(2u, clRootDevice->getNumSubDevices());
    EXPECT_EQ(0b1u, clRootDevice->getSubDevice(0)->getDeviceBitfield().to_ulong());
    EXPECT_EQ(0b100u, clRootDevice->getSubDevice(1)->getDeviceBitfield().to_ulong());
}

HWTEST2_F(EngineInstancedDeviceTests, givenEngineInstancedDeviceWhenProgrammingCfeStateThenSetSingleSliceDispatch, IsAtLeastXeHpCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    DebugManager.flags.EngineInstancedSubDevices.set(true);

    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.AllowSingleTileEngineInstancedSubDevices.set(true);
    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(0));
    auto defaultEngine = subDevice->getDefaultEngine();
    EXPECT_TRUE(defaultEngine.osContext->isEngineInstanced());

    char buffer[64] = {};
    MockGraphicsAllocation graphicsAllocation(buffer, sizeof(buffer));
    LinearStream linearStream(&graphicsAllocation, graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(defaultEngine.commandStreamReceiver);
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    csr->programVFEState(linearStream, dispatchFlags, 1);

    auto cfeState = reinterpret_cast<CFE_STATE *>(buffer);
    EXPECT_TRUE(cfeState->getSingleSliceDispatchCcsMode());
}

HWTEST_F(EngineInstancedDeviceTests, givenEngineInstancedDeviceWhenCreatingProgramThenAssignAllSubDevices) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.EngineInstancedSubDevices.set(true);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    const char *source = "text";
    size_t sourceSize = strlen(source);

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);
    auto clSubDevice = clRootDevice->getSubDevice(0);
    auto clSubSubDevice0 = clSubDevice->getSubDevice(0);
    auto clSubSubDevice1 = clSubDevice->getSubDevice(1);

    cl_device_id deviceIds[] = {clSubDevice, clSubSubDevice0, clSubSubDevice1};
    ClDeviceVector deviceVector{deviceIds, 3};
    MockContext context(deviceVector);

    cl_int retVal = CL_INVALID_PROGRAM;
    auto program = std::unique_ptr<MockProgram>(Program::create<MockProgram>(
        &context,
        1,
        &source,
        &sourceSize,
        retVal));

    ASSERT_NE(nullptr, program.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    ASSERT_TRUE(program->deviceBuildInfos.find(clSubDevice) != program->deviceBuildInfos.end());

    auto &associatedSubDevices = program->deviceBuildInfos[clSubDevice].associatedSubDevices;
    ASSERT_EQ(2u, associatedSubDevices.size());
    EXPECT_EQ(clSubSubDevice0, associatedSubDevices[0]);
    EXPECT_EQ(clSubSubDevice1, associatedSubDevices[1]);
}

HWTEST_F(EngineInstancedDeviceTests, whenCreateMultipleCommandQueuesThenEnginesAreAssignedUsingRoundRobin) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 4;

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCmdQRoundRobindEngineAssign.set(1);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto &hwInfo = rootDevice->getHardwareInfo();
    const auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (!hwHelper.isAssignEngineRoundRobinSupported(hwInfo)) {
        GTEST_SKIP();
    }

    EXPECT_EQ(ccsCount, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);
    cl_device_id deviceIds[] = {clRootDevice.get()};
    ClDeviceVector deviceVector{deviceIds, 1};
    MockContext context(deviceVector);

    std::array<std::unique_ptr<MockCommandQueueHw<FamilyType>>, 24> cmdQs;
    for (auto &cmdQ : cmdQs) {
        cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, clRootDevice.get(), nullptr);
    }

    const auto &defaultEngine = clRootDevice->getDefaultEngine();
    const auto engineGroupType = hwHelper.getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), hwInfo);

    auto defaultEngineGroupIndex = clRootDevice->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);
    auto engines = clRootDevice->getDevice().getRegularEngineGroups()[defaultEngineGroupIndex].engines;

    for (size_t i = 0; i < cmdQs.size(); i++) {
        auto engineIndex = i % engines.size();
        auto expectedCsr = engines[engineIndex].commandStreamReceiver;
        auto csr = &cmdQs[i]->getGpgpuCommandStreamReceiver();

        EXPECT_EQ(csr, expectedCsr);
    }
}

HWTEST_F(EngineInstancedDeviceTests, givenCmdQRoundRobindEngineAssignBitfieldwWenCreateMultipleCommandQueuesThenEnginesAreAssignedUsingRoundRobinSkippingNotAvailableEngines) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 4;

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCmdQRoundRobindEngineAssign.set(1);
    DebugManager.flags.CmdQRoundRobindEngineAssignBitfield.set(0b1101);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto &hwInfo = rootDevice->getHardwareInfo();
    const auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (!hwHelper.isAssignEngineRoundRobinSupported(hwInfo)) {
        GTEST_SKIP();
    }

    EXPECT_EQ(ccsCount, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);
    cl_device_id deviceIds[] = {clRootDevice.get()};
    ClDeviceVector deviceVector{deviceIds, 1};
    MockContext context(deviceVector);

    std::array<std::unique_ptr<MockCommandQueueHw<FamilyType>>, 24> cmdQs;
    for (auto &cmdQ : cmdQs) {
        cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, clRootDevice.get(), nullptr);
    }

    const auto &defaultEngine = clRootDevice->getDefaultEngine();
    const auto engineGroupType = hwHelper.getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), hwInfo);

    auto defaultEngineGroupIndex = clRootDevice->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);
    auto engines = clRootDevice->getDevice().getRegularEngineGroups()[defaultEngineGroupIndex].engines;

    for (size_t i = 0, j = 0; i < cmdQs.size(); i++, j++) {
        if ((j % engines.size()) == 1) {
            j++;
        }
        auto engineIndex = j % engines.size();
        auto expectedCsr = engines[engineIndex].commandStreamReceiver;
        auto csr = &cmdQs[i]->getGpgpuCommandStreamReceiver();

        EXPECT_EQ(csr, expectedCsr);
    }
}

HWTEST_F(EngineInstancedDeviceTests, givenCmdQRoundRobindEngineAssignNTo1wWenCreateMultipleCommandQueuesThenEnginesAreAssignedUsingRoundRobinAndNQueuesShareSameCsr) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 4;

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCmdQRoundRobindEngineAssign.set(1);
    DebugManager.flags.CmdQRoundRobindEngineAssignNTo1.set(3);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto &hwInfo = rootDevice->getHardwareInfo();
    const auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (!hwHelper.isAssignEngineRoundRobinSupported(hwInfo)) {
        GTEST_SKIP();
    }

    EXPECT_EQ(ccsCount, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);
    cl_device_id deviceIds[] = {clRootDevice.get()};
    ClDeviceVector deviceVector{deviceIds, 1};
    MockContext context(deviceVector);

    std::array<std::unique_ptr<MockCommandQueueHw<FamilyType>>, 24> cmdQs;
    for (auto &cmdQ : cmdQs) {
        cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, clRootDevice.get(), nullptr);
    }

    const auto &defaultEngine = clRootDevice->getDefaultEngine();
    const auto engineGroupType = hwHelper.getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), hwInfo);

    auto defaultEngineGroupIndex = clRootDevice->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);
    auto engines = clRootDevice->getDevice().getRegularEngineGroups()[defaultEngineGroupIndex].engines;

    for (size_t i = 0, j = 0; i < cmdQs.size(); i++, j++) {
        auto engineIndex = (j / 3) % engines.size();
        auto expectedCsr = engines[engineIndex].commandStreamReceiver;
        auto csr = &cmdQs[i]->getGpgpuCommandStreamReceiver();

        EXPECT_EQ(csr, expectedCsr);
    }
}

HWTEST_F(EngineInstancedDeviceTests, givenCmdQRoundRobindEngineAssignNTo1AndCmdQRoundRobindEngineAssignBitfieldwWenCreateMultipleCommandQueuesThenEnginesAreAssignedProperlyUsingRoundRobin) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 4;

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCmdQRoundRobindEngineAssign.set(1);
    DebugManager.flags.CmdQRoundRobindEngineAssignNTo1.set(3);
    DebugManager.flags.CmdQRoundRobindEngineAssignBitfield.set(0b1101);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto &hwInfo = rootDevice->getHardwareInfo();
    const auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (!hwHelper.isAssignEngineRoundRobinSupported(hwInfo)) {
        GTEST_SKIP();
    }

    EXPECT_EQ(ccsCount, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);
    cl_device_id deviceIds[] = {clRootDevice.get()};
    ClDeviceVector deviceVector{deviceIds, 1};
    MockContext context(deviceVector);

    std::array<std::unique_ptr<MockCommandQueueHw<FamilyType>>, 24> cmdQs;
    for (auto &cmdQ : cmdQs) {
        cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, clRootDevice.get(), nullptr);
    }

    const auto &defaultEngine = clRootDevice->getDefaultEngine();
    const auto engineGroupType = hwHelper.getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), hwInfo);

    auto defaultEngineGroupIndex = clRootDevice->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);
    auto engines = clRootDevice->getDevice().getRegularEngineGroups()[defaultEngineGroupIndex].engines;

    for (size_t i = 0, j = 0; i < cmdQs.size(); i++, j++) {
        while (((j / 3) % engines.size()) == 1) {
            j++;
        }
        auto engineIndex = (j / 3) % engines.size();
        auto expectedCsr = engines[engineIndex].commandStreamReceiver;
        auto csr = &cmdQs[i]->getGpgpuCommandStreamReceiver();

        EXPECT_EQ(csr, expectedCsr);
    }
}

HWTEST_F(EngineInstancedDeviceTests, givenEnableCmdQRoundRobindEngineAssignDisabledWenCreateMultipleCommandQueuesThenDefaultEngineAssigned) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 4;

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCmdQRoundRobindEngineAssign.set(0);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto &hwInfo = rootDevice->getHardwareInfo();
    const auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (!hwHelper.isAssignEngineRoundRobinSupported(hwInfo)) {
        GTEST_SKIP();
    }

    EXPECT_EQ(ccsCount, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);

    auto clRootDevice = std::make_unique<ClDevice>(*rootDevice, nullptr);
    cl_device_id deviceIds[] = {clRootDevice.get()};
    ClDeviceVector deviceVector{deviceIds, 1};
    MockContext context(deviceVector);

    std::array<std::unique_ptr<MockCommandQueueHw<FamilyType>>, 24> cmdQs;
    for (auto &cmdQ : cmdQs) {
        cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, clRootDevice.get(), nullptr);
    }

    const auto &defaultEngine = clRootDevice->getDefaultEngine();

    for (auto &cmdQ : cmdQs) {
        auto expectedCsr = defaultEngine.commandStreamReceiver;
        auto csr = &cmdQ->getGpgpuCommandStreamReceiver();

        EXPECT_EQ(csr, expectedCsr);
    }
}

TEST(SubDevicesTest, whenInitializeRootCsrThenDirectSubmissionIsNotInitialized) {
    auto device = std::make_unique<MockDevice>();
    device->initializeRootCommandStreamReceiver();

    auto csr = device->getEngine(1u).commandStreamReceiver;
    EXPECT_FALSE(csr->isDirectSubmissionEnabled());
}

TEST(SubDevicesTest, givenCreateMultipleSubDevicesFlagSetWhenBindlessHeapHelperCreatedThenSubDeviceReturnRootDeviceMember) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->createBindlessHeapsHelper(device->getMemoryManager(), device->getNumGenericSubDevices() > 1, device->getRootDeviceIndex(), device->getDeviceBitfield());
    EXPECT_EQ(device->getBindlessHeapsHelper(), device->subdevices.at(0)->getBindlessHeapsHelper());
}
