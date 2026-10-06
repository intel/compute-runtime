/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/driver/linux/mock_sysman_pci_device_properties.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanPciDevicePropertiesLinuxTest = SysmanDeviceFixture;

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenNullCountWhenCallingGetPciDevicePropertiesThenInvalidNullPointerIsReturned) {
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, pLinuxDriverImp->getPciDeviceProperties(nullptr, nullptr));
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenNoPciDevicesWhenCallingGetPciDevicePropertiesThenCountIsZeroAndSuccessIsReturned) {
    MockPciSysfs pciSysfs;
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenTwoIntelGpuDevicesWhenCallingGetPciDevicePropertiesWithZeroCountThenCorrectCountIsReturned) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    pciSysfs.addDevice("0000:04:00.0");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(2u, count);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenIntelGpuDeviceWhenCallingGetPciDevicePropertiesThenIdentityAndGoodStatusAreReturned) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0001:03:02.1");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    ASSERT_EQ(1u, count);

    zes_intel_driver_pci_device_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_DRIVER_PCI_DEVICE_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    EXPECT_EQ(1u, count);

    EXPECT_EQ(1u, properties.address.domain);
    EXPECT_EQ(3u, properties.address.bus);
    EXPECT_EQ(2u, properties.address.device);
    EXPECT_EQ(1u, properties.address.function);
    EXPECT_EQ(ZES_PCI_LINK_STATUS_GOOD, properties.status);

    EXPECT_EQ(-1, properties.maxSpeed.gen);
    EXPECT_EQ(-1, properties.maxSpeed.width);
    EXPECT_EQ(-1, properties.maxSpeed.maxBandwidth);

    EXPECT_EQ(ZES_INTEL_STRUCTURE_TYPE_PCI_CONFIG_EXP_PROPERTIES_1_1, properties.configProperties.stype);
    EXPECT_EQ(nullptr, properties.configProperties.pNext);
    EXPECT_EQ(mockPciIntelVendorId, properties.configProperties.vendorId);
    EXPECT_EQ(getMockPciDeviceId(), properties.configProperties.deviceId);
    EXPECT_EQ(mockPciSubsystemVendorId, properties.configProperties.subsystemVendorId);
    EXPECT_EQ(mockPciSubsystemDeviceId, properties.configProperties.subsystemDeviceId);
    EXPECT_EQ(mockPciRevision, properties.configProperties.revision);
    // The walk reads the standard header only, so the PCI Express capability details stay zero.
    EXPECT_EQ(0u, properties.configProperties.pcieCapabilityVersion);
    EXPECT_EQ(0u, properties.configProperties.supportedLinkSpeeds);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenCountSmallerThanAvailableWhenCallingGetPciDevicePropertiesThenOnlyRequestedDevicesAreReturned) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    pciSysfs.addDevice("0000:04:00.0");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 1;
    zes_intel_driver_pci_device_properties_exp_t properties[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, properties));
    EXPECT_EQ(1u, count);
    EXPECT_EQ(3u, properties[0].address.bus);
    // The second element must be left untouched.
    EXPECT_EQ(0u, properties[1].address.bus);
    EXPECT_EQ(ZES_PCI_LINK_STATUS_UNKNOWN, properties[1].status);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenCountLargerThanAvailableWhenCallingGetPciDevicePropertiesThenCountIsUpdatedToAvailable) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 5;
    zes_intel_driver_pci_device_properties_exp_t properties[5] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, properties));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenNullPropertiesAndNonZeroCountWhenCallingGetPciDevicePropertiesThenCountIsUpdated) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 5;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenNonNullPNextWhenCallingGetPciDevicePropertiesThenInvalidArgumentIsReturnedAndNothingIsWritten) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    pciSysfs.addDevice("0000:04:00.0");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    zes_intel_pci_config_exp_properties_t extension = {};
    uint32_t count = 2;
    zes_intel_driver_pci_device_properties_exp_t properties[2] = {};
    properties[1].pNext = &extension;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pLinuxDriverImp->getPciDeviceProperties(&count, properties));
    EXPECT_EQ(2u, count);
    EXPECT_EQ(0u, properties[0].address.bus);
    EXPECT_EQ(ZES_PCI_LINK_STATUS_UNKNOWN, properties[0].status);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenNonNullPNextOnElementBeyondAvailableCountWhenCallingGetPciDevicePropertiesThenSuccessIsReturned) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    zes_intel_pci_config_exp_properties_t extension = {};
    uint32_t count = 2;
    zes_intel_driver_pci_device_properties_exp_t properties[2] = {};
    properties[1].pNext = &extension;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, properties));
    EXPECT_EQ(1u, count);
    EXPECT_EQ(3u, properties[0].address.bus);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenNonIntelAndNonGpuDevicesWhenCallingGetPciDevicePropertiesThenOnlyIntelGpuDevicesAreReported) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");

    auto &otherVendorDevice = pciSysfs.addDevice("0000:04:00.0");
    otherVendorDevice.attributes["vendor"] = getMockSysfsHexValue(mockPciNonIntelVendorId, 4);

    auto &networkDevice = pciSysfs.addDevice("0000:05:00.0");
    networkDevice.attributes["class"] = getMockSysfsHexValue(mockPciNetworkControllerClassCode, 6);

    // A directory whose name is not a BDF cannot be a PCI device.
    pciSysfs.addDevice("not-a-bdf");

    auto &missingVendorDevice = pciSysfs.addDevice("0000:06:00.0");
    missingVendorDevice.attributes.erase("vendor");

    auto &missingClassDevice = pciSysfs.addDevice("0000:07:00.0");
    missingClassDevice.attributes.erase("class");

    auto &missingDeviceIdDevice = pciSysfs.addDevice("0000:08:00.0");
    missingDeviceIdDevice.attributes.erase("device");

    auto &malformedVendorDevice = pciSysfs.addDevice("0000:09:00.0");
    malformedVendorDevice.attributes["vendor"] = "invalid";

    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenVirtualFunctionWhenCallingGetPciDevicePropertiesThenVirtualFunctionIsNotReported) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    auto &virtualFunction = pciSysfs.addDevice("0000:03:00.1");
    virtualFunction.isVirtualFunction = true;
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenBothAcceptedBaseClassesWhenCallingGetPciDevicePropertiesThenBothDevicesAreReported) {
    MockPciSysfs pciSysfs;
    auto &displayController = pciSysfs.addDevice("0000:03:00.0");
    displayController.attributes["class"] = getMockSysfsHexValue(mockPciDisplayControllerClassCode, 6);
    auto &processingAccelerator = pciSysfs.addDevice("0000:04:00.0");
    processingAccelerator.attributes["class"] = getMockSysfsHexValue(mockPciProcessingAcceleratorClassCode, 6);
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(2u, count);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenProcessingAcceleratorWithUnknownDeviceIdWhenCallingGetPciDevicePropertiesThenItIsNotReported) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    auto &npu = pciSysfs.addDevice("0000:00:0b.0");
    npu.attributes["class"] = getMockSysfsHexValue(mockPciProcessingAcceleratorClassCode, 6);
    npu.attributes["device"] = getMockSysfsHexValue(mockPciNpuDeviceId, 4);
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    ASSERT_EQ(1u, count);

    zes_intel_driver_pci_device_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_DRIVER_PCI_DEVICE_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    EXPECT_EQ(3u, properties.address.bus);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenDisplayControllerWithUnknownDeviceIdWhenCallingGetPciDevicePropertiesThenItIsNotReported) {
    MockPciSysfs pciSysfs;
    auto &displayController = pciSysfs.addDevice("0000:03:00.0");
    displayController.attributes["device"] = getMockSysfsHexValue(mockPciUnknownDeviceId, 4);
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenUnreadableSubsystemAndRevisionAttributesWhenCallingGetPciDevicePropertiesThenDeviceIsReportedWithZeroedValues) {
    MockPciSysfs pciSysfs;
    auto &device = pciSysfs.addDevice("0000:03:00.0");
    device.attributes.erase("subsystem_vendor");
    device.attributes.erase("subsystem_device");
    device.attributes.erase("revision");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 1;
    zes_intel_driver_pci_device_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    EXPECT_EQ(1u, count);
    EXPECT_EQ(0u, properties.configProperties.subsystemVendorId);
    EXPECT_EQ(0u, properties.configProperties.subsystemDeviceId);
    EXPECT_EQ(0u, properties.configProperties.revision);
    EXPECT_EQ(ZES_PCI_LINK_STATUS_GOOD, properties.status);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenUnreadableOrEmptyAttributesWhenCallingGetPciDevicePropertiesThenOnlyDevicesWithAReadableIdentityAreReported) {
    MockPciSysfs pciSysfs;
    auto &emptyVendorDevice = pciSysfs.addDevice("0000:03:00.0");
    emptyVendorDevice.attributes["vendor"] = "";

    auto &failingReadDevice = pciSysfs.addDevice("0000:04:00.0");
    failingReadDevice.attributeReadErrorNum = EIO;

    auto &emptyRevisionDevice = pciSysfs.addDevice("0000:05:00.0");
    emptyRevisionDevice.attributes["revision"] = "";

    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 3;
    zes_intel_driver_pci_device_properties_exp_t properties[3] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, properties));
    ASSERT_EQ(1u, count);
    EXPECT_EQ(5u, properties[0].address.bus);
    EXPECT_EQ(mockPciIntelVendorId, properties[0].configProperties.vendorId);
    EXPECT_EQ(0u, properties[0].configProperties.revision);
    EXPECT_EQ(ZES_PCI_LINK_STATUS_GOOD, properties[0].status);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenConfigSpaceReadingAllOnesWhenCallingGetPciDevicePropertiesThenLinkErrorIsReported) {
    MockPciSysfs pciSysfs;
    auto &device = pciSysfs.addDevice("0000:03:00.0");
    device.configHeader = getMockAllOnesConfigHeader();
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 1;
    zes_intel_driver_pci_device_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    EXPECT_EQ(1u, count);
    EXPECT_EQ(ZES_INTEL_PCI_LINK_STATUS_EXP_LINK_ERROR, properties.status);
    // The identity comes from the cached attributes, so it survives the device dying.
    EXPECT_EQ(mockPciIntelVendorId, properties.configProperties.vendorId);
    EXPECT_EQ(getMockPciDeviceId(), properties.configProperties.deviceId);
    EXPECT_EQ(mockPciRevision, properties.configProperties.revision);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenConfigSpaceWithAllOnesRevisionWhenCallingGetPciDevicePropertiesThenLinkErrorIsReported) {
    MockPciSysfs pciSysfs;
    auto &device = pciSysfs.addDevice("0000:03:00.0");
    device.configHeader[mockPciConfigRevisionIdOffset] = 0xffu;
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 1;
    zes_intel_driver_pci_device_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    EXPECT_EQ(ZES_INTEL_PCI_LINK_STATUS_EXP_LINK_ERROR, properties.status);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenConfigSpaceWithNonEndpointHeaderTypeWhenCallingGetPciDevicePropertiesThenLinkErrorIsReported) {
    for (const uint8_t headerType : {static_cast<uint8_t>(0x01u), static_cast<uint8_t>(0x7fu)}) {
        MockPciSysfs pciSysfs;
        auto &device = pciSysfs.addDevice("0000:03:00.0");
        device.configHeader[mockPciConfigHeaderTypeOffset] = headerType;
        pciSysfs.setupDirectoryListing();
        auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

        uint32_t count = 1;
        zes_intel_driver_pci_device_properties_exp_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
        EXPECT_EQ(ZES_INTEL_PCI_LINK_STATUS_EXP_LINK_ERROR, properties.status);
    }
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenMultiFunctionEndpointWhenCallingGetPciDevicePropertiesThenGoodStatusIsReported) {
    MockPciSysfs pciSysfs;
    auto &device = pciSysfs.addDevice("0000:03:00.0");
    device.configHeader[mockPciConfigHeaderTypeOffset] = 0x80u;
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 1;
    zes_intel_driver_pci_device_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    EXPECT_EQ(ZES_PCI_LINK_STATUS_GOOD, properties.status);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenSingleBarReadingAllOnesWhenCallingGetPciDevicePropertiesThenLinkErrorIsReported) {
    for (const uint32_t barIndex : {0u, 5u}) {
        MockPciSysfs pciSysfs;
        auto &device = pciSysfs.addDevice("0000:03:00.0");
        setMockConfigDword(device.configHeader, mockPciConfigBar0Offset + (barIndex * sizeof(uint32_t)), 0xffffffffu);
        pciSysfs.setupDirectoryListing();
        auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

        uint32_t count = 1;
        zes_intel_driver_pci_device_properties_exp_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
        EXPECT_EQ(ZES_INTEL_PCI_LINK_STATUS_EXP_LINK_ERROR, properties.status);
    }
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenBarsWhichAreNotAMasterAbortSignatureWhenCallingGetPciDevicePropertiesThenGoodStatusIsReported) {
    // An unimplemented BAR reads zero, and a sizing readback keeps its read only type bits, so
    // neither of them may be mistaken for the all ones master abort signature.
    for (const uint32_t bar : {0x00000000u, 0xfffffff0u, 0xfffffffdu}) {
        MockPciSysfs pciSysfs;
        auto &device = pciSysfs.addDevice("0000:03:00.0");
        setMockConfigDword(device.configHeader, mockPciConfigBar0Offset, bar);
        pciSysfs.setupDirectoryListing();
        auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

        uint32_t count = 1;
        zes_intel_driver_pci_device_properties_exp_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
        EXPECT_EQ(ZES_PCI_LINK_STATUS_GOOD, properties.status);
    }
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenConfigNodeIsNotAvailableWhenCallingGetPciDevicePropertiesThenDeviceIsNotReported) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    auto &hotRemovedDevice = pciSysfs.addDevice("0000:04:00.0");
    hotRemovedDevice.configOpenErrorNum = ENOENT;
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(1u, count);

    count = 2;
    zes_intel_driver_pci_device_properties_exp_t properties[2] = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, properties));
    EXPECT_EQ(1u, count);
    EXPECT_EQ(3u, properties[0].address.bus);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenConfigNodeOpenFailsForNonRootUserWhenCallingGetPciDevicePropertiesThenUnknownStatusIsReported) {
    for (const int errorNum : {EACCES, EPERM}) {
        MockPciSysfs pciSysfs;
        auto &device = pciSysfs.addDevice("0000:03:00.0");
        device.configOpenErrorNum = errorNum;
        pciSysfs.setupDirectoryListing();
        auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

        uint32_t count = 1;
        zes_intel_driver_pci_device_properties_exp_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
        EXPECT_EQ(1u, count);
        EXPECT_EQ(ZES_PCI_LINK_STATUS_UNKNOWN, properties.status);
    }
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenConfigNodeOpenFailsWhenCallingGetPciDevicePropertiesThenLinkErrorIsReported) {
    MockPciSysfs pciSysfs;
    auto &device = pciSysfs.addDevice("0000:03:00.0");
    device.configOpenErrorNum = EIO;
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 1;
    zes_intel_driver_pci_device_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    EXPECT_EQ(ZES_INTEL_PCI_LINK_STATUS_EXP_LINK_ERROR, properties.status);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenShortConfigSpaceReadWhenCallingGetPciDevicePropertiesThenLinkErrorIsReported) {
    MockPciSysfs pciSysfs;
    auto &device = pciSysfs.addDevice("0000:03:00.0");
    device.configBytesToRead = 32;
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 1;
    zes_intel_driver_pci_device_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    EXPECT_EQ(ZES_INTEL_PCI_LINK_STATUS_EXP_LINK_ERROR, properties.status);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenConfigSpaceReadFailsForNonRootUserWhenCallingGetPciDevicePropertiesThenUnknownStatusIsReported) {
    for (const int errorNum : {EACCES, EPERM}) {
        MockPciSysfs pciSysfs;
        auto &device = pciSysfs.addDevice("0000:03:00.0");
        device.configBytesToRead = -1;
        device.configReadErrorNum = errorNum;
        pciSysfs.setupDirectoryListing();
        auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

        uint32_t count = 1;
        zes_intel_driver_pci_device_properties_exp_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
        EXPECT_EQ(ZES_PCI_LINK_STATUS_UNKNOWN, properties.status);
    }
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenIntelGpuDeviceWhenCallingGetPciDevicePropertiesThenConfigSpaceIsNeverReadPastTheStandardHeader) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    pciSysfs.setupDirectoryListing();
    auto pLinuxDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();

    uint32_t count = 1;
    zes_intel_driver_pci_device_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxDriverImp->getPciDeviceProperties(&count, &properties));
    // Reading beyond offset 0x3f needs CAP_SYS_ADMIN and is silently truncated for anybody else.
    EXPECT_LE(pciSysfs.largestConfigReadRequest, mockPciConfigHeaderSize);
    EXPECT_NE(0u, pciSysfs.configOpenCallCount);
}

TEST_F(SysmanPciDevicePropertiesLinuxTest, GivenDriverHandleWithoutOsSysmanDriverWhenCallingGetPciDevicePropertiesThenOsSysmanDriverIsCreatedOnDemand) {
    MockPciSysfs pciSysfs;
    pciSysfs.addDevice("0000:03:00.0");
    pciSysfs.setupDirectoryListing();

    auto emptyDriverHandle = std::make_unique<L0::Sysman::SysmanDriverHandleImp>();
    ASSERT_TRUE(emptyDriverHandle->sysmanDevices.empty());
    ASSERT_EQ(nullptr, emptyDriverHandle->pOsSysmanDriver);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, emptyDriverHandle->getPciDeviceProperties(&count, nullptr));
    EXPECT_EQ(1u, count);
    EXPECT_NE(nullptr, emptyDriverHandle->pOsSysmanDriver);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
