/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/api/api.h"

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
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
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

    EXPECT_EQ(0u, device->getNumGenericSubDevices());

    cl_int retVal = CL_SUCCESS;

    if (device->getNumSubDevices() > 0) {
        retVal = clCreateSubDevices(device->getSubDevice(0), nullptr, 0, nullptr, nullptr);
    } else {
        retVal = clCreateSubDevices(device.get(), nullptr, 0, nullptr, nullptr);
    }

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
    EXPECT_EQ(device->getSubDevice(0), outDevices[0]);
    EXPECT_EQ(device->getSubDevice(1), outDevices[1]);

    properties[1] = CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE;
    cl_device_id outDevices2[2];
    retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices2, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(outDevices[0], outDevices2[0]);
    EXPECT_EQ(outDevices[1], outDevices2[1]);
}

TEST_F(clCreateSubDevicesTests, GivenValidInputWhenCreatingSubDevicesThenDeviceApiReferenceCountIsIncreasedEveryTime) {
    setup(2);

    EXPECT_EQ(0, device->getSubDevice(0)->getRefApiCount());
    EXPECT_EQ(0, device->getSubDevice(1)->getRefApiCount());

    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1, device->getSubDevice(0)->getRefApiCount());
    EXPECT_EQ(1, device->getSubDevice(1)->getRefApiCount());

    retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, device->getSubDevice(0)->getRefApiCount());
    EXPECT_EQ(2, device->getSubDevice(1)->getRefApiCount());
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
    cl_device_partition_property expectedRootDevicePartitionType[1] = {0};

    cl_device_id expectedSubDeviceParentDevice;
    cl_device_affinity_domain expectedSubDevicePartitionAffinityDomain = 0;
    cl_uint expectedSubDevicePartitionMaxSubDevices = 0;
    cl_device_partition_property expectedSubDevicePartitionProperties[1] = {0};
    cl_device_partition_property expectedSubDevicePartitionType[3] =
        {CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, CL_DEVICE_AFFINITY_DOMAIN_NUMA, 0};

    cl_device_id expectedRootDeviceWithoutSubDevicesParentDevice = nullptr;
    cl_device_affinity_domain expectedRootDeviceWithoutSubDevicesPartitionAffinityDomain = 0;
    cl_uint expectedRootDeviceWithoutSubDevicesPartitionMaxSubDevices = 0;
    cl_device_partition_property expectedRootDeviceWithoutSubDevicesPartitionProperties[1] = {0};
    cl_device_partition_property expectedRootDeviceWithoutSubDevicesPartitionType[1] = {0};

    cl_device_id parentDevice;
    cl_device_affinity_domain partitionAffinityDomain;
    cl_uint partitionMaxSubDevices;
    cl_device_partition_property partitionProperties[2];
    cl_device_partition_property partitionType[3];
    size_t returnValueSize;
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

        if (subDevice.getNumSubDevices() > 0) {
            for (uint32_t i = 0; i < subDevice.getNumSubDevices(); i++) {
                auto subSubDevice = subDevice.getSubDevice(i);
                auto &subSubDeviceInfo = subSubDevice->getDeviceInfo();
                EXPECT_EQ(expectedSubDevicePartitionAffinityDomain, subSubDeviceInfo.partitionAffinityDomain);
                EXPECT_EQ(expectedSubDevicePartitionMaxSubDevices, subSubDeviceInfo.partitionMaxSubDevices);
                EXPECT_EQ(expectedSubDevicePartitionProperties[0], subSubDeviceInfo.partitionProperties[0]);
                EXPECT_EQ(expectedSubDevicePartitionType[0], subSubDeviceInfo.partitionType[0]);
                EXPECT_EQ(expectedSubDevicePartitionType[1], subSubDeviceInfo.partitionType[1]);
                EXPECT_EQ(expectedSubDevicePartitionType[2], subSubDeviceInfo.partitionType[2]);
            }
            EXPECT_NE(expectedSubDevicePartitionAffinityDomain, subDeviceInfo.partitionAffinityDomain);
            EXPECT_NE(expectedSubDevicePartitionMaxSubDevices, subDeviceInfo.partitionMaxSubDevices);
            EXPECT_NE(expectedSubDevicePartitionProperties[0], subDeviceInfo.partitionProperties[0]);
            EXPECT_EQ(expectedSubDevicePartitionType[0], subDeviceInfo.partitionType[0]);
            EXPECT_EQ(expectedSubDevicePartitionType[1], subDeviceInfo.partitionType[1]);
            EXPECT_EQ(expectedSubDevicePartitionType[2], subDeviceInfo.partitionType[2]);
        } else {
            EXPECT_EQ(expectedSubDevicePartitionAffinityDomain, subDeviceInfo.partitionAffinityDomain);
            EXPECT_EQ(expectedSubDevicePartitionMaxSubDevices, subDeviceInfo.partitionMaxSubDevices);
            EXPECT_EQ(expectedSubDevicePartitionProperties[0], subDeviceInfo.partitionProperties[0]);
            EXPECT_EQ(expectedSubDevicePartitionType[0], subDeviceInfo.partitionType[0]);
            EXPECT_EQ(expectedSubDevicePartitionType[1], subDeviceInfo.partitionType[1]);
            EXPECT_EQ(expectedSubDevicePartitionType[2], subDeviceInfo.partitionType[2]);
        }
    }
}

