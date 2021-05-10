/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

TEST(SubDevicesTest, givenDefaultConfigWhenCreateRootDeviceThenItDoesntContainSubDevices) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(0u, device->getNumSubDevices());
    EXPECT_EQ(1u, device->getNumAvailableDevices());
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

    EXPECT_EQ(2u, device->getNumAvailableDevices());
    EXPECT_EQ(1u, device->subdevices.at(0)->getNumAvailableDevices());
    EXPECT_EQ(1u, device->subdevices.at(1)->getNumAvailableDevices());
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

    auto subDevice = device->getDeviceById(1);
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
    auto subDevice = device->getDeviceById(0);

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
    DebugManager.flags.CreateMultipleSubDevices.set(10);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.incRefInternal();
    executionEnvironment.memoryManager.reset(new FailMemoryManager(10, executionEnvironment));
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

TEST(SubDevicesTest, givenDeviceWithoutSubDevicesWhenGettingDeviceByIdZeroThenGetThisDevice) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_EQ(1u, device->getNumAvailableDevices());
    EXPECT_EQ(device.get(), device->getDeviceById(0u));
}

TEST(SubDevicesTest, givenDeviceWithSubDevicesWhenGettingDeviceByIdThenGetCorrectSubDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(2u, device->getNumSubDevices());
    EXPECT_EQ(device->subdevices.at(0), device->getDeviceById(0));
    EXPECT_EQ(device->subdevices.at(1), device->getDeviceById(1));
    EXPECT_THROW(device->getDeviceById(2), std::exception);
}

TEST(SubDevicesTest, givenSubDevicesWhenGettingDeviceByIdZeroThenGetThisSubDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(2u, device->getNumSubDevices());
    auto subDevice = device->subdevices.at(0);

    EXPECT_EQ(subDevice, subDevice->getDeviceById(0));
}

TEST(RootDevicesTest, givenRootDeviceWithoutSubdevicesWhenCreateEnginesThenDeviceCreatesCorrectNumberOfEngines) {
    auto hwInfo = *defaultHwInfo;
    auto &gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

    auto executionEnvironment = new MockExecutionEnvironment;
    MockDevice device(executionEnvironment, 0);
    EXPECT_EQ(0u, device.engines.size());
    device.createEngines();
    EXPECT_EQ(gpgpuEngines.size(), device.engines.size());
}

TEST(RootDevicesTest, givenRootDeviceWithSubdevicesWhenCreateEnginesThenDeviceCreatesSpecialEngine) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

    auto executionEnvironment = new MockExecutionEnvironment;
    MockDevice device(executionEnvironment, 0);
    device.createSubDevices();
    EXPECT_EQ(2u, device.getNumAvailableDevices());
    EXPECT_EQ(0u, device.engines.size());
    device.createEngines();
    EXPECT_EQ(1u, device.engines.size());
}

