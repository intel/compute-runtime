/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/api/api.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "test.h"

#include <memory>

using namespace NEO;

namespace ULT {

struct clCreateSubDevicesTests : ::testing::Test {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> mockDeviceCreateSingleDeviceBackup{&MockDevice::createSingleDevice};
    std::unique_ptr<MockClDevice> device;
    cl_device_partition_property properties[3] = {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, CL_DEVICE_AFFINITY_DOMAIN_NUMA, 0};
    cl_uint outDevicesCount;
    cl_device_id outDevices[4];

    void setup(int numberOfDevices) {
        DebugManager.flags.CreateMultipleSubDevices.set(numberOfDevices);
        mockDeviceCreateSingleDeviceBackup = (numberOfDevices == 1);
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        outDevicesCount = numberOfDevices;
    }
};

TEST_F(clCreateSubDevicesTests, GivenInvalidDeviceWhenCreatingSubDevicesThenInvalidDeviceErrorIsReturned) {
    auto retVal = clCreateSubDevices(
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreateSubDevicesTests, GivenDeviceWithoutSubDevicesWhenCreatingSubDevicesThenDevicePartitionFailedErrorIsReturned) {
    setup(1);

    auto retVal = clCreateSubDevices(device.get(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_DEVICE_PARTITION_FAILED, retVal);
}

TEST_F(clCreateSubDevicesTests, GivenInvalidOrUnsupportedPropertiesWhenCreatingSubDevicesThenInvalidValueErrorIsReturned) {
    setup(2);

    auto retVal = clCreateSubDevices(device.get(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    properties[0] = 0;
    retVal = clCreateSubDevices(device.get(), properties, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    properties[0] = CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN;
    properties[1] = 0;
    retVal = clCreateSubDevices(device.get(), properties, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    properties[1] = CL_DEVICE_AFFINITY_DOMAIN_NUMA;
    properties[2] = CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN;
    retVal = clCreateSubDevices(device.get(), properties, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateSubDevicesTests, GivenOutDevicesNullWhenCreatingSubDevicesThenSuccessIsReturned) {
    setup(2);

    cl_uint returnedOutDeviceCount = 0;
    auto retVal = clCreateSubDevices(device.get(), properties, 0, nullptr, &returnedOutDeviceCount);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, returnedOutDeviceCount);
}

TEST_F(clCreateSubDevicesTests, GivenOutDevicesTooSmallWhenCreatingSubDevicesThenInvalidValueErrorIsReturned) {
    setup(2);

    outDevicesCount = 1;
    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateSubDevicesTests, GivenValidInputWhenCreatingSubDevicesThenSubDevicesAreReturned) {
    setup(2);

    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(device->getDeviceById(0), outDevices[0]);
    EXPECT_EQ(device->getDeviceById(1), outDevices[1]);

    properties[1] = CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE;
    cl_device_id outDevices2[2];
    retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices2, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(outDevices[0], outDevices2[0]);
    EXPECT_EQ(outDevices[1], outDevices2[1]);
}

TEST_F(clCreateSubDevicesTests, GivenValidInputWhenCreatingSubDevicesThenDeviceApiReferenceCountIsIncreasedEveryTime) {
    setup(2);

    EXPECT_EQ(0, device->getDeviceById(0)->getRefApiCount());
    EXPECT_EQ(0, device->getDeviceById(1)->getRefApiCount());

    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1, device->getDeviceById(0)->getRefApiCount());
    EXPECT_EQ(1, device->getDeviceById(1)->getRefApiCount());

    retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, device->getDeviceById(0)->getRefApiCount());
    EXPECT_EQ(2, device->getDeviceById(1)->getRefApiCount());
}

struct clCreateSubDevicesDeviceInfoTests : clCreateSubDevicesTests {
    void setup(int numberOfDevices) {
        clCreateSubDevicesTests::setup(numberOfDevices);
        expectedSubDeviceParentDevice = device.get();
        expectedRootDevicePartitionMaxSubDevices = numberOfDevices;
    }

    cl_device_id expectedRootDeviceParentDevice = nullptr;
    cl_device_affinity_domain expectedRootDevicePartitionAffinityDomain =
        CL_DEVICE_AFFINITY_DOMAIN_NUMA | CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE;
    cl_uint expectedRootDevicePartitionMaxSubDevices;
    cl_device_partition_property expectedRootDevicePartitionProperties[2] = {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, 0};
    cl_device_partition_property expectedRootDevicePartitionType[3] = {0};

    cl_device_id expectedSubDeviceParentDevice;
    cl_device_affinity_domain expectedSubDevicePartitionAffinityDomain = 0;
    cl_uint expectedSubDevicePartitionMaxSubDevices = 0;
    cl_device_partition_property expectedSubDevicePartitionProperties[2] = {0};
    cl_device_partition_property expectedSubDevicePartitionType[3] =
        {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, CL_DEVICE_AFFINITY_DOMAIN_NUMA, 0};

    cl_device_id expectedRootDeviceWithoutSubDevicesParentDevice = nullptr;
    cl_device_affinity_domain expectedRootDeviceWithoutSubDevicesPartitionAffinityDomain = 0;
    cl_uint expectedRootDeviceWithoutSubDevicesPartitionMaxSubDevices = 0;
    cl_device_partition_property expectedRootDeviceWithoutSubDevicesPartitionProperties[2] = {0};
    cl_device_partition_property expectedRootDeviceWithoutSubDevicesPartitionType[3] = {0};

    cl_device_id parentDevice;
    cl_device_affinity_domain partitionAffinityDomain;
    cl_uint partitionMaxSubDevices;
    cl_device_partition_property partitionProperties[2];
    cl_device_partition_property partitionType[3];
};

TEST_F(clCreateSubDevicesDeviceInfoTests, WhenGettingSubDeviceRelatedDeviceInfoThenCorrectValuesAreSet) {
    setup(4);

    auto &rootDeviceInfo = device->getDeviceInfo();
    EXPECT_EQ(expectedRootDeviceParentDevice, rootDeviceInfo.parentDevice);
    EXPECT_EQ(expectedRootDevicePartitionAffinityDomain, rootDeviceInfo.partitionAffinityDomain);
    EXPECT_EQ(expectedRootDevicePartitionMaxSubDevices, rootDeviceInfo.partitionMaxSubDevices);
    EXPECT_EQ(expectedRootDevicePartitionProperties[0], rootDeviceInfo.partitionProperties[0]);
    EXPECT_EQ(expectedRootDevicePartitionProperties[1], rootDeviceInfo.partitionProperties[1]);
    EXPECT_EQ(expectedRootDevicePartitionType[0], rootDeviceInfo.partitionType[0]);

    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto outDevice : outDevices) {
        auto &subDevice = *castToObject<ClDevice>(outDevice);
        auto &subDeviceInfo = subDevice.getDeviceInfo();
        EXPECT_EQ(expectedSubDeviceParentDevice, subDeviceInfo.parentDevice);
        EXPECT_EQ(expectedSubDevicePartitionAffinityDomain, subDeviceInfo.partitionAffinityDomain);
        EXPECT_EQ(expectedSubDevicePartitionMaxSubDevices, subDeviceInfo.partitionMaxSubDevices);
        EXPECT_EQ(expectedSubDevicePartitionProperties[0], subDeviceInfo.partitionProperties[0]);
        EXPECT_EQ(expectedSubDevicePartitionType[0], subDeviceInfo.partitionType[0]);
        EXPECT_EQ(expectedSubDevicePartitionType[1], subDeviceInfo.partitionType[1]);
        EXPECT_EQ(expectedSubDevicePartitionType[2], subDeviceInfo.partitionType[2]);
    }
}

TEST_F(clCreateSubDevicesDeviceInfoTests, GivenRootDeviceWithoutSubDevicesWhenGettingSubDeviceRelatedDeviceInfoThenCorrectValuesAreSet) {
    setup(1);

    auto &rootDeviceInfo = device->getDeviceInfo();
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesParentDevice, rootDeviceInfo.parentDevice);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionAffinityDomain, rootDeviceInfo.partitionAffinityDomain);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionMaxSubDevices, rootDeviceInfo.partitionMaxSubDevices);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionProperties[0], rootDeviceInfo.partitionProperties[0]);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionType[0], rootDeviceInfo.partitionType[0]);
}

