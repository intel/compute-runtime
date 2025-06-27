/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ContextMultiDevice, GivenSingleDeviceWhenCreatingContextThenContextIsCreated) {
    cl_device_id devices[] = {
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)}};
    auto numDevices = static_cast<cl_uint>(arrayCount(devices));

    auto retVal = CL_SUCCESS;
    auto pContext = Context::create<Context>(nullptr, ClDeviceVector(devices, numDevices),
                                             nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, pContext);

    auto numDevicesReturned = pContext->getNumDevices();
    EXPECT_EQ(numDevices, numDevicesReturned);

    ClDeviceVector ctxDevices;
    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        ctxDevices.push_back(pContext->getDevice(deviceOrdinal));
    }

    delete pContext;

    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        auto pDevice = (ClDevice *)devices[deviceOrdinal];
        ASSERT_NE(nullptr, pDevice);

        EXPECT_EQ(pDevice, ctxDevices[deviceOrdinal]);
        delete pDevice;
    }
}

TEST(ContextMultiDevice, GivenMultipleDevicesWhenCreatingContextThenContextIsCreatedForEachDevice) {
    cl_device_id devices[] = {
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)},
        new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)}};
    auto numDevices = static_cast<cl_uint>(arrayCount(devices));
    ASSERT_EQ(8u, numDevices); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    auto retVal = CL_SUCCESS;
    auto pContext = Context::create<Context>(nullptr, ClDeviceVector(devices, numDevices),
                                             nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, pContext);

    auto numDevicesReturned = pContext->getNumDevices();
    EXPECT_EQ(numDevices, numDevicesReturned);

    ClDeviceVector ctxDevices;
    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        ctxDevices.push_back(pContext->getDevice(deviceOrdinal));
    }

    delete pContext;

    for (size_t deviceOrdinal = 0; deviceOrdinal < numDevicesReturned; ++deviceOrdinal) {
        auto pDevice = (ClDevice *)devices[deviceOrdinal];
        ASSERT_NE(nullptr, pDevice);

        EXPECT_EQ(pDevice, ctxDevices[deviceOrdinal]);
        delete pDevice;
    }
}

TEST(ContextMultiDevice, WhenGettingSubDeviceByIndexFromContextThenCorrectDeviceIsReturned) {
    DebugManagerStateRestore restorer;
    VariableBackup<bool> createSingleDeviceBackup{&MockDevice::createSingleDevice, false};
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createRootDeviceFuncBackup{&DeviceFactory::createRootDeviceFunc};

    debugManager.flags.CreateMultipleSubDevices.set(2);
    createRootDeviceFuncBackup = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return std::unique_ptr<Device>(MockDevice::create<MockDevice>(&executionEnvironment, rootDeviceIndex));
    };

    auto executionEnvironment = new ExecutionEnvironment;
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    auto pRootDevice = std::make_unique<MockClDevice>(static_cast<MockDevice *>(devices[0].release()));
    auto pSubDevice0 = pRootDevice->subDevices[0].get();
    auto pSubDevice1 = pRootDevice->subDevices[1].get();

    cl_device_id allDevices[3]{};
    cl_device_id onlyRootDevices[1]{};
    cl_device_id onlySubDevices[2]{};

    allDevices[0] = onlyRootDevices[0] = pRootDevice.get();
    allDevices[1] = onlySubDevices[0] = pSubDevice0;
    allDevices[2] = onlySubDevices[1] = pSubDevice1;

    cl_int retVal;
    auto pContextWithAllDevices = std::unique_ptr<Context>(Context::create<Context>(nullptr, ClDeviceVector(allDevices, 3),
                                                                                    nullptr, nullptr, retVal));
    EXPECT_NE(nullptr, pContextWithAllDevices);

    auto pContextWithRootDevices = std::unique_ptr<Context>(Context::create<Context>(nullptr, ClDeviceVector(onlyRootDevices, 1),
                                                                                     nullptr, nullptr, retVal));
    EXPECT_NE(nullptr, pContextWithRootDevices);

    auto pContextWithSubDevices = std::unique_ptr<Context>(Context::create<Context>(nullptr, ClDeviceVector(onlySubDevices, 2),
                                                                                    nullptr, nullptr, retVal));
    EXPECT_NE(nullptr, pContextWithSubDevices);

    EXPECT_EQ(pSubDevice0, pContextWithAllDevices->getSubDeviceByIndex(0));
    EXPECT_EQ(nullptr, pContextWithRootDevices->getSubDeviceByIndex(0));
    EXPECT_EQ(pSubDevice0, pContextWithSubDevices->getSubDeviceByIndex(0));

    EXPECT_EQ(pSubDevice1, pContextWithAllDevices->getSubDeviceByIndex(1));
    EXPECT_EQ(nullptr, pContextWithRootDevices->getSubDeviceByIndex(1));
    EXPECT_EQ(pSubDevice1, pContextWithSubDevices->getSubDeviceByIndex(1));
}

