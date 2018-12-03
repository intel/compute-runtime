/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/gl/mock_gl_sharing.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "runtime/sharings/gl/gl_texture.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/libult/create_command_stream.h"
#include "gtest/gtest.h"

using namespace OCLRT;

class GlSharingTextureTests : public ::testing::Test {
  public:
    // temp solution - we need to query size from GMM:
    class TempMM : public MockMemoryManager {
      public:
        using MockMemoryManager::MockMemoryManager;
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override {
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, requireSpecificBitness);
            if (useForcedGmm) {
                alloc->gmm = forceGmm.get();
            }
            return alloc;
        }
        void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
            if (useForcedGmm) {
                forceGmm.release();
            }
            OsAgnosticMemoryManager::freeGraphicsMemoryImpl(gfxAllocation);
        }
        size_t forceAllocationSize;
        std::unique_ptr<Gmm> forceGmm;
        bool useForcedGmm = true;
    };

    void SetUp() override {
        executionEnvironment.incRefInternal();
        imgDesc = {};
        imgDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
        imgDesc.image_width = 10;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        glSharing = new MockGlSharing;

        tempMM = new TempMM(executionEnvironment);
        executionEnvironment.memoryManager.reset(tempMM);
        device.reset(MockDevice::create<MockDevice>(*platformDevices, &executionEnvironment, 0));
        clContext = std::make_unique<MockContext>(device.get());

        clContext->setSharingFunctions(&glSharing->m_sharingFunctions);
        ASSERT_FALSE(overrideCommandStreamReceiverCreation);

        tempMM->forceGmm = MockGmm::queryImgParams(imgInfo);
        tempMM->forceAllocationSize = textureSize;
        textureSize = imgInfo.size;
        textureId = 1;
    }

    void TearDown() override {
        clContext->releaseSharingFunctions(SharingType::CLGL_SHARING);
        delete glSharing;
    }

    ExecutionEnvironment executionEnvironment;
    cl_image_desc imgDesc;
    TempMM *tempMM;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> clContext;
    MockGlSharing *glSharing;
    size_t textureSize;
    unsigned int textureId;
};

TEST_F(GlSharingTextureTests, givenMockGlWhen1dGlTextureIsCreatedThenMemObjectHasGlHandler) {
    //this will create OS specific memory Manager
    //overrideCommandStreamReceiverCreation = true;
    ASSERT_FALSE(overrideCommandStreamReceiverCreation);
    cl_int retVal = CL_INVALID_VALUE;

    glSharing->uploadDataToTextureInfo(textureId);

    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_1D, 0, textureId, &retVal);
    ASSERT_NE(nullptr, glTexture);
    EXPECT_NE(nullptr, glTexture->getGraphicsAllocation());
    EXPECT_EQ(textureSize, glTexture->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(1, GLAcquireSharedTextureCalled);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(textureId, textureInfoInput.name);

    auto handler = glTexture->peekSharingHandler();
    ASSERT_NE(nullptr, handler);
    auto glHandler = static_cast<GlSharing *>(handler);

    EXPECT_EQ(glHandler->peekFunctionsHandler(), &glSharing->m_sharingFunctions);

    delete glTexture;
}

class FailingMemoryManager : public MockMemoryManager {
  public:
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override {
        return nullptr;
    }
};

TEST_F(GlSharingTextureTests, givenMockGlWhenGlTextureIsCreatedFromWrongHandleThenErrorAndNoTextureIsReturned) {
    auto tempMemoryManager = clContext->getMemoryManager();
    tempMM->useForcedGmm = false;
    auto memoryManager = std::unique_ptr<FailingMemoryManager>(new FailingMemoryManager());
    clContext->setMemoryManager(memoryManager.get());

    auto retVal = CL_SUCCESS;
    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_1D, 0, textureId, &retVal);

    EXPECT_EQ(nullptr, glTexture);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);

    clContext->setMemoryManager(tempMemoryManager);
}