TEST_F(clCreateSubDevicesDeviceInfoTests, WhenGettingSubDeviceRelatedDeviceInfoViaApiThenCorrectValuesAreSet) {
    setup(4);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARENT_DEVICE, sizeof(parentDevice), &parentDevice, nullptr);
    EXPECT_EQ(expectedRootDeviceParentDevice, parentDevice);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_AFFINITY_DOMAIN, sizeof(partitionAffinityDomain), &partitionAffinityDomain, nullptr);
    EXPECT_EQ(expectedRootDevicePartitionAffinityDomain, partitionAffinityDomain);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_MAX_SUB_DEVICES, sizeof(partitionMaxSubDevices), &partitionMaxSubDevices, nullptr);
    EXPECT_EQ(expectedRootDevicePartitionMaxSubDevices, partitionMaxSubDevices);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_PROPERTIES, sizeof(partitionProperties), &partitionProperties, nullptr);
    EXPECT_EQ(expectedRootDevicePartitionProperties[0], partitionProperties[0]);
    EXPECT_EQ(expectedRootDevicePartitionProperties[1], partitionProperties[1]);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_TYPE, sizeof(partitionType), &partitionType, nullptr);
    EXPECT_EQ(expectedRootDevicePartitionType[0], partitionType[0]);

    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto subDevice : outDevices) {
        clGetDeviceInfo(subDevice, CL_DEVICE_PARENT_DEVICE, sizeof(parentDevice), &parentDevice, nullptr);
        EXPECT_EQ(expectedSubDeviceParentDevice, parentDevice);

        clGetDeviceInfo(subDevice, CL_DEVICE_PARTITION_AFFINITY_DOMAIN, sizeof(partitionAffinityDomain), &partitionAffinityDomain, nullptr);
        EXPECT_EQ(expectedSubDevicePartitionAffinityDomain, partitionAffinityDomain);

        clGetDeviceInfo(subDevice, CL_DEVICE_PARTITION_MAX_SUB_DEVICES, sizeof(partitionMaxSubDevices), &partitionMaxSubDevices, nullptr);
        EXPECT_EQ(expectedSubDevicePartitionMaxSubDevices, partitionMaxSubDevices);

        clGetDeviceInfo(subDevice, CL_DEVICE_PARTITION_PROPERTIES, sizeof(partitionProperties), &partitionProperties, nullptr);
        EXPECT_EQ(expectedSubDevicePartitionProperties[0], partitionProperties[0]);

        clGetDeviceInfo(subDevice, CL_DEVICE_PARTITION_TYPE, sizeof(partitionType), &partitionType, nullptr);
        EXPECT_EQ(expectedSubDevicePartitionType[0], partitionType[0]);
        EXPECT_EQ(expectedSubDevicePartitionType[1], partitionType[1]);
        EXPECT_EQ(expectedSubDevicePartitionType[2], partitionType[2]);
    }
}

TEST_F(clCreateSubDevicesDeviceInfoTests, GivenRootDeviceWithoutSubDevicesWhenGettingSubDeviceRelatedDeviceInfoViaApiThenCorrectValuesAreSet) {
    setup(1);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARENT_DEVICE, sizeof(parentDevice), &parentDevice, nullptr);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesParentDevice, parentDevice);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_AFFINITY_DOMAIN, sizeof(partitionAffinityDomain), &partitionAffinityDomain, nullptr);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionAffinityDomain, partitionAffinityDomain);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_MAX_SUB_DEVICES, sizeof(partitionMaxSubDevices), &partitionMaxSubDevices, nullptr);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionMaxSubDevices, partitionMaxSubDevices);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_PROPERTIES, sizeof(partitionProperties), &partitionProperties, nullptr);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionProperties[0], partitionProperties[0]);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_TYPE, sizeof(partitionType), &partitionType, nullptr);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionType[0], partitionType[0]);
}

} // namespace ULT
