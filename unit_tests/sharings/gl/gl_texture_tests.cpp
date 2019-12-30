/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_types_converter.h"
#include "runtime/mem_obj/image.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/gl/gl_texture.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

class GlSharingTextureTests : public ::testing::Test {
  public:
    // temp solution - we need to query size from GMM:
    class TempMM : public MockMemoryManager {
      public:
        using MockMemoryManager::MockMemoryManager;
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) override {
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, properties, requireSpecificBitness);
            if (useForcedGmm) {
                alloc->setDefaultGmm(forceGmm.get());
            }
            return alloc;
        }
        void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
            if (useForcedGmm) {
                forceGmm.release();
            }
            OsAgnosticMemoryManager::freeGraphicsMemoryImpl(gfxAllocation);
        }
        bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) override {
            mapAuxGpuVACalled++;
            return false;
        }
        uint32_t mapAuxGpuVACalled = 0u;
        size_t forceAllocationSize;
        std::unique_ptr<Gmm> forceGmm;
        bool useForcedGmm = true;
    };

    void SetUp() override {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        imgDesc = {};
        imgDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
        imgDesc.image_width = 10;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

        tempMM = new TempMM(*executionEnvironment);
        executionEnvironment->memoryManager.reset(tempMM);
        device.reset(MockDevice::create<MockDevice>(executionEnvironment, 0));
        clContext = std::make_unique<MockContext>(device.get());

        mockGlSharingFunctions = glSharing->sharingFunctions.release();
        clContext->setSharingFunctions(mockGlSharingFunctions);
        ASSERT_FALSE(overrideCommandStreamReceiverCreation);

        tempMM->forceGmm = MockGmm::queryImgParams(executionEnvironment->getGmmClientContext(), imgInfo);
        tempMM->forceAllocationSize = textureSize;
        textureSize = imgInfo.size;
        textureId = 1;
    }

    void setUnifiedAuxSurf() {
        tempMM->useForcedGmm = true;
        auto mockGmmResInfo = reinterpret_cast<::testing::NiceMock<MockGmmResourceInfo> *>(tempMM->forceGmm->gmmResourceInfo.get());
        mockGmmResInfo->setUnifiedAuxTranslationCapable();
    }

    ExecutionEnvironment *executionEnvironment;
    cl_image_desc imgDesc;
    TempMM *tempMM;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> clContext;
    std::unique_ptr<MockGlSharing> glSharing = std::make_unique<MockGlSharing>();
    GlSharingFunctionsMock *mockGlSharingFunctions;
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
    EXPECT_EQ(1, glSharing->dllParam->getParam("GLAcquireSharedTextureCalled"));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(textureId, glSharing->dllParam->getTextureInfo().name); //input

    auto handler = glTexture->peekSharingHandler();
    ASSERT_NE(nullptr, handler);
    auto glHandler = static_cast<GlSharing *>(handler);

    EXPECT_EQ(glHandler->peekFunctionsHandler(), mockGlSharingFunctions);

    delete glTexture;
}

class FailingMemoryManager : public MockMemoryManager {
  public:
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) override {
        return nullptr;
    }
};

TEST_F(GlSharingTextureTests, givenMockGlWhenGlTextureIsCreatedFromWrongHandleThenErrorAndNoTextureIsReturned) {
    auto tempMemoryManager = clContext->getMemoryManager();
    tempMM->useForcedGmm = false;
    auto memoryManager = std::unique_ptr<FailingMemoryManager>(new FailingMemoryManager());
    clContext->memoryManager = memoryManager.get();

    auto retVal = CL_SUCCESS;
    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_1D, 0, textureId, &retVal);

    EXPECT_EQ(nullptr, glTexture);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);

    clContext->memoryManager = tempMemoryManager;
}

GLboolean OSAPI mockGLAcquireSharedTexture(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    auto pTextureInfo = (CL_GL_RESOURCE_INFO *)pResourceInfo;
    glDllHelper dllParam;
    pTextureInfo->globalShareHandle = dllParam.getTextureInfo().globalShareHandle;
    pTextureInfo->globalShareHandleMCS = dllParam.getTextureInfo().globalShareHandleMCS;

    if (pTextureInfo->target == GL_TEXTURE_BUFFER) {
        // size and width for texture buffer are queried from textureInfo - not from gmm
        pTextureInfo->textureBufferSize = dllParam.getTextureInfo().textureBufferSize;
        pTextureInfo->textureBufferWidth = dllParam.getTextureInfo().textureBufferWidth;
    }
    pTextureInfo->pGmmResInfo = dllParam.getTextureInfo().pGmmResInfo;
    pTextureInfo->glInternalFormat = 99999;
    pTextureInfo->glHWFormat = dllParam.getTextureInfo().glHWFormat;
    pTextureInfo->textureBufferOffset = dllParam.getTextureInfo().textureBufferOffset;
    dllParam.loadTexture(*pTextureInfo);
    return (GLboolean)1;
};

