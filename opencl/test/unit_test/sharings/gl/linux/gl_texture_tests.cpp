/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/gl/gl_texture.h"
#include "opencl/test/unit_test/mocks/gl/linux/mock_gl_sharing_linux.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

using namespace NEO;

class GlSharingTextureTests : public ::testing::Test {
  public:
    class TempMM : public MockMemoryManager {
      public:
        using MockMemoryManager::MockMemoryManager;
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation, mapPointer);
            if (useForcedGmm) {
                if (alloc->getDefaultGmm()) {
                    delete alloc->getDefaultGmm();
                }
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
        executionEnvironment = platform()->peekExecutionEnvironment();
        imgDesc = {};
        imgDesc.imageType = ImageType::image2D;
        imgDesc.imageWidth = 10;
        imgDesc.imageHeight = 10;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

        tempMM = new TempMM(*executionEnvironment);
        executionEnvironment->memoryManager.reset(tempMM);
        device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));
        clContext = std::make_unique<MockContext>(device.get());

        mockGlSharingFunctions = glSharing->sharingFunctions.release();
        clContext->setSharingFunctions(mockGlSharingFunctions);

        tempMM->forceGmm = MockGmm::queryImgParams(device->getGmmHelper(), imgInfo, false);
        tempMM->forceAllocationSize = textureSize;
        textureSize = imgInfo.size;
        textureId = 1;
    }

    void setUnifiedAuxSurf() {
        tempMM->useForcedGmm = true;
        auto mockGmmResInfo = static_cast<MockGmmResourceInfo *>(tempMM->forceGmm->gmmResourceInfo.get());
        mockGmmResInfo->setUnifiedAuxTranslationCapable();
    }

    ExecutionEnvironment *executionEnvironment;
    ImageDescriptor imgDesc;
    TempMM *tempMM;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> clContext;
    std::unique_ptr<MockGlSharing> glSharing = std::make_unique<MockGlSharing>();
    GlSharingFunctionsMock *mockGlSharingFunctions;
    size_t textureSize;
    unsigned int textureId;
};

TEST_F(GlSharingTextureTests, givenMockGlWhen2dGlTextureIsCreatedThenMemObjectHasGlHandler) {
    cl_int retVal = CL_INVALID_VALUE;

    glSharing->uploadDataToTextureInfo(textureId);

    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_2D, 0, textureId, &retVal);
    ASSERT_NE(nullptr, glTexture);
    auto graphicsAllocation = glTexture->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(textureSize, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(1, glSharing->dllParam->getParam("GLAcquireSharedTextureCalled"));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(textureId, glSharing->dllParam->getTextureInfo().name); // input

    auto handler = glTexture->peekSharingHandler();
    ASSERT_NE(nullptr, handler);
    auto glHandler = static_cast<GlSharing *>(handler);

    EXPECT_EQ(glHandler->peekFunctionsHandler(), mockGlSharingFunctions);

    delete glTexture;
}

class FailingMemoryManager : public MockMemoryManager {
  public:
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        return nullptr;
    }
};

TEST_F(GlSharingTextureTests, givenMockGlWhenGlTextureIsCreatedFromWrongHandleThenErrorAndNoTextureIsReturned) {
    auto tempMemoryManager = clContext->getMemoryManager();
    tempMM->useForcedGmm = false;
    auto memoryManager = std::unique_ptr<FailingMemoryManager>(new FailingMemoryManager());
    clContext->memoryManager = memoryManager.get();

    auto retVal = CL_SUCCESS;
    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_2D, 0, textureId, &retVal);

    EXPECT_EQ(nullptr, glTexture);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);

    clContext->memoryManager = tempMemoryManager;
}