GLboolean OSAPI mockGLAcquireSharedTexture(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    auto pTextureInfo = (CL_GL_RESOURCE_INFO *)pResourceInfo;
    textureInfoInput = *pTextureInfo;
    pTextureInfo->globalShareHandle = textureInfoOutput.globalShareHandle;
    pTextureInfo->globalShareHandleMCS = textureInfoOutput.globalShareHandleMCS;
    if (pTextureInfo->target == GL_TEXTURE_BUFFER) {
        // size and width for texture buffer are queried from textureInfo - not from gmm
        pTextureInfo->textureBufferSize = textureInfoOutput.textureBufferSize;
        pTextureInfo->textureBufferWidth = textureInfoOutput.textureBufferWidth;
    }
    pTextureInfo->pGmmResInfo = textureInfoOutput.pGmmResInfo;
    pTextureInfo->glInternalFormat = 99999;
    pTextureInfo->glHWFormat = textureInfoOutput.glHWFormat;
    pTextureInfo->textureBufferOffset = textureInfoOutput.textureBufferOffset;
    return (GLboolean)1;
};

TEST_F(GlSharingTextureTests, givenMockGlWhenGlTextureIsCreatedFromIncorrectFormatThenErrorAndNoTextureIsReturned) {
    glSharing->m_sharingFunctions.setGLAcquireSharedTextureMock(mockGLAcquireSharedTexture);

    auto retVal = CL_SUCCESS;
    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_1D, 0, textureId, &retVal);

    EXPECT_EQ(nullptr, glTexture);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);
}

TEST_F(GlSharingTextureTests, givenMockGlWhenRenderBufferTextureIsCreatedThenMemObjectHasGlHandler) {
    //this will create OS specific memory Manager
    //overrideCommandStreamReceiverCreation = true;
    ASSERT_FALSE(overrideCommandStreamReceiverCreation);
    cl_int retVal = CL_INVALID_VALUE;

    glSharing->uploadDataToTextureInfo(textureId);

    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_RENDERBUFFER_EXT, 0, textureId, &retVal);
    ASSERT_NE(nullptr, glTexture);
    EXPECT_NE(nullptr, glTexture->getGraphicsAllocation());
    EXPECT_EQ(textureSize, glTexture->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(1, GLAcquireSharedRenderBufferCalled);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(textureId, textureInfoInput.name);

    auto handler = glTexture->peekSharingHandler();
    ASSERT_NE(nullptr, handler);
    auto glHandler = static_cast<GlSharing *>(handler);

    EXPECT_EQ(glHandler->peekFunctionsHandler(), &glSharing->m_sharingFunctions);

    delete glTexture;
}

TEST_F(GlSharingTextureTests, givenGmmResourceAsInputeWhenTextureIsCreatedItHasGmmSet) {
    cl_int retVal = CL_INVALID_VALUE;

    glSharing->m_textureInfoOutput.globalShareHandle = textureId;
    glSharing->m_textureInfoOutput.pGmmResInfo = this->tempMM->forceGmm->gmmResourceInfo->peekHandle();
    this->tempMM->useForcedGmm = false;
    glSharing->m_textureInfoOutput.pGmmResInfo = this->tempMM->forceGmm->gmmResourceInfo->peekHandle();

    glSharing->uploadDataToTextureInfo();

    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_1D, 0, textureId, &retVal);
    ASSERT_NE(nullptr, glTexture);
    EXPECT_NE(nullptr, glTexture->getGraphicsAllocation());

    ASSERT_NE(nullptr, glTexture->getGraphicsAllocation()->gmm);
    ASSERT_NE(nullptr, glTexture->getGraphicsAllocation()->gmm->gmmResourceInfo->peekHandle());

    delete glTexture;
}

