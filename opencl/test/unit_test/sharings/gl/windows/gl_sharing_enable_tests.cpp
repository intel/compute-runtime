/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/memory_management_fixture.h"

#include "opencl/source/sharings/gl/windows/win_enable_gl.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

class GlSharingEnablerTests : public ::testing::Test {
  public:
    void SetUp() override {
        factory.reset(new GlSharingBuilderFactory());
        ASSERT_NE(nullptr, factory.get());
    }

    std::unique_ptr<GlSharingBuilderFactory> factory;
};

TEST_F(GlSharingEnablerTests, givenGlFactoryWhenAskedThenExtensionsAreReturned) {
    auto ext = factory->getExtensions(nullptr);
    EXPECT_GT(ext.length(), 0u);
    EXPECT_STRNE("", ext.c_str());
}

TEST_F(GlSharingEnablerTests, givenGlFactoryWhenAskedThenGlobalIcdIsConfigured) {
    class IcdRestore {
      public:
        IcdRestore() { icdSnapshot = icdGlobalDispatchTable; }
        ~IcdRestore() { icdGlobalDispatchTable = icdSnapshot; }
        decltype(icdGlobalDispatchTable) icdSnapshot;
    };
    // we play with global table, so first save state then restore it with use of RAII
    IcdRestore icdRestore;
    // clear ICD table
    icdGlobalDispatchTable.clCreateFromGLBuffer = nullptr;
    icdGlobalDispatchTable.clCreateFromGLTexture = nullptr;
    icdGlobalDispatchTable.clCreateFromGLTexture2D = nullptr;
    icdGlobalDispatchTable.clCreateFromGLTexture3D = nullptr;
    icdGlobalDispatchTable.clCreateFromGLRenderbuffer = nullptr;
    icdGlobalDispatchTable.clGetGLObjectInfo = nullptr;
    icdGlobalDispatchTable.clGetGLTextureInfo = nullptr;
    icdGlobalDispatchTable.clEnqueueAcquireGLObjects = nullptr;
    icdGlobalDispatchTable.clEnqueueReleaseGLObjects = nullptr;
    icdGlobalDispatchTable.clCreateEventFromGLsyncKHR = nullptr;
    icdGlobalDispatchTable.clGetGLContextInfoKHR = nullptr;

    factory->fillGlobalDispatchTable();
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clCreateFromGLBuffer);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clCreateFromGLTexture);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clCreateFromGLTexture2D);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clCreateFromGLTexture3D);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clCreateFromGLRenderbuffer);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clGetGLObjectInfo);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clGetGLTextureInfo);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clEnqueueAcquireGLObjects);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clEnqueueReleaseGLObjects);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clCreateEventFromGLsyncKHR);
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clGetGLContextInfoKHR);
}

TEST_F(GlSharingEnablerTests, givenGlFactoryWhenAskedThenBuilderIsCreated) {
    auto builder = factory->createContextBuilder();
    EXPECT_NE(nullptr, builder);
}

TEST_F(GlSharingEnablerTests, givenGlBuilderWhenUnknownPropertyThenFalseIsReturned) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    cl_context_properties property = CL_CONTEXT_PLATFORM;
    cl_context_properties value;
    auto res = builder->processProperties(property, value);
    EXPECT_FALSE(res);
}

TEST_F(GlSharingEnablerTests, givenGlBuilderWhenInvalidPropertyThenFalseIsReturned) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    cl_context_properties property = CL_CGL_SHAREGROUP_KHR;
    cl_context_properties value;
    auto res = builder->processProperties(property, value);
    EXPECT_FALSE(res);
}

TEST_F(GlSharingEnablerTests, givenGlBuilderWhenValidPropertyThenTrueIsReturned) {
    cl_context_properties props[] = {CL_GL_CONTEXT_KHR, CL_WGL_HDC_KHR, CL_GLX_DISPLAY_KHR, CL_EGL_DISPLAY_KHR};
    for (auto currProperty : props) {
        auto builder = factory->createContextBuilder();
        ASSERT_NE(nullptr, builder);

        cl_context_properties property = currProperty;
        cl_context_properties value = 0x10000;
        auto res = builder->processProperties(property, value);
        EXPECT_TRUE(res);

        // repeat to check if we don't allocate twice
        auto prevAllocations = MemoryManagement::numAllocations.load();
        res = builder->processProperties(property, value);
        EXPECT_TRUE(res);
        auto currAllocations = MemoryManagement::numAllocations.load();
        EXPECT_EQ(prevAllocations, currAllocations);
    }
}

TEST_F(GlSharingEnablerTests, givenGlBuilderWhenNoPropertiesThenFinalizerReturnsTrue) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    MockContext context;
    int32_t errcodeRet = CL_SUCCESS;
    auto res = builder->finalizeProperties(context, errcodeRet);
    EXPECT_TRUE(res);
    EXPECT_EQ(CL_SUCCESS, errcodeRet);
}

TEST_F(GlSharingEnablerTests, givenGlBuilderWhenInvalidPropertiesThenFinalizerReturnsTrue) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    cl_context_properties property = CL_CONTEXT_PLATFORM;
    cl_context_properties value;
    auto res = builder->processProperties(property, value);
    EXPECT_FALSE(res);

    MockContext context;
    int32_t errcodeRet = CL_SUCCESS;
    res = builder->finalizeProperties(context, errcodeRet);
    EXPECT_TRUE(res);
    EXPECT_EQ(CL_SUCCESS, errcodeRet);
}

TEST_F(GlSharingEnablerTests, givenGlBuilderWhenNullHandleThenFinalizerReturnsTrueAndNoSharingRegistered) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    cl_context_properties property = CL_GL_CONTEXT_KHR;
    cl_context_properties value = 0x0;
    auto res = builder->processProperties(property, value);
    EXPECT_TRUE(res);

    MockContext context;
    int32_t errcodeRet = CL_SUCCESS;
    res = builder->finalizeProperties(context, errcodeRet);
    EXPECT_TRUE(res);
    EXPECT_EQ(CL_SUCCESS, errcodeRet);

    auto sharing = context.getSharing<GLSharingFunctions>();
    EXPECT_EQ(nullptr, sharing);
}

TEST_F(GlSharingEnablerTests, givenGlBuilderWhenHandleThenFinalizerReturnsTrueAndSharingIsRegistered) {
    auto builder = factory->createContextBuilder();
    ASSERT_NE(nullptr, builder);

    cl_context_properties property = CL_GL_CONTEXT_KHR;
    cl_context_properties value = 0x1000;
    auto res = builder->processProperties(property, value);
    EXPECT_TRUE(res);

    MockContext context;
    int32_t errcodeRet = CL_SUCCESS;
    res = builder->finalizeProperties(context, errcodeRet);
    EXPECT_TRUE(res);
    EXPECT_EQ(CL_SUCCESS, errcodeRet);

    auto sharing = context.getSharing<GLSharingFunctions>();
    EXPECT_NE(nullptr, sharing);
}
