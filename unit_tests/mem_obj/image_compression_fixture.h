/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/surface_formats.h"
#include "test.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/utilities/base_object_utils.h"

using namespace OCLRT;

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
        mockDevice.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        myMemoryManager = new MyMemoryManager(*mockDevice->getExecutionEnvironment());
        mockDevice->injectMemoryManager(myMemoryManager);
        mockContext = make_releaseable<MockContext>(mockDevice.get());
    }

    std::unique_ptr<MockDevice> mockDevice;
    ReleaseableObjectPtr<MockContext> mockContext;
    MyMemoryManager *myMemoryManager = nullptr;

    cl_image_desc imageDesc = {};
    cl_image_format imageFormat{CL_RGBA, CL_UNORM_INT8};
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_int retVal = CL_SUCCESS;
};