TEST_F(GlSharingTextureTests, givenDifferentHwFormatWhenSurfaceFormatInfoIsSetThenOverwrite) {
    cl_int retVal = CL_INVALID_VALUE;
    cl_image_format imageFormat = {};
    GlTexture::setClImageFormat(GL_DEPTH32F_STENCIL8, imageFormat);
    auto format = Image::getSurfaceFormatFromTable(CL_MEM_READ_ONLY, &imageFormat);
    ASSERT_NE(format, nullptr);
    auto newHwFormat = 217u;

    EXPECT_TRUE(format->GenxSurfaceFormat != newHwFormat);

    glSharing->m_textureInfoOutput.glHWFormat = newHwFormat;
    glSharing->m_textureInfoOutput.glInternalFormat = GL_DEPTH32F_STENCIL8;

    glSharing->uploadDataToTextureInfo();

    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, textureId, &retVal);
    ASSERT_NE(nullptr, glTexture);
    EXPECT_TRUE(newHwFormat == glTexture->getSurfaceFormatInfo().GenxSurfaceFormat);

    delete glTexture;
}

TEST_F(GlSharingTextureTests, givenGLRGB10FormatWhenSharedGlTextureIsCreatedThenItHasCorrectGenxSurfaceFormatAssigned) {
    cl_int retVal = CL_INVALID_VALUE;
    glSharing->m_textureInfoOutput.glInternalFormat = GL_RGB10;
    glSharing->m_textureInfoOutput.glHWFormat = GFX3DSTATE_SURFACEFORMAT_R16G16B16X16_UNORM;
    glSharing->uploadDataToTextureInfo();

    std::unique_ptr<Image> glTexture(GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, textureId, &retVal));
    ASSERT_NE(nullptr, glTexture);
    EXPECT_EQ(glTexture->getSurfaceFormatInfo().GenxSurfaceFormat, GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM);
}