TEST_F(GlSharingTextureTests, givenMockGlWhenGlTextureIsCreatedFromIncorrectFormatThenErrorAndNoTextureIsReturned) {
    mockGlSharingFunctions->setGLAcquireSharedTextureMock(mockGLAcquireSharedTexture);

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
    EXPECT_EQ(1, glSharing->dllParam->getParam("GLAcquireSharedRenderBufferCalled"));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(textureId, glSharing->dllParam->getTextureInfo().name);

    auto handler = glTexture->peekSharingHandler();
    ASSERT_NE(nullptr, handler);
    auto glHandler = static_cast<GlSharing *>(handler);

    EXPECT_EQ(glHandler->peekFunctionsHandler(), mockGlSharingFunctions);

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

    ASSERT_NE(nullptr, glTexture->getGraphicsAllocation()->getDefaultGmm());
    ASSERT_NE(nullptr, glTexture->getGraphicsAllocation()->getDefaultGmm()->gmmResourceInfo->peekHandle());

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
    clContext->resetSharingFunctions(CLGL_SHARING);
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
    clContext->resetSharingFunctions(CLGL_SHARING);
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
    clContext->resetSharingFunctions(CLGL_SHARING);
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
    clContext->resetSharingFunctions(CLGL_SHARING);
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

    EXPECT_EQ(1, glSharing->dllParam->getParam("GLAcquireSharedTextureCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, glSharing->dllParam->getParam("GLAcquireSharedTextureCalled"));

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3, glSharing->dllParam->getParam("GLAcquireSharedTextureCalled"));

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
    EXPECT_EQ(memObj->getGraphicsAllocation()->getAllocationOffset(), 0u);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    glSharing->uploadTextureBufferOffsetToTextureInfo(0x660);
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    memObj = castToObject<MemObj>(glImage);
    EXPECT_NE(memObj, nullptr);
    EXPECT_EQ(memObj->getGraphicsAllocation()->getAllocationOffset(), 0x660u);

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

    EXPECT_EQ(1, glSharing->dllParam->getParam("GLAcquireSharedRenderBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, glSharing->dllParam->getParam("GLAcquireSharedRenderBufferCalled"));

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glImage, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3, glSharing->dllParam->getParam("GLAcquireSharedRenderBufferCalled"));

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
    EXPECT_EQ(0, glSharing->dllParam->getParam("GLReleaseSharedTextureCalled"));

    sharingHandler->release(image.get());
    EXPECT_EQ(1, glSharing->dllParam->getParam("GLReleaseSharedTextureCalled"));
    EXPECT_EQ(0, glSharing->dllParam->getParam("GLReleaseSharedRenderBufferCalled"));
    EXPECT_EQ(textureId, glSharing->dllParam->getTextureInfo().name);
}

TEST_F(GlSharingTextureTests, givenSharedRenderBufferWhenItIsAcquireCountIsDecrementedToZeroThenCallReleaseFunction) {
    std::unique_ptr<Image> image(GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_READ_WRITE, GL_RENDERBUFFER_EXT, 0, textureId, nullptr));
    auto sharingHandler = image->peekSharingHandler();

    sharingHandler->acquire(image.get());
    sharingHandler->acquire(image.get());

    sharingHandler->release(image.get());
    EXPECT_EQ(0, glSharing->dllParam->getParam("GLReleaseSharedRenderBufferCalled"));

    sharingHandler->release(image.get());
    EXPECT_EQ(1, glSharing->dllParam->getParam("GLReleaseSharedRenderBufferCalled"));
    EXPECT_EQ(0, glSharing->dllParam->getParam("GLReleaseSharedTextureCalled"));
    EXPECT_EQ(textureId, glSharing->dllParam->getTextureInfo().name);
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
    auto textureInfoOutput = std::make_unique<CL_GL_RESOURCE_INFO>();
    textureInfoOutput->glInternalFormat = GL_SRGB8_ALPHA8;
    glSharing->dllParam->loadTexture(*textureInfoOutput);

    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_WRITE_ONLY, GL_SRGB8_ALPHA8, 0, textureId, &retVal);

    EXPECT_EQ(nullptr, glTexture);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);
}

TEST_F(GlSharingTextureTests, givenMockGlWhenGlTextureIsCreatedWithUnifiedAuxSurfThenMapAuxGpuVaIsCalled) {
    cl_int retVal = CL_SUCCESS;
    setUnifiedAuxSurf();
    EXPECT_EQ(0u, tempMM->mapAuxGpuVACalled);

    auto glTexture = std::unique_ptr<Image>(GlTexture::createSharedGlTexture(clContext.get(), CL_MEM_WRITE_ONLY, GL_SRGB8_ALPHA8, 0, textureId, &retVal));

    auto hwInfo = executionEnvironment->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    uint32_t expectedMapAuxGpuVaCalls = hwHelper.isPageTableManagerSupported(*hwInfo) ? 1 : 0;

    EXPECT_EQ(expectedMapAuxGpuVaCalls, tempMM->mapAuxGpuVACalled);
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
    auto gmmCubeFaceIndex = static_cast<unsigned int>(GmmTypesConverter::getCubeFaceIndex(apiTarget));

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