TEST_F(clCreateSubDevicesDeviceInfoTests, GivenRootDeviceWithoutSubDevicesWhenGettingSubDeviceRelatedDeviceInfoThenCorrectValuesAreSet) {
    setup(1);

    auto &rootDeviceInfo = device->getDeviceInfo();
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesParentDevice, rootDeviceInfo.parentDevice);

    if (expectedRootDeviceWithoutSubDevicesPartitionAffinityDomain != rootDeviceInfo.partitionAffinityDomain) {
        EXPECT_EQ(0u, device->getNumGenericSubDevices());
        EXPECT_NE(0u, device->getNumSubDevices());

        EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionAffinityDomain, device->getSubDevice(0)->getDeviceInfo().partitionAffinityDomain);
        EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionMaxSubDevices, device->getSubDevice(0)->getDeviceInfo().partitionMaxSubDevices);
        EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionProperties[0], device->getSubDevice(0)->getDeviceInfo().partitionProperties[0]);
        EXPECT_NE(expectedRootDeviceWithoutSubDevicesPartitionType[0], device->getSubDevice(0)->getDeviceInfo().partitionType[0]);

    } else {
        EXPECT_EQ(0u, device->getNumGenericSubDevices());
        EXPECT_EQ(0u, device->getNumSubDevices());

        EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionMaxSubDevices, rootDeviceInfo.partitionMaxSubDevices);
        EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionProperties[0], rootDeviceInfo.partitionProperties[0]);
        EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionType[0], rootDeviceInfo.partitionType[0]);
    }
}