TEST_F(GlSharingTextureTests, givenContextAnd1dTextureWhenClCreateFromGlTextureIsCalledThenImageIsReturned) {
    cl_int retVal = CL_INVALID_GL_OBJECT;
    auto glImage = clCreateFromGLTexture(clContext.get(), 0, GL_TEXTURE_1D, 0, textureId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glImage);

    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTextureTests, givenContextWithoutSharingAnd1dTextureWhenClCreateFromGlTextureIsCalledThenErrorIsReturned) {
    tempMM->useForcedGmm = false;
    clContext->releaseSharingFunctions(CLGL_SHARING);
    cl_int retVal = CL_INVALID_GL_OBJECT;
    auto glImage = clCreateFromGLTexture(clContext.get(), 0, GL_TEXTURE_1D, 0, textureId, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    ASSERT_EQ(nullptr, glImage);
}

TEST_F(GlSharingTextureTests, givenContextAndRenderBufferTextureWhenClCreateFromGlTextureIsCalledThenImageIsReturned) {
    cl_int retVal = CL_INVALID_GL_OBJECT;
    auto glImage = clCreateFromGLRenderbuffer(clContext.get(), 0, textureId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glImage);

    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTextureTests, givenContextWithoutSharingAndRenderBufferTextureWhenClCreateFromGlTextureIsCalledThenErrorIsReturned) {
    tempMM->useForcedGmm = false;
    clContext->releaseSharingFunctions(CLGL_SHARING);
    cl_int retVal = CL_INVALID_GL_OBJECT;
    auto glImage = clCreateFromGLRenderbuffer(clContext.get(), 0, textureId, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    ASSERT_EQ(nullptr, glImage);
}

TEST_F(GlSharingTextureTests, givenGlCl1dTextureWhenAskedForCLGLGetInfoThenIdAndTypeIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glImage = clCreateFromGLTexture(clContext.get(), 0, GL_TEXTURE_1D, 0, textureId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glImage);

    cl_gl_object_type objectType = 0u;
    cl_GLuint objectId = 0u;

    retVal = clGetGLObjectInfo(glImage, &objectType, &objectId);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(objectType, (cl_gl_object_type)CL_GL_OBJECT_TEXTURE1D);
    EXPECT_EQ(objectId, textureId);

    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTextureTests, givenHwCommandQueueAndGlTextureWhenItIsCreatedWithClCreateFromGlTexture2dThenImageObjectIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glImage = clCreateFromGLTexture2D(clContext.get(), 0, GL_TEXTURE_2D, 0, textureId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glImage);

    cl_gl_object_type objectType = 0u;
    cl_GLuint objectId = 0u;

    retVal = clGetGLObjectInfo(glImage, &objectType, &objectId);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(objectType, (cl_gl_object_type)CL_GL_OBJECT_TEXTURE2D);
    EXPECT_EQ(objectId, textureId);

    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTextureTests, givenContextWithoutSharingAndGlTextureWhenItIsCreatedWithClCreateFromGlTexture2dThenErrorIsReturned) {
    tempMM->useForcedGmm = false;
    clContext->releaseSharingFunctions(CLGL_SHARING);
    auto retVal = CL_SUCCESS;
    auto glImage = clCreateFromGLTexture2D(clContext.get(), 0, GL_TEXTURE_2D, 0, textureId, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    ASSERT_EQ(nullptr, glImage);
}

TEST_F(GlSharingTextureTests, givenHwCommandQueueAndGlTextureWhenItIsCreatedWithClCreateFromGlTexture3dThenImageObjectIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glImage = clCreateFromGLTexture3D(clContext.get(), 0, GL_TEXTURE_3D, 0, textureId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glImage);

    cl_gl_object_type objectType = 0u;
    cl_GLuint objectId = 0u;

    retVal = clGetGLObjectInfo(glImage, &objectType, &objectId);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(objectType, (cl_gl_object_type)CL_GL_OBJECT_TEXTURE3D);
    EXPECT_EQ(objectId, textureId);

    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTextureTests, givenContextWithoutSharingAndGlTextureWhenItIsCreatedWithClCreateFromGlTexture3dThenErrorIsReturned) {
    tempMM->useForcedGmm = false;
    clContext->releaseSharingFunctions(CLGL_SHARING);
    auto retVal = CL_SUCCESS;
    auto glImage = clCreateFromGLTexture3D(clContext.get(), 0, GL_TEXTURE_3D, 0, textureId, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    ASSERT_EQ(nullptr, glImage);
}

TEST_F(GlSharingTextureTests, givenHwCommandQueueAndGlTextureWhenAcquireIsCalledThenAcquireCountIsIncremented) {
    glSharing->uploadDataToTextureInfo(textureId);

    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(clContext.get(), clContext->getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glImage = clCreateFromGLTexture(clContext.get(), 0, GL_TEXTURE_1D, 0, textureId, &retVal);

    EXPECT_EQ(1, GLAcquireSharedTextureCalled);
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, GLAcquireSharedTextureCalled);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3, GLAcquireSharedTextureCalled);

    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTextureTests, verifyGlTextureBufferOffset) {
    glSharing->uploadDataToTextureInfo(textureId);

    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(clContext.get(), clContext->getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glImage = clCreateFromGLTexture(clContext.get(), 0, GL_TEXTURE_1D, 0, textureId, &retVal);
    EXPECT_NE(glImage, nullptr);

    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto memObj = castToObject<MemObj>(glImage);
    EXPECT_NE(memObj, nullptr);
    EXPECT_EQ(memObj->getGraphicsAllocation()->allocationOffset, 0u);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    glSharing->uploadTextureBufferOffsetToTextureInfo(0x660);
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    memObj = castToObject<MemObj>(glImage);
    EXPECT_NE(memObj, nullptr);
    EXPECT_EQ(memObj->getGraphicsAllocation()->allocationOffset, 0x660u);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTextureTests, givenHwCommandQueueAndGlRenderBufferWhenAcquireIsCalledThenAcquireCountIsIncremented) {
    glSharing->uploadDataToTextureInfo(textureId);

    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(clContext.get(), clContext->getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glImage = clCreateFromGLRenderbuffer(clContext.get(), 0, textureId, &retVal);

    EXPECT_EQ(1, GLAcquireSharedRenderBufferCalled);
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, GLAcquireSharedRenderBufferCalled);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3, GLAcquireSharedRenderBufferCalled);

    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTextureTests, givenSharedGlTextureWhenItIsAcquireCountIsDecrementedToZeroThenCallReleaseFunction) {
    std::unique_ptr<Image> image(GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, textureId, nullptr));
    auto sharingHandler = image->peekSharingHandler();

    sharingHandler->acquire(image.get());
    sharingHandler->acquire(image.get());

    sharingHandler->release(image.get());
    EXPECT_EQ(0, GLReleaseSharedTextureCalled);

    sharingHandler->release(image.get());
    EXPECT_EQ(1, GLReleaseSharedTextureCalled);
    EXPECT_EQ(0, GLReleaseSharedRenderBufferCalled);
    EXPECT_EQ(textureId, textureInfoInput.name);
}

TEST_F(GlSharingTextureTests, givenSharedRenderBufferWhenItIsAcquireCountIsDecrementedToZeroThenCallReleaseFunction) {
    std::unique_ptr<Image> image(GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_READ_WRITE, GL_RENDERBUFFER_EXT, 0, textureId, nullptr));
    auto sharingHandler = image->peekSharingHandler();

    sharingHandler->acquire(image.get());
    sharingHandler->acquire(image.get());

    sharingHandler->release(image.get());
    EXPECT_EQ(0, GLReleaseSharedRenderBufferCalled);

    sharingHandler->release(image.get());
    EXPECT_EQ(1, GLReleaseSharedRenderBufferCalled);
    EXPECT_EQ(0, GLReleaseSharedTextureCalled);
    EXPECT_EQ(textureId, textureInfoInput.name);
}

TEST_F(GlSharingTextureTests, givenMultisampleTextureWithMoreThanOneSampleWhenAskedForNumSamplesThenReturnCorrectValue) {
    GLsizei expectedNumSamples = 2;
    glSharing->m_textureInfoOutput.numberOfSamples = expectedNumSamples;
    glSharing->uploadDataToTextureInfo();
    std::unique_ptr<Image> image(GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_READ_WRITE, GL_TEXTURE_2D_MULTISAMPLE, 0, textureId, nullptr));

    GLsizei numSamples = 0;
    size_t retSize = 0;

    auto retVal = clGetGLTextureInfo(image.get(), CL_GL_NUM_SAMPLES, sizeof(GLsizei), &numSamples, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedNumSamples, numSamples);
    EXPECT_EQ(sizeof(GLsizei), retSize);
}

