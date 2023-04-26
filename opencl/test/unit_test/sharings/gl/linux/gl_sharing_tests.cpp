/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sharings/gl/cl_gl_api_intel.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"
#include "opencl/source/sharings/gl/gl_buffer.h"
#include "opencl/source/sharings/gl/gl_context_guard.h"
#include "opencl/source/sharings/gl/gl_sync_event.h"
#include "opencl/source/sharings/gl/gl_texture.h"
#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"
#include "opencl/source/sharings/gl/linux/include/gl_types.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/mocks/gl/linux/mock_gl_arb_sync_event_linux.h"
#include "opencl/test/unit_test/mocks/gl/linux/mock_gl_sharing_linux.h"
#include "opencl/test/unit_test/mocks/mock_async_event_handler.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"

using namespace NEO;

class GlSharingTests : public ::testing::Test {
  public:
    void SetUp() override {
        rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
        mockGlSharingFunctions = mockGlSharing->sharingFunctions.release();
        context.setSharingFunctions(mockGlSharingFunctions);

        mockGlSharing->bufferInfoOutput.globalShareHandle = bufferId;
        mockGlSharing->bufferInfoOutput.bufferSize = 4096u;
        mockGlSharing->uploadDataToBufferInfo();
    }

    uint32_t rootDeviceIndex;
    MockContext context;
    std::unique_ptr<MockGlSharing> mockGlSharing = std::make_unique<MockGlSharing>();
    GlSharingFunctionsMock *mockGlSharingFunctions;
    unsigned int bufferId = 1u;
};

TEST(APIclCreateEventFromGLsyncKHR, givenInvalidContexWhenCreateThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    cl_GLsync sync = {0};

    auto event = clCreateEventFromGLsyncKHR(nullptr, sync, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, event);
}

using clGetSupportedGLTextureFormatsINTELTests = GlSharingTests;

TEST_F(clGetSupportedGLTextureFormatsINTELTests, givenContextWithoutGlSharingWhenGettingFormatsThenInvalidContextErrorIsReturned) {
    MockContext context;

    auto retVal = clGetSupportedGLTextureFormatsINTEL(&context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clGetSupportedGLTextureFormatsINTELTests, givenValidInputsWhenGettingFormatsThenSuccesAndValidFormatsAreReturned) {
    cl_uint numFormats = 0;
    cl_GLenum glFormats[2] = {};
    auto glFormatsCount = static_cast<cl_uint>(arrayCount(glFormats));

    auto retVal = clGetSupportedGLTextureFormatsINTEL(&context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                      glFormatsCount, glFormats, &numFormats);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, numFormats);

    for (uint32_t i = 0; i < glFormatsCount; i++) {
        EXPECT_NE(GlSharing::glToCLFormats.end(), GlSharing::glToCLFormats.find(glFormats[i]));
    }
}