TEST_F(clCreateSubDevicesDeviceInfoTests, WhenGettingSubDeviceRelatedDeviceInfoViaApiThenCorrectValuesAreSet) {
    setup(4);

    size_t partitionPropertiesReturnValueSize = 0;
    size_t partitionTypeReturnValueSize = 0;

    clGetDeviceInfo(device.get(), CL_DEVICE_PARENT_DEVICE,
                    sizeof(parentDevice), &parentDevice, nullptr);
    EXPECT_EQ(expectedRootDeviceParentDevice, parentDevice);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
                    sizeof(partitionAffinityDomain), &partitionAffinityDomain, nullptr);
    EXPECT_EQ(expectedRootDevicePartitionAffinityDomain, partitionAffinityDomain);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_MAX_SUB_DEVICES,
                    sizeof(partitionMaxSubDevices), &partitionMaxSubDevices, nullptr);
    EXPECT_EQ(expectedRootDevicePartitionMaxSubDevices, partitionMaxSubDevices);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_PROPERTIES,
                    sizeof(partitionProperties), &partitionProperties, &partitionPropertiesReturnValueSize);
    EXPECT_EQ(sizeof(expectedRootDevicePartitionProperties), partitionPropertiesReturnValueSize);
    EXPECT_EQ(expectedRootDevicePartitionProperties[0], partitionProperties[0]);
    EXPECT_EQ(expectedRootDevicePartitionProperties[1], partitionProperties[1]);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_TYPE,
                    sizeof(partitionType), &partitionType, &partitionTypeReturnValueSize);
    EXPECT_EQ(sizeof(expectedRootDevicePartitionType), partitionTypeReturnValueSize);
    EXPECT_EQ(expectedRootDevicePartitionType[0], partitionType[0]);

    auto retVal = clCreateSubDevices(device.get(), properties, outDevicesCount, outDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto subDevice : outDevices) {
        auto neoSubDevice = castToObject<ClDevice>(subDevice);
        ASSERT_NE(nullptr, neoSubDevice);
        bool hasSubDevices = neoSubDevice->getNumSubDevices() > 0;

        clGetDeviceInfo(subDevice, CL_DEVICE_PARENT_DEVICE,
                        sizeof(parentDevice), &parentDevice, nullptr);

        clGetDeviceInfo(subDevice, CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
                        sizeof(partitionAffinityDomain), &partitionAffinityDomain, nullptr);

        clGetDeviceInfo(subDevice, CL_DEVICE_PARTITION_MAX_SUB_DEVICES,
                        sizeof(partitionMaxSubDevices), &partitionMaxSubDevices, nullptr);

        clGetDeviceInfo(subDevice, CL_DEVICE_PARTITION_PROPERTIES,
                        sizeof(partitionProperties), &partitionProperties, &partitionPropertiesReturnValueSize);

        clGetDeviceInfo(subDevice, CL_DEVICE_PARTITION_TYPE,
                        sizeof(partitionType), &partitionType, &partitionTypeReturnValueSize);

        EXPECT_EQ(expectedSubDeviceParentDevice, parentDevice);

        if (hasSubDevices) {
            EXPECT_NE(expectedSubDevicePartitionAffinityDomain, partitionAffinityDomain);
            EXPECT_NE(expectedSubDevicePartitionMaxSubDevices, partitionMaxSubDevices);
            EXPECT_NE(sizeof(expectedSubDevicePartitionProperties), partitionPropertiesReturnValueSize);
            EXPECT_NE(expectedSubDevicePartitionProperties[0], partitionProperties[0]);

            EXPECT_EQ(sizeof(expectedSubDevicePartitionType), partitionTypeReturnValueSize);

            EXPECT_EQ(expectedSubDevicePartitionType[0], partitionType[0]);
            EXPECT_EQ(expectedSubDevicePartitionType[1], partitionType[1]);
            EXPECT_EQ(expectedSubDevicePartitionType[2], partitionType[2]);

            auto neoSubDevice = castToObject<ClDevice>(subDevice);
            ASSERT_NE(nullptr, neoSubDevice);
            EXPECT_NE(0u, neoSubDevice->getNumSubDevices());

            cl_device_id clSubSubDevice = neoSubDevice->getSubDevice(0);

            clGetDeviceInfo(clSubSubDevice, CL_DEVICE_PARENT_DEVICE,
                            sizeof(parentDevice), &parentDevice, nullptr);
            EXPECT_EQ(subDevice, parentDevice);

            clGetDeviceInfo(clSubSubDevice, CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
                            sizeof(partitionAffinityDomain), &partitionAffinityDomain, nullptr);
            EXPECT_EQ(expectedSubDevicePartitionAffinityDomain, partitionAffinityDomain);

            clGetDeviceInfo(clSubSubDevice, CL_DEVICE_PARTITION_MAX_SUB_DEVICES,
                            sizeof(partitionMaxSubDevices), &partitionMaxSubDevices, nullptr);
            EXPECT_EQ(expectedSubDevicePartitionMaxSubDevices, partitionMaxSubDevices);

            clGetDeviceInfo(clSubSubDevice, CL_DEVICE_PARTITION_PROPERTIES,
                            sizeof(partitionProperties), &partitionProperties, &partitionPropertiesReturnValueSize);
            EXPECT_EQ(sizeof(expectedSubDevicePartitionProperties), partitionPropertiesReturnValueSize);
            EXPECT_EQ(expectedSubDevicePartitionProperties[0], partitionProperties[0]);

            clGetDeviceInfo(clSubSubDevice, CL_DEVICE_PARTITION_TYPE,
                            sizeof(partitionType), &partitionType, &partitionTypeReturnValueSize);
            EXPECT_EQ(sizeof(expectedSubDevicePartitionType), partitionTypeReturnValueSize);

            EXPECT_EQ(expectedSubDevicePartitionType[0], partitionType[0]);
            EXPECT_EQ(expectedSubDevicePartitionType[1], partitionType[1]);
            EXPECT_EQ(expectedSubDevicePartitionType[2], partitionType[2]);
        } else {
            EXPECT_EQ(expectedSubDeviceParentDevice, parentDevice);
            EXPECT_EQ(expectedSubDevicePartitionAffinityDomain, partitionAffinityDomain);
            EXPECT_EQ(expectedSubDevicePartitionMaxSubDevices, partitionMaxSubDevices);
            EXPECT_EQ(sizeof(expectedSubDevicePartitionProperties), partitionPropertiesReturnValueSize);
            EXPECT_EQ(expectedSubDevicePartitionProperties[0], partitionProperties[0]);

            EXPECT_EQ(sizeof(expectedSubDevicePartitionType), partitionTypeReturnValueSize);

            EXPECT_EQ(expectedSubDevicePartitionType[0], partitionType[0]);
            EXPECT_EQ(expectedSubDevicePartitionType[1], partitionType[1]);
            EXPECT_EQ(expectedSubDevicePartitionType[2], partitionType[2]);
        }
    }
}

TEST_F(clCreateSubDevicesDeviceInfoTests, GivenRootDeviceWithoutSubDevicesWhenGettingSubDeviceRelatedDeviceInfoViaApiThenCorrectValuesAreSet) {
    DebugManager.flags.EngineInstancedSubDevices.set(false);
    setup(1);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARENT_DEVICE,
                    sizeof(parentDevice), &parentDevice, nullptr);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesParentDevice, parentDevice);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
                    sizeof(partitionAffinityDomain), &partitionAffinityDomain, nullptr);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionAffinityDomain, partitionAffinityDomain);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_MAX_SUB_DEVICES,
                    sizeof(partitionMaxSubDevices), &partitionMaxSubDevices, nullptr);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionMaxSubDevices, partitionMaxSubDevices);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_PROPERTIES,
                    sizeof(partitionProperties), &partitionProperties, &returnValueSize);
    EXPECT_EQ(sizeof(expectedRootDeviceWithoutSubDevicesPartitionProperties), returnValueSize);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionProperties[0], partitionProperties[0]);

    clGetDeviceInfo(device.get(), CL_DEVICE_PARTITION_TYPE,
                    sizeof(partitionType), &partitionType, &returnValueSize);
    EXPECT_EQ(sizeof(expectedRootDeviceWithoutSubDevicesPartitionType), returnValueSize);
    EXPECT_EQ(expectedRootDeviceWithoutSubDevicesPartitionType[0], partitionType[0]);
}

} // namespace ULT
