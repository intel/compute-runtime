/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/xe_hpc_core/xe_hpc_core_test_l0_fixtures.h"

#include "neo_aot_platforms.h"

namespace L0 {
namespace ult {

using DeviceTestPvcXt = Test<DeviceFixtureXeHpcTests>;

PVCTEST_F(DeviceTestPvcXt, whenCallingGetMemoryPropertiesWithNonNullPtrAndRevisionIsNotBaseDieA0OnPvcXtThenMaxClockRateReturnedIsZero) {
    auto &device = driverHandle->devices[0];
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &productHelper = device->getProductHelper();
    hwInfo->platform.usDeviceID = pvcXtDeviceIds.front();
    hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo); // not BD A0

    checkIfCallingGetMemoryPropertiesWithNonNullPtrThenMaxClockRateReturnZero(hwInfo);
}

using DeviceTestPvc = Test<DeviceFixtureXeHpcTests>;
PVCTEST_F(DeviceTestPvc, givenPvcAStepWhenCreatingMultiTileDeviceThenExpectImplicitScalingDisabled) {
    auto hwInfo = *NEO::defaultHwInfo;
    auto &productHelper = getHelper<NEO::ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

    debugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> apiSupportBackup(&NEO::ImplicitScaling::apiSupport, true);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);

    EXPECT_FALSE(device->isImplicitScalingCapable());

    static_cast<DeviceImp *>(device)->releaseResources();
    delete device;
}

PVCTEST_F(DeviceTestPvc, GivenPvcWhenGettingPhysicalEuSimdWidthThenReturn16) {
    ze_device_properties_t properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&properties);
    EXPECT_EQ(16u, properties.physicalEUSimdWidth);
}

PVCTEST_F(DeviceTestPvc, givenPvcXlDeviceIdAndRevIdWhenGetDeviceIpVersion) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_ip_version_ext_t zeDeviceIpVersion = {ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT};
    zeDeviceIpVersion.ipVersion = std::numeric_limits<uint32_t>::max();
    deviceProperties.pNext = &zeDeviceIpVersion;
    std::vector<std::pair<uint32_t, AOT::PRODUCT_CONFIG>> pvcValues = {
        {0x0, AOT::PVC_XL_A0},
        {0x1, AOT::PVC_XL_A0P}};

    for (const auto &deviceId : pvcXlDeviceIds) {
        for (const auto &[revId, config] : pvcValues) {
            device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usDeviceID = deviceId;
            device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = revId;
            device->getProperties(&deviceProperties);

            EXPECT_NE(std::numeric_limits<uint32_t>::max(), zeDeviceIpVersion.ipVersion);
            EXPECT_EQ(config, zeDeviceIpVersion.ipVersion);
        }
    }
}

PVCTEST_F(DeviceTestPvc, givenPvcXtDeviceIdAndRevIdWhenGetDeviceIpVersion) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_device_ip_version_ext_t zeDeviceIpVersion = {ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT};
    zeDeviceIpVersion.ipVersion = std::numeric_limits<uint32_t>::max();
    deviceProperties.pNext = &zeDeviceIpVersion;
    std::vector<std::pair<uint32_t, AOT::PRODUCT_CONFIG>> pvcXtValues = {
        {0x3, AOT::PVC_XT_A0},
        {0x5, AOT::PVC_XT_B0},
        {0x6, AOT::PVC_XT_B1},
        {0x7, AOT::PVC_XT_C0}};

    std::vector<std::pair<uint32_t, AOT::PRODUCT_CONFIG>> pvcXtVgValues = {{0x7, AOT::PVC_XT_C0_VG}};

    for (const auto &deviceId : pvcXtDeviceIds) {
        for (const auto &[revId, config] : pvcXtValues) {
            device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usDeviceID = deviceId;
            device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = revId;
            device->getProperties(&deviceProperties);

            EXPECT_NE(std::numeric_limits<uint32_t>::max(), zeDeviceIpVersion.ipVersion);
            EXPECT_EQ(config, zeDeviceIpVersion.ipVersion);
        }
    }

    for (const auto &deviceId : pvcXtVgDeviceIds) {
        for (const auto &[revId, config] : pvcXtVgValues) {
            device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usDeviceID = deviceId;
            device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = revId;
            device->getProperties(&deviceProperties);

            EXPECT_NE(std::numeric_limits<uint32_t>::max(), zeDeviceIpVersion.ipVersion);
            EXPECT_EQ(config, zeDeviceIpVersion.ipVersion);
        }
    }
}

} // namespace ult
} // namespace L0
