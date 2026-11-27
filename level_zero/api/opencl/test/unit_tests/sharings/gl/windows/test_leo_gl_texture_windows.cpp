/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_gl_private_intel.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_texture.h"
#include "level_zero/api/opencl/source/sharings/gl/windows/leo_gl_sharing_windows.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_gl.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct StubGlTextureSharingFunctionsWindows : public GLSharingFunctionsWindows {
    StubGlTextureSharingFunctionsWindows() {
        glAcquireSharedTexture = &acquireSharedTextureStub;
    }

    static GLboolean OSAPI acquireSharedTextureStub(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
        auto resourceInfo = static_cast<CL_GL_RESOURCE_INFO *>(pResourceInfo);
        resourceInfo->globalShareHandle = 0x400;
        resourceInfo->glInternalFormat = glInternalFormat;
        resourceInfo->textureBufferWidth = textureBufferWidth;
        return GL_TRUE;
    }

    static constexpr GLint textureBufferWidth = 10;
    inline static GLint glInternalFormat = GL_RGBA8;
};

struct LeoGlTextureWindowsTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        cl_device_id clDeviceId = platform->getDevices()[0].get();
        capturingContext = std::make_unique<ImageDescCapturingContext>(driverHandle.get(), platform->getDevices()[0]->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
        ASSERT_EQ(CL_SUCCESS, leoContext->initialize());
        leoContext->registerSharing(new StubGlTextureSharingFunctionsWindows());
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    std::unique_ptr<ImageDescCapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
};

TEST(LeoGlTextureObjectTypeWindowsTest, givenGlTextureBufferTargetWhenGettingClObjectTypesThenImage1dBufferAndTextureBufferAreReturned) {
    EXPECT_EQ(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE1D_BUFFER), GlTexture::getClMemObjectType(GL_TEXTURE_BUFFER));
    EXPECT_EQ(static_cast<cl_gl_object_type>(CL_GL_OBJECT_TEXTURE_BUFFER), GlTexture::getClGlObjectType(GL_TEXTURE_BUFFER));
}

TEST_F(LeoGlTextureWindowsTest, givenGlTextureBufferWhenCreatingSharedGlTextureThenBufferImageWithWidthInBytesIsRequested) {
    cl_int retCode = CL_SUCCESS;
    auto image = GlTexture::createSharedGlTexture(leoContext.get(), CL_MEM_READ_WRITE, GL_TEXTURE_BUFFER, 0, 1u, &retCode);
    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retCode);

    ASSERT_EQ(1u, capturingContext->capturedImageDescs.size());
    const auto &imageDesc = capturingContext->capturedImageDescs[0];
    constexpr uint64_t rgba8TexelSize = 4u;
    EXPECT_EQ(ZE_IMAGE_TYPE_BUFFER, imageDesc.type);
    EXPECT_EQ(StubGlTextureSharingFunctionsWindows::textureBufferWidth * rgba8TexelSize, imageDesc.width);
    EXPECT_EQ(1u, imageDesc.height);
    EXPECT_EQ(1u, imageDesc.depth);
    EXPECT_EQ(0u, imageDesc.arraylevels);
    EXPECT_EQ(ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, imageDesc.format.layout);
}

TEST_F(LeoGlTextureWindowsTest, givenGlTextureBufferWithUnknownFormatWhenCreatingSharedGlTextureThenInvalidGlObjectIsReturnedAndNoImageIsRequested) {
    VariableBackup<GLint> glInternalFormatBackup(&StubGlTextureSharingFunctionsWindows::glInternalFormat, 0);
    cl_int retCode = CL_SUCCESS;
    auto image = GlTexture::createSharedGlTexture(leoContext.get(), CL_MEM_READ_WRITE, GL_TEXTURE_BUFFER, 0, 1u, &retCode);
    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retCode);
    EXPECT_EQ(0u, capturingContext->capturedImageDescs.size());
}

TEST_F(LeoGlTextureWindowsTest, givenGlTexture2dWithFormatUnsupportedForFlagsWhenCreatingSharedGlTextureThenInvalidGlObjectIsReturnedAndNoImageIsRequested) {
    VariableBackup<GLint> glInternalFormatBackup(&StubGlTextureSharingFunctionsWindows::glInternalFormat, GL_SRGB8_ALPHA8);
    cl_int retCode = CL_SUCCESS;
    auto image = GlTexture::createSharedGlTexture(leoContext.get(), CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, 1u, &retCode);
    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retCode);
    EXPECT_EQ(0u, capturingContext->capturedImageDescs.size());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
