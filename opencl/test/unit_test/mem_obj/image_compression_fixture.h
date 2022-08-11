/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

class ImageCompressionTests : public ::testing::Test {
  public:
    class MyMemoryManager : public MockMemoryManager {
      public:
        using MockMemoryManager::MockMemoryManager;
        GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override {
            mockMethodCalled = true;
            capturedPreferCompressed = allocationData.flags.preferCompressed;
            return OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(allocationData);
        }
        bool mockMethodCalled = false;
        bool capturedPreferCompressed = false;
    };

    void SetUp() override {
        mockExecutionEnvironment = new MockExecutionEnvironment();
        myMemoryManager = new MyMemoryManager(*mockExecutionEnvironment);
        mockExecutionEnvironment->memoryManager.reset(myMemoryManager);

        mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(nullptr, mockExecutionEnvironment, 0u));
        mockContext = makeReleaseable<MockContext>(mockDevice.get());
    }

    MockExecutionEnvironment *mockExecutionEnvironment;
    std::unique_ptr<MockClDevice> mockDevice;
    ReleaseableObjectPtr<MockContext> mockContext;
    MyMemoryManager *myMemoryManager = nullptr;

    cl_image_desc imageDesc = {};
    cl_image_format imageFormat{CL_R, CL_UNSIGNED_INT8};
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_int retVal = CL_SUCCESS;
};