TEST_F(GlSharingTextureTests, givenTextureWithOneSampleWhenAskedForNumSamplesThenReturnZero) {
    glSharing->m_textureInfoOutput.numberOfSamples = 1;
    glSharing->uploadDataToTextureInfo();
    std::unique_ptr<Image> image(GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_READ_WRITE, GL_TEXTURE_2D_MULTISAMPLE, 0, textureId, nullptr));

    GLenum numSamples = 0;
    size_t retSize = 0;

    auto retVal = clGetGLTextureInfo(image.get(), CL_GL_NUM_SAMPLES, sizeof(GLsizei), &numSamples, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, numSamples);
    EXPECT_EQ(sizeof(GLsizei), retSize);
}

TEST_F(GlSharingTextureTests, givenTextureWithZeroSamplesWhenAskedForNumSamplesThenReturnZero) {
    glSharing->m_textureInfoOutput.numberOfSamples = 0;
    glSharing->uploadDataToTextureInfo();
    std::unique_ptr<Image> image(GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_READ_WRITE, GL_TEXTURE_2D_MULTISAMPLE, 0, textureId, nullptr));

    GLenum numSamples = 0;
    size_t retSize = 0;

    auto retVal = clGetGLTextureInfo(image.get(), CL_GL_NUM_SAMPLES, sizeof(GLsizei), &numSamples, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, numSamples);
    EXPECT_EQ(sizeof(GLsizei), retSize);
}

TEST_F(GlSharingTextureTests, givenMockGlWhenGlTextureIsCreatedFromFormatNotIncludedInSurfaceFormatsThenErrorAndNoTextureIsReturned) {
    cl_int retVal = CL_SUCCESS;
    textureInfoOutput.glInternalFormat = GL_SRGB8_ALPHA8;
    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_WRITE_ONLY, GL_SRGB8_ALPHA8, 0, textureId, &retVal);

    EXPECT_EQ(nullptr, glTexture);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);
}

