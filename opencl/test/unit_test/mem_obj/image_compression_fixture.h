/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/utilities/base_object_utils.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

using namespace NEO;

class ImageCompressionTests : public ::testing::Test {
  public:
    class MyMemoryManager : public MockMemoryManager {
      public:
        using MockMemoryManager::MockMemoryManager;
        GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override {
            mockMethodCalled = true;
            capturedImgInfo = *allocationData.imgInfo;
            return OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(allocationData);
        }
        ImageInfo capturedImgInfo = {};
        bool mockMethodCalled = false;
    };

    void SetUp() override {
        mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        myMemoryManager = new MyMemoryManager(*mockDevice->getExecutionEnvironment());
        mockDevice->injectMemoryManager(myMemoryManager);
        mockContext = make_releaseable<MockContext>(mockDevice.get());
    }

    std::unique_ptr<MockClDevice> mockDevice;
    ReleaseableObjectPtr<MockContext> mockContext;
    MyMemoryManager *myMemoryManager = nullptr;

    cl_image_desc imageDesc = {};
    cl_image_format imageFormat{CL_R, CL_UNSIGNED_INT8};
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_int retVal = CL_SUCCESS;
};