TEST(ContextMultiDevice, givenContextWithNonDefaultContextTypeWhenSetupContextTypeThenDoNothing) {
    UltClDeviceFactory deviceFactory{1, 2};

    MockContext context0(deviceFactory.rootDevices[0]);
    context0.contextType = ContextType::CONTEXT_TYPE_DEFAULT;
    context0.setupContextType();
    EXPECT_EQ(ContextType::CONTEXT_TYPE_DEFAULT, context0.peekContextType());

    MockContext context1(deviceFactory.rootDevices[0]);
    context1.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    context1.setupContextType();
    EXPECT_EQ(ContextType::CONTEXT_TYPE_SPECIALIZED, context1.peekContextType());

    MockContext context2(deviceFactory.rootDevices[0]);
    context2.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context2.setupContextType();
    EXPECT_EQ(ContextType::CONTEXT_TYPE_UNRESTRICTIVE, context2.peekContextType());

    MockContext context3(deviceFactory.subDevices[0]);
    context3.contextType = ContextType::CONTEXT_TYPE_DEFAULT;
    context3.setupContextType();
    EXPECT_EQ(ContextType::CONTEXT_TYPE_SPECIALIZED, context3.peekContextType());

    MockContext context4(deviceFactory.subDevices[0]);
    context4.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    context4.setupContextType();
    EXPECT_EQ(ContextType::CONTEXT_TYPE_UNRESTRICTIVE, context4.peekContextType());
}

TEST(ContextMultiDevice, givenRootDeviceWhenCreatingContextThenItHasDefaultType) {
    UltClDeviceFactory deviceFactory{1, 2};
    cl_int retVal = CL_INVALID_CONTEXT;
    cl_device_id device = deviceFactory.rootDevices[0];

    auto context = clUniquePtr(Context::create<Context>(nullptr, ClDeviceVector(&device, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context.get());
    EXPECT_EQ(ContextType::CONTEXT_TYPE_DEFAULT, context->peekContextType());
}

TEST(ContextMultiDevice, givenSubsetOfSubdevicesWhenCreatingContextThenItHasSpecializedType) {
    UltClDeviceFactory deviceFactory{1, 2};
    cl_int retVal = CL_INVALID_CONTEXT;
    cl_device_id firstSubDevice = deviceFactory.subDevices[0];
    cl_device_id secondSubDevice = deviceFactory.subDevices[1];
    cl_device_id bothSubDevices[]{firstSubDevice, secondSubDevice};

    auto context0 = clUniquePtr(Context::create<Context>(nullptr, ClDeviceVector(&firstSubDevice, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context0.get());
    EXPECT_EQ(ContextType::CONTEXT_TYPE_SPECIALIZED, context0->peekContextType());

    retVal = CL_INVALID_CONTEXT;
    auto context1 = clUniquePtr(Context::create<Context>(nullptr, ClDeviceVector(&secondSubDevice, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context1.get());
    EXPECT_EQ(ContextType::CONTEXT_TYPE_SPECIALIZED, context1->peekContextType());

    retVal = CL_INVALID_CONTEXT;
    auto context2 = clUniquePtr(Context::create<Context>(nullptr, ClDeviceVector(bothSubDevices, 2), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context2.get());
    EXPECT_EQ(ContextType::CONTEXT_TYPE_SPECIALIZED, context2->peekContextType());
}

TEST(ContextMultiDevice, givenRootDeviceAndSubsetOfSubdevicesWhenCreatingContextThenItHasUnrestrictiveType) {
    UltClDeviceFactory deviceFactory{1, 2};
    cl_int retVal = CL_INVALID_CONTEXT;
    cl_device_id rootDeviceAndFirstSubDevice[]{deviceFactory.subDevices[0], deviceFactory.rootDevices[0]};
    cl_device_id rootDeviceAndSecondSubDevice[]{deviceFactory.subDevices[1], deviceFactory.rootDevices[0]};
    cl_device_id rootDeviceAndBothSubDevices[]{deviceFactory.subDevices[0], deviceFactory.subDevices[1], deviceFactory.rootDevices[0]};

    auto context0 = clUniquePtr(Context::create<Context>(nullptr, ClDeviceVector(rootDeviceAndFirstSubDevice, 2), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context0.get());
    EXPECT_EQ(ContextType::CONTEXT_TYPE_UNRESTRICTIVE, context0->peekContextType());

    retVal = CL_INVALID_CONTEXT;
    auto context1 = clUniquePtr(Context::create<Context>(nullptr, ClDeviceVector(rootDeviceAndSecondSubDevice, 2), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context1.get());
    EXPECT_EQ(ContextType::CONTEXT_TYPE_UNRESTRICTIVE, context1->peekContextType());

    retVal = CL_INVALID_CONTEXT;
    auto context2 = clUniquePtr(Context::create<Context>(nullptr, ClDeviceVector(rootDeviceAndBothSubDevices, 3), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context2.get());
    EXPECT_EQ(ContextType::CONTEXT_TYPE_UNRESTRICTIVE, context2->peekContextType());
}