class GetGlTextureInfoTests : public GlSharingTextureTests,
                              public ::testing::WithParamInterface<unsigned int /*cl_GLenum*/> {
};

INSTANTIATE_TEST_CASE_P(
    GetGlTextureInfoTests,
    GetGlTextureInfoTests,
    testing::ValuesIn(glTextureTargets::supportedTargets));

TEST_P(GetGlTextureInfoTests, givenGlTextureWhenAskedForCLGetGLTextureInfoThenReturnValidInfo) {
    auto retVal = CL_SUCCESS;
    GLenum expectedTarget = GetParam();
    GLint mipLevel = 1u;
    auto glImage = clCreateFromGLTexture(clContext.get(), 0, expectedTarget, mipLevel, textureId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glImage);

    auto pMemObj = castToObject<MemObj>(glImage);
    auto glTextureObj = (GlTexture *)pMemObj->peekSharingHandler();

    GLenum textureTarget = 0u;

    size_t retSize = 0;

    retVal = clGetGLTextureInfo(glImage, CL_GL_TEXTURE_TARGET, sizeof(GLenum), &textureTarget, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedTarget, textureTarget);
    EXPECT_EQ(sizeof(GLenum), retSize);

    retVal = clGetGLTextureInfo(glImage, CL_GL_MIPMAP_LEVEL, sizeof(GLenum), &mipLevel, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(glTextureObj->getMiplevel(), mipLevel);
    EXPECT_EQ(sizeof(GLint), retSize);

    retVal = clGetGLTextureInfo(glImage, CL_INVALID_VALUE, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    auto image = castToObject<Image>(glImage);
    EXPECT_EQ(mipLevel, image->peekBaseMipLevel());

    retVal = clReleaseMemObject(glImage);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(GetGlTextureInfoTests, givenApiTargetTypeWhenAskedForBaseTypeThenConvertOnlyCubeMaps) {
    tempMM->useForcedGmm = false;
    auto apiTarget = GetParam();
    unsigned int expectedBaseType;

    switch (apiTarget) {
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        expectedBaseType = GL_TEXTURE_CUBE_MAP_ARB;
        break;
    default:
        expectedBaseType = apiTarget;
        break;
    }

    EXPECT_EQ(GlTexture::getBaseTargetType(apiTarget), expectedBaseType);
}

TEST_P(GetGlTextureInfoTests, givenApiTargetTypeWhenAskedForGmmCubeFaceIndexThenReturnValidOnlyForCubeType) {
    tempMM->useForcedGmm = false;
    auto apiTarget = GetParam();
    auto gmmCubeFaceIndex = static_cast<unsigned int>(GmmHelper::getCubeFaceIndex(apiTarget));

    switch (apiTarget) {
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        EXPECT_EQ(gmmCubeFaceIndex, static_cast<unsigned int>(__GMM_CUBE_FACE_NEG_X));
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        EXPECT_EQ(gmmCubeFaceIndex, static_cast<unsigned int>(__GMM_CUBE_FACE_POS_X));
        break;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        EXPECT_EQ(gmmCubeFaceIndex, static_cast<unsigned int>(__GMM_CUBE_FACE_NEG_Y));
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        EXPECT_EQ(gmmCubeFaceIndex, static_cast<unsigned int>(__GMM_CUBE_FACE_POS_Y));
        break;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        EXPECT_EQ(gmmCubeFaceIndex, static_cast<unsigned int>(__GMM_CUBE_FACE_NEG_Z));
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        EXPECT_EQ(gmmCubeFaceIndex, static_cast<unsigned int>(__GMM_CUBE_FACE_POS_Z));
        break;
    default:
        EXPECT_EQ(gmmCubeFaceIndex, static_cast<unsigned int>(__GMM_NO_CUBE_MAP));
        break;
    }
}
