/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

#define ADAPTER_HANDLE_WDDM_FAKE (static_cast<D3DKMT_HANDLE>(0x40001234))

struct CloseAdapterMock {
    static NTSTATUS(APIENTRY closeAdapter)(
        const D3DKMT_CLOSEADAPTER *closeAdapter) {
        return STATUS_SUCCESS;
    }
};

struct MockHwDeviceIdWddm : public HwDeviceIdWddm {
    using HwDeviceIdWddm::osEnvironment;
    MockHwDeviceIdWddm(D3DKMT_HANDLE adapterIn, LUID adapterLuidIn, uint32_t adapterNodeOrdinalIn, OsEnvironment *osEnvironmentIn, std::unique_ptr<UmKmDataTranslator> umKmDataTranslator) : HwDeviceIdWddm(adapterIn, adapterLuidIn, adapterNodeOrdinalIn, osEnvironmentIn, std::move(umKmDataTranslator)) {}
};

class MockDriverModelWDDMLUID : public NEO::Wddm {
  public:
    MockDriverModelWDDMLUID(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::move(hwDeviceId), rootDeviceEnvironment) {
    }
    bool init() {
        return true;
    }

    bool isDriverAvailable() override {
        return false;
    }

    bool skipResourceCleanup() {
        return true;
    }

    MockDriverModelWDDMLUID(RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::make_unique<MockHwDeviceIdWddm>(ADAPTER_HANDLE_WDDM_FAKE, LUID{0x12, 0x1234}, 1u, rootDeviceEnvironment.executionEnvironment.osEnvironment.get(), std::make_unique<UmKmDataTranslator>()), rootDeviceEnvironment) {
        if (!rootDeviceEnvironment.executionEnvironment.osEnvironment.get()) {
            rootDeviceEnvironment.executionEnvironment.osEnvironment = std::make_unique<OsEnvironmentWin>();
        }
        static_cast<MockHwDeviceIdWddm *>(this->hwDeviceId.get())->osEnvironment = rootDeviceEnvironment.executionEnvironment.osEnvironment.get();
        OsEnvironmentWin *osEnvWin = reinterpret_cast<OsEnvironmentWin *>(static_cast<MockHwDeviceIdWddm *>(this->hwDeviceId.get())->osEnvironment);
        osEnvWin->gdi.get()->closeAdapter.mFunc = CloseAdapterMock::closeAdapter;
    }
};

class MockOsContextWin : public OsContextWin {
  public:
    MockOsContextWin(MockDriverModelWDDMLUID &wddm, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor)
        : OsContextWin(wddm, rootDeviceIndex, contextId, engineDescriptor) {}
};

using LuidDeviceTest = Test<DeviceFixture>;

TEST_F(LuidDeviceTest, givenOsContextWinAndGetDeviceNodeMaskThenNodeMaskIsAtLeast1) {
    auto luidMock = new MockDriverModelWDDMLUID(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    auto defaultEngine = defaultHwInfo->capabilityTable.defaultEngineType;
    OsContextWin osContext(*luidMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({defaultEngine, EngineUsage::regular}));
    EXPECT_GE(osContext.getDeviceNodeMask(), 1u);
    delete luidMock;
}

TEST_F(LuidDeviceTest, givenOsContextWinAndGetLUIDArrayThenLUIDisValid) {
    auto luidMock = new MockDriverModelWDDMLUID(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    auto defaultEngine = defaultHwInfo->capabilityTable.defaultEngineType;
    OsContextWin osContext(*luidMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({defaultEngine, EngineUsage::regular}));
    std::vector<uint8_t> luidData;
    size_t arraySize = 8;
    osContext.getDeviceLuidArray(luidData, arraySize);
    uint64_t luid = 0;
    memcpy_s(&luid, sizeof(uint64_t), luidData.data(), sizeof(uint8_t) * luidData.size());
    EXPECT_NE(luid, (uint64_t)0);
    delete luidMock;
}

TEST_F(LuidDeviceTest, givenLuidDevicePropertiesStructureAndWDDMDriverTypeThenSuccessReturned) {
    auto defaultEngine = defaultHwInfo->capabilityTable.defaultEngineType;
    auto luidMock = new MockDriverModelWDDMLUID(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(luidMock));
    MockOsContextWin mockContext(*luidMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({defaultEngine, EngineUsage::regular}));
    auto &deviceRegularEngines = neoDevice->getRegularEngineGroups();
    auto &deviceEngine = deviceRegularEngines[0].engines[0];
    auto csr = deviceEngine.commandStreamReceiver;
    csr->setupContext(mockContext);
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_luid_ext_properties_t deviceLuidProperties = {ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES};
    deviceProperties.pNext = &deviceLuidProperties;
    ze_result_t result = device->getProperties(&deviceProperties);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    uint64_t luid = 0;
    LUID adapterLuid{0x12, 0x1234};
    memcpy_s(&luid, sizeof(uint64_t), &deviceLuidProperties.luid, sizeof(deviceLuidProperties.luid));
    uint32_t lowLUID = luid & 0xFFFFFFFF;
    uint32_t highLUID = ((luid & 0xFFFFFFFF00000000) >> 32);
    EXPECT_EQ(lowLUID, (uint32_t)adapterLuid.LowPart);
    EXPECT_EQ(highLUID, (uint32_t)adapterLuid.HighPart);
    EXPECT_NE(deviceLuidProperties.nodeMask, (uint32_t)0);
}

TEST_F(LuidDeviceTest, givenLuidDevicePropertiesStructureAndDRMDriverTypeThenUnsupportedReturned) {
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_luid_ext_properties_t deviceLuidProperties = {ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES};
    deviceProperties.pNext = &deviceLuidProperties;
    ze_result_t result = device->getProperties(&deviceProperties);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(LuidDeviceTest, givenLuidDevicePropertiesStructureAndAndNoOsInterfaceThenUninitReturned) {
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(nullptr);
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_luid_ext_properties_t deviceLuidProperties = {ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES};
    deviceProperties.pNext = &deviceLuidProperties;
    ze_result_t result = device->getProperties(&deviceProperties);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNINITIALIZED);
}

} // namespace ult
} // namespace L0