EGLBoolean mockGLAcquireSharedTexture(EGLDisplay dpy, EGLImageKHR image, int *fds, EGLint *strides, EGLint *offsets) {
    CL_GL_RESOURCE_INFO textureInfo;
    GlDllHelper dllParam;
    textureInfo.globalShareHandle = dllParam.getTextureInfo().globalShareHandle;
    textureInfo.globalShareHandleMCS = dllParam.getTextureInfo().globalShareHandleMCS;
    if (textureInfo.target == GL_TEXTURE_BUFFER) {
        // size and width for texture buffer are queried from textureInfo - not from gmm
        textureInfo.textureBufferSize = dllParam.getTextureInfo().textureBufferSize;
        textureInfo.textureBufferWidth = dllParam.getTextureInfo().textureBufferWidth;
    }
    textureInfo.pGmmResInfo = dllParam.getTextureInfo().pGmmResInfo;
    textureInfo.glInternalFormat = 99999;
    textureInfo.glHWFormat = dllParam.getTextureInfo().glHWFormat;
    textureInfo.textureBufferOffset = dllParam.getTextureInfo().textureBufferOffset;
    dllParam.loadTexture(textureInfo);

    *fds = 0;
    return GL_FALSE;
};

TEST_F(GlSharingTextureTests, givenMockGlWhenGlTextureIsCreatedFromIncorrectFormatThenErrorAndNoTextureIsReturned) {
    mockGlSharingFunctions->setGLAcquireSharedTextureMock(mockGLAcquireSharedTexture);

    tempMM->useForcedGmm = false;
    auto retVal = CL_SUCCESS;
    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_2D, 0, textureId, &retVal);

    EXPECT_EQ(nullptr, glTexture);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);
}

TEST_F(GlSharingTextureTests, givenGmmResourceAsInputWhenTextureIsCreatedThenItHasGmmSet) {
    cl_int retVal = CL_INVALID_VALUE;

    glSharing->textureInfoOutput.globalShareHandle = textureId;
    glSharing->textureInfoOutput.pGmmResInfo = this->tempMM->forceGmm->gmmResourceInfo->peekGmmResourceInfo();
    this->tempMM->useForcedGmm = false;
    glSharing->textureInfoOutput.pGmmResInfo = this->tempMM->forceGmm->gmmResourceInfo->peekGmmResourceInfo();

    glSharing->uploadDataToTextureInfo();

    auto glTexture = GlTexture::createSharedGlTexture(clContext.get(), (cl_mem_flags)0, GL_TEXTURE_2D, 0, textureId, &retVal);
    ASSERT_NE(nullptr, glTexture);
    auto graphicsAllocation = glTexture->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, graphicsAllocation);

    ASSERT_NE(nullptr, graphicsAllocation->getDefaultGmm());
    ASSERT_NE(nullptr, graphicsAllocation->getDefaultGmm()->gmmResourceInfo->peekHandle());

    delete glTexture;
}

TEST_F(GlSharingTextureTests, givenContextWithoutSharingAnd2dTextureWhenClCreateFromGlTextureIsCalledThenErrorIsReturned) {
    tempMM->useForcedGmm = false;
    clContext->resetSharingFunctions(CLGL_SHARING);
    cl_int retVal = CL_INVALID_GL_OBJECT;
    auto glImage = clCreateFromGLTexture(clContext.get(), 0, GL_TEXTURE_2D, 0, textureId, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    ASSERT_EQ(nullptr, glImage);
}

TEST_F(GlSharingTextureTests, givenGlCl2dTextureWhenAskedForCLGLGetInfoThenIdAndTypeIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glImage = clCreateFromGLTexture(clContext.get(), 0, GL_TEXTURE_2D, 0, textureId, &retVal);
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

TEST_F(GlSharingTextureTests, givenContextWithoutSharingAndGlTextureWhenItIsCreatedWithClCreateFromGlTexture1dThenErrorIsReturned) {
    tempMM->useForcedGmm = false;
    clContext->resetSharingFunctions(CLGL_SHARING);
    auto retVal = CL_SUCCESS;
    auto glImage = clCreateFromGLTexture2D(clContext.get(), 0, GL_TEXTURE_2D, 0, textureId, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    ASSERT_EQ(nullptr, glImage);
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

TEST_F(GlSharingTextureTests, givenHwCommandQueueAndGlTextureWhenAcquireIsCalledThenAcquireCountIsIncremented) {
    glSharing->uploadDataToTextureInfo(textureId);

    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(clContext.get(), clContext->getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glImage = clCreateFromGLTexture(clContext.get(), 0, GL_TEXTURE_2D, 0, textureId, &retVal);

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