TEST(SubDevicesTest, givenRootDeviceWithSubDevicesWhenGettingGlobalMemorySizeThenSubDevicesReturnReducedAmountOfGlobalMemAllocSize) {
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

TEST(SubDevicesTest, whenCreatingEngineInstancedSubDeviceThenSetCorrectSubdeviceIndex) {
    class MyRootDevice : public RootDevice {
      public:
        using RootDevice::createEngineInstancedSubDevice;
        using RootDevice::RootDevice;
    };

    auto executionEnvironment = new ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DeviceFactory::createMemoryManagerFunc(*executionEnvironment);

    auto rootDevice = std::unique_ptr<MyRootDevice>(Device::create<MyRootDevice>(executionEnvironment, 0));

    auto subDevice = std::unique_ptr<SubDevice>(rootDevice->createEngineInstancedSubDevice(1, defaultHwInfo->capabilityTable.defaultEngineType));

    ASSERT_NE(nullptr, subDevice.get());

    EXPECT_EQ(2u, subDevice->getDeviceBitfield().to_ulong());
}

struct EngineInstancedDeviceTests : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.EngineInstancedSubDevices.set(true);
    }

    bool createDevices(uint32_t numGenericSubDevices, uint32_t numCcs) {
        DebugManager.flags.CreateMultipleSubDevices.set(numGenericSubDevices);

        auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);

        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = numCcs;
        hwInfo->featureTable.ftrCCSNode = (numCcs > 0);
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
        return ((device->engines.size() == 1) &&
                device->engines[0].osContext->isRootDevice());
    }

    bool isEngineInstanced(MockSubDevice *subDevice, aub_stream::EngineType engineType, uint32_t subDeviceIndex, DeviceBitfield deviceBitfield) {
        bool isEngineInstanced = !subDevice->engines[0].osContext->isRootDevice();
        isEngineInstanced &= subDevice->engineInstanced;
        isEngineInstanced &= (subDevice->getNumAvailableDevices() == 1);
        isEngineInstanced &= (engineType == subDevice->engineInstancedType);
        isEngineInstanced &= (subDeviceIndex == subDevice->getSubDeviceIndex());
        isEngineInstanced &= (deviceBitfield == subDevice->getDeviceBitfield());

        return isEngineInstanced;
    }

    template <typename MockDeviceT>
    bool hasAllEngines(MockDeviceT *device) {
        auto &hwInfo = device->getHardwareInfo();
        auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

        for (size_t i = 0; i < gpgpuEngines.size(); i++) {
            if (device->engines[i].getEngineType() != gpgpuEngines[i].first) {
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
        bool ccsFound = false;

        for (auto &engine : device->engines) {
            if ((engine.getEngineType() != engineType) && !EngineHelpers::isBcs(engine.getEngineType())) {
                return false;
            }

            auto osContext = engine.osContext;

            if ((engine.getEngineType() == engineType) &&
                osContext->isDefaultContext() &&
                osContext->isRegular() &&
                !osContext->isLowPriority() &&
                !osContext->isInternalEngine()) {
                EXPECT_FALSE(ccsFound);
                ccsFound = true;
            }
        }

        return ccsFound;
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    MockDevice *rootDevice = nullptr;
};

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetAndMoreThanOneCcsWhenCreatingRootDeviceWithoutGenericSubDevicesThenCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto &hwInfo = rootDevice->getHardwareInfo();

    EXPECT_EQ(ccsCount, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    EXPECT_EQ(ccsCount, rootDevice->getNumAvailableDevices());

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));
    EXPECT_TRUE(hasAllEngines(rootDevice));

    for (uint32_t i = 0; i < ccsCount; i++) {
        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + i);
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getDeviceById(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_TRUE(isEngineInstanced(subDevice, engineType, 0, 1));
        EXPECT_TRUE(hasEngineInstancedEngines(subDevice, engineType));
    }
}

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetAndZeroCcsesWhenCreatingRootDeviceWithoutGenericSubDevicesThenCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 0;

    EXPECT_TRUE(createDevices(genericDevicesCount, ccsCount));

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));
    EXPECT_TRUE(hasAllEngines(rootDevice));

    EXPECT_EQ(1u, rootDevice->getNumAvailableDevices());
    EXPECT_FALSE(rootDevice->getDeviceById(0)->isSubDevice());
}

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetAndSingleCcsWhenCreatingRootDeviceWithoutGenericSubDevicesThenDontCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 1;

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));
    EXPECT_TRUE(hasAllEngines(rootDevice));

    EXPECT_EQ(1u, rootDevice->getNumAvailableDevices());
    EXPECT_FALSE(rootDevice->getDeviceById(0)->isSubDevice());
}

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetWhenCreatingRootDeviceWithGenericSubDevicesAndZeroCcsesThenDontCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 0;

    EXPECT_TRUE(createDevices(genericDevicesCount, ccsCount));

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getDeviceById(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->engines[0].osContext->isRootDevice());
        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(1u, subDevice->getNumAvailableDevices());
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
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getDeviceById(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->engines[0].osContext->isRootDevice());
        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(1u, subDevice->getNumAvailableDevices());
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_TRUE(hasAllEngines(subDevice));
    }
}

TEST_F(EngineInstancedDeviceTests, givenDebugFlagSetWhenCreatingRootDeviceWithGenericSubDevicesThenCreateEngineInstanced) {
    constexpr uint32_t genericDevicesCount = 2;
    constexpr uint32_t ccsCount = 2;

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getDeviceById(i));
        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->engines[0].osContext->isRootDevice());
        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(ccsCount, subDevice->getNumAvailableDevices());
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_TRUE(hasAllEngines(subDevice));

        for (uint32_t j = 0; j < ccsCount; j++) {
            auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + j);
            auto engineSubDevice = static_cast<MockSubDevice *>(subDevice->getDeviceById(j));
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

    auto subDevice = static_cast<MockSubDevice *>(rootDevice->getDeviceById(0));

    auto &hwInfo = rootDevice->getHardwareInfo();
    auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

    subDevice->engineInstanced = true;
    subDevice->failOnCreateEngine = true;
    subDevice->engineInstancedType = gpgpuEngines[0].first;

    EXPECT_FALSE(subDevice->createEngines());
}

TEST_F(EngineInstancedDeviceTests, givenAffinityMaskSetWhenCreatingDevicesThenFilterMaskedDevices) {
    constexpr uint32_t genericDevicesCount = 3;
    constexpr uint32_t ccsCount = 4;
    constexpr uint32_t engineInstancedPerGeneric[3] = {3, 0, 2};
    constexpr bool supportedGenericSubDevices[3] = {true, false, true};
    constexpr bool supportedEngineDevices[3][4] = {{true, true, true, false},
                                                   {false, false, false, false},
                                                   {false, false, true, true}};

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0.0, 0.0.1, 0.0.2, 0.2.2, 0.2.3, 0.1.5");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        if (!supportedGenericSubDevices[i]) {
            EXPECT_EQ(nullptr, rootDevice->getDeviceById(i));
            continue;
        }

        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getDeviceById(i));

        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->engines[0].osContext->isRootDevice());
        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(engineInstancedPerGeneric[i], subDevice->getNumAvailableDevices());
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_TRUE(hasAllEngines(subDevice));

        for (uint32_t j = 0; j < ccsCount; j++) {
            if (!supportedEngineDevices[i][j]) {
                EXPECT_EQ(nullptr, subDevice->getDeviceById(j));
                continue;
            }
            auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + j);
            auto engineSubDevice = static_cast<MockSubDevice *>(subDevice->getDeviceById(j));
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

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.0, 0.1.1");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_TRUE(hasRootCsrOnly(rootDevice));

    for (uint32_t i = 0; i < genericDevicesCount; i++) {
        auto subDevice = static_cast<MockSubDevice *>(rootDevice->getDeviceById(i));

        ASSERT_NE(nullptr, subDevice);

        EXPECT_FALSE(subDevice->engines[0].osContext->isRootDevice());

        if (create2ndLevelAsEngineInstanced[i]) {
            auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + engineInstanced2ndLevelEngineIndex);

            DeviceBitfield deviceBitfield = (1llu << i);
            EXPECT_TRUE(isEngineInstanced(subDevice, engineType, i, deviceBitfield));
            EXPECT_TRUE(hasEngineInstancedEngines(subDevice, engineType));

            EXPECT_EQ(1u, subDevice->getNumAvailableDevices());

            continue;
        }

        EXPECT_TRUE(hasAllEngines(subDevice));

        EXPECT_FALSE(subDevice->engineInstanced);
        EXPECT_EQ(aub_stream::EngineType::NUM_ENGINES, subDevice->engineInstancedType);

        EXPECT_EQ(ccsCount, subDevice->getNumAvailableDevices());

        for (uint32_t j = 0; j < ccsCount; j++) {
            auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + j);
            auto engineSubDevice = static_cast<MockSubDevice *>(subDevice->getDeviceById(j));
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

    EXPECT_FALSE(rootDevice->engines[0].osContext->isRootDevice());
    EXPECT_TRUE(rootDevice->engineInstanced);
    EXPECT_TRUE(rootDevice->getNumAvailableDevices() == 1);
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

    DebugManager.flags.ZE_AFFINITY_MASK.set("0.1, 0.9");

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    EXPECT_FALSE(hasRootCsrOnly(rootDevice));

    auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + engineInstancedEngineIndex);

    DeviceBitfield deviceBitfield = (1llu << genericDeviceIndex);

    EXPECT_FALSE(rootDevice->engines[0].osContext->isRootDevice());
    EXPECT_TRUE(rootDevice->engineInstanced);
    EXPECT_TRUE(rootDevice->getNumAvailableDevices() == 1);
    EXPECT_TRUE(engineType == rootDevice->engineInstancedType);
    EXPECT_TRUE(deviceBitfield == rootDevice->getDeviceBitfield());
    EXPECT_EQ(1u, rootDevice->getDeviceBitfield().count());

    EXPECT_TRUE(hasEngineInstancedEngines(rootDevice, engineType));
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

    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->createBindlessHeapsHelper(device->getMemoryManager(), device->getNumAvailableDevices() > 1, device->getRootDeviceIndex());
    EXPECT_EQ(device->getBindlessHeapsHelper(), device->subdevices.at(0)->getBindlessHeapsHelper());
}
