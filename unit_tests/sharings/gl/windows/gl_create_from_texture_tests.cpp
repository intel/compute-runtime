/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_types_converter.h"
#include "core/helpers/get_info.h"
#include "runtime/mem_obj/image.h"
#include "runtime/sharings/gl/gl_texture.h"
#include "test.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/gl/windows/mock_gl_sharing_windows.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_gmm.h"

#include "gtest/gtest.h"

namespace NEO {
class CreateFromGlTexture : public ::testing::Test {
  public:
    // temp solution - we need to query size from GMM:
    class TempMM : public OsAgnosticMemoryManager {
      public:
        TempMM() : OsAgnosticMemoryManager(*(new MockExecutionEnvironment(*platformDevices))) {
            mockExecutionEnvironment.reset(&executionEnvironment);
        }
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) override {
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, properties, requireSpecificBitness);
            if (handle == CreateFromGlTexture::mcsHandle) {
                alloc->setDefaultGmm(forceMcsGmm);
            } else {
                alloc->setDefaultGmm(forceGmm);
            }
            return alloc;
        }
        size_t forceAllocationSize;
        Gmm *forceGmm = nullptr;
        Gmm *forceMcsGmm = nullptr;
        std::unique_ptr<ExecutionEnvironment> mockExecutionEnvironment;
    };

    void SetUp() override {
        imgDesc = {};
        imgInfo = {};
        clContext.setSharingFunctions(glSharing->sharingFunctions.release());
        ASSERT_FALSE(overrideCommandStreamReceiverCreation);
        clContext.memoryManager = &tempMM;
    }

    void TearDown() override {
        gmm.release();
        mcsGmm.release();
    }

    void updateImgInfoAndForceGmm() {
        imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        gmm = MockGmm::queryImgParams(clContext.getDevice(0)->getExecutionEnvironment()->getGmmClientContext(), imgInfo);

        tempMM.forceAllocationSize = imgInfo.size;
        tempMM.forceGmm = gmm.get();

        if (glSharing->m_textureInfoOutput.globalShareHandleMCS != 0) {
            cl_image_desc mcsImgDesc = {};
            mcsImgDesc.image_height = 128;
            mcsImgDesc.image_row_pitch = 256;
            mcsImgDesc.image_width = 128;
            mcsImgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
            auto mcsImgInfo = MockGmm::initImgInfo(mcsImgDesc, 0, nullptr);
            mcsGmm = MockGmm::queryImgParams(clContext.getDevice(0)->getExecutionEnvironment()->getGmmClientContext(), mcsImgInfo);
            tempMM.forceMcsGmm = mcsGmm.get();
        }
    }

    cl_image_desc imgDesc;
    ImageInfo imgInfo = {};
    std::unique_ptr<Gmm> gmm;
    std::unique_ptr<Gmm> mcsGmm;
    TempMM tempMM;
    MockContext clContext;
    std::unique_ptr<MockGlSharing> glSharing = std::make_unique<MockGlSharing>();
    cl_int retVal;
    static const unsigned int mcsHandle = 0xFF;
};

class CreateFromGlTextureTestsWithParams : public CreateFromGlTexture,
                                           public ::testing::WithParamInterface<unsigned int /*cl_GLenum*/> {
};

class CreateFromGlTextureTests : public CreateFromGlTexture {
};

INSTANTIATE_TEST_CASE_P(
    CreateFromGlTextureTestsWithParams,
    CreateFromGlTextureTestsWithParams,
    testing::ValuesIn(glTextureTargets::supportedTargets));

TEST_P(CreateFromGlTextureTestsWithParams, givenAllTextureSpecificParamsWhenCreateIsCalledThenFillImageDescription) {
    unsigned int target = GetParam();
    unsigned int baseTarget = GlTexture::getBaseTargetType(target);
    imgDesc.image_type = GlTexture::getClMemObjectType(target);
    imgDesc.image_width = 5;
    if (target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        imgDesc.image_array_size = 5;
    }
    if (target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE ||
        target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_3D ||
        target == GL_RENDERBUFFER_EXT || baseTarget == GL_TEXTURE_CUBE_MAP_ARB ||
        target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        imgDesc.image_height = 5;
    }

    if (target == GL_TEXTURE_3D) {
        imgDesc.image_depth = 5;
    }

    if (target == GL_TEXTURE_BUFFER) {
        // size and width for texture buffer are queried from textureInfo - not from gmm
        glSharing->m_textureInfoOutput.textureBufferWidth = 64;
        glSharing->m_textureInfoOutput.textureBufferSize = 1024;
        glSharing->uploadDataToTextureInfo();
    }

    if (target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        imgDesc.num_samples = 16;
        glSharing->m_textureInfoOutput.numberOfSamples = 16;
        glSharing->m_textureInfoOutput.globalShareHandleMCS = CreateFromGlTexture::mcsHandle;
        glSharing->uploadDataToTextureInfo();
    }

    updateImgInfoAndForceGmm();

    auto glImage = GlTexture::createSharedGlTexture(&clContext, (cl_mem_flags)0, target, 0, 0, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    if (target == GL_RENDERBUFFER_EXT) {
        EXPECT_EQ(1, glSharing->dllParam->getParam("GLAcquireSharedRenderBufferCalled"));
    } else {
        EXPECT_EQ(1, glSharing->dllParam->getParam("GLAcquireSharedTextureCalled"));
    }

    EXPECT_EQ(GmmTypesConverter::getCubeFaceIndex(target), glImage->getCubeFaceIndex());

    auto glTexture = reinterpret_cast<GlTexture *>(glImage->peekSharingHandler());
    EXPECT_EQ(glTexture->getTarget(), target);

    EXPECT_EQ(glImage->getImageDesc().image_type, imgDesc.image_type);
    if (target == GL_TEXTURE_BUFFER) {
        EXPECT_EQ(glImage->getImageDesc().image_width,
                  static_cast<size_t>(glTexture->getTextureInfo()->textureBufferWidth));
        EXPECT_EQ(glImage->getImageDesc().image_row_pitch,
                  static_cast<size_t>(glTexture->getTextureInfo()->textureBufferSize));
    } else {
        EXPECT_EQ(glImage->getImageDesc().image_width, gmm->gmmResourceInfo->getBaseWidth());
        size_t slicePitch = glImage->getHostPtrSlicePitch();
        size_t rowPitch = glImage->getHostPtrRowPitch();
        EXPECT_EQ(glImage->getImageDesc().image_row_pitch, rowPitch);
        EXPECT_EQ(glImage->getImageDesc().image_slice_pitch, slicePitch);

        size_t gmmRowPitch = gmm->gmmResourceInfo->getRenderPitch();
        if (gmmRowPitch == 0) {
            size_t alignedWidth = alignUp(glImage->getImageDesc().image_width, gmm->gmmResourceInfo->getHAlign());
            size_t bpp = gmm->gmmResourceInfo->getBitsPerPixel() >> 3;
            EXPECT_EQ(glImage->getImageDesc().image_row_pitch, alignedWidth * bpp);
        } else {
            EXPECT_EQ(glImage->getImageDesc().image_row_pitch, gmmRowPitch);
        }

        size_t ImageInfoRowPitch = 0;
        retVal = clGetImageInfo(glImage, CL_IMAGE_ROW_PITCH, sizeof(size_t), &ImageInfoRowPitch, NULL);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_EQ(rowPitch, ImageInfoRowPitch);

        size_t ImageInfoSlicePitch = 0;
        slicePitch *= !(glImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE2D || glImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D || glImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER);

        retVal = clGetImageInfo(glImage, CL_IMAGE_SLICE_PITCH, sizeof(size_t), &ImageInfoSlicePitch, NULL);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_EQ(slicePitch, ImageInfoSlicePitch);
    }

    EXPECT_EQ(glImage->getImageDesc().image_height, gmm->gmmResourceInfo->getBaseHeight());
    EXPECT_EQ(glImage->getImageDesc().image_array_size, gmm->gmmResourceInfo->getArraySize());
    if (target == GL_TEXTURE_3D) {
        EXPECT_EQ(glImage->getImageDesc().image_depth, gmm->gmmResourceInfo->getBaseDepth());
    } else {
        EXPECT_EQ(glImage->getImageDesc().image_depth, 0u);
    }

    if (imgDesc.image_array_size > 1 || imgDesc.image_depth > 1) {
        GMM_REQ_OFFSET_INFO GMMReqInfo = {};
        GMMReqInfo.ArrayIndex = imgDesc.image_array_size > 1 ? 1 : 0;
        GMMReqInfo.Slice = imgDesc.image_depth > 1 ? 1 : 0;
        GMMReqInfo.ReqLock = 1;
        gmm->gmmResourceInfo->getOffset(GMMReqInfo);
        size_t expectedSlicePitch = GMMReqInfo.Lock.Offset;
        EXPECT_EQ(glImage->getImageDesc().image_slice_pitch, expectedSlicePitch);
    } else {
        EXPECT_EQ(glImage->getImageDesc().image_slice_pitch, imgInfo.size);
    }

    EXPECT_EQ(glImage->getQPitch(), gmm->queryQPitch(gmm->gmmResourceInfo->getResourceType()));

    // gmm returns 1 by default - OCL requires 0
    uint32_t numSamples = static_cast<uint32_t>(gmm->gmmResourceInfo->getNumSamples());
    auto expectedNumSamples = getValidParam(numSamples, 0u, 1u);
    EXPECT_EQ(expectedNumSamples, glImage->getImageDesc().num_samples);

    if (target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        EXPECT_NE(nullptr, glImage->getMcsAllocation());
        EXPECT_EQ(getValidParam(static_cast<uint32_t>(mcsGmm->gmmResourceInfo->getRenderPitch() / 128)),
                  glImage->getMcsSurfaceInfo().pitch);
        EXPECT_EQ(static_cast<uint32_t>(mcsGmm->gmmResourceInfo->getQPitch()),
                  glImage->getMcsSurfaceInfo().qPitch);
        EXPECT_EQ(GmmTypesConverter::getRenderMultisamplesCount(static_cast<uint32_t>(gmm->gmmResourceInfo->getNumSamples())),
                  glImage->getMcsSurfaceInfo().multisampleCount);
    }

    delete glImage;
}

TEST_P(CreateFromGlTextureTestsWithParams, givenArrayTextureTargetAndArraySizeEqualOneWhenCreateIsCalledThenSlicePitchAndSizeAreEqual) {
    unsigned int target = GetParam();
    // only array targets
    if (target == GL_TEXTURE_1D_ARRAY ||
        target == GL_TEXTURE_2D_ARRAY) {
        imgDesc.image_type = GlTexture::getClMemObjectType(target);
        imgDesc.image_width = 5;
        if (target == GL_TEXTURE_2D_ARRAY) {
            imgDesc.image_height = 5;
        }
        imgDesc.image_array_size = 1;

        updateImgInfoAndForceGmm();

        auto glImage = GlTexture::createSharedGlTexture(&clContext, (cl_mem_flags)0, target, 0, 0, &retVal);

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(glImage->getImageDesc().image_slice_pitch, imgInfo.size);

        delete glImage;
    }
}

TEST_P(CreateFromGlTextureTestsWithParams, givenZeroRowPitchFromGmmWhenCreatingTextureThenComputeIt) {
    unsigned int target = GL_TEXTURE_2D;

    imgDesc.image_type = GlTexture::getClMemObjectType(target);
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    imgDesc.image_array_size = 1;

    updateImgInfoAndForceGmm();

    auto mockResInfo = reinterpret_cast<::testing::NiceMock<MockGmmResourceInfo> *>(gmm->gmmResourceInfo.get());
    mockResInfo->overrideReturnedRenderPitch(0u);

    auto alignedWidth = alignUp(imgDesc.image_width, gmm->gmmResourceInfo->getHAlign());
    auto expectedRowPitch = alignedWidth * (gmm->gmmResourceInfo->getBitsPerPixel() >> 3);

    auto glImage = std::unique_ptr<Image>(GlTexture::createSharedGlTexture(&clContext, (cl_mem_flags)0, target, 0, 0, &retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(imgInfo.size, glImage->getImageDesc().image_slice_pitch);
    EXPECT_EQ(expectedRowPitch, glImage->getImageDesc().image_row_pitch);
}

TEST_F(CreateFromGlTextureTests, GivenGlTextureTargetAndMipLevelNegativeWhenCreateIsCalledThenMipMappedImageIsCreated) {
    unsigned int target = GL_TEXTURE_3D;
    cl_GLint miplevel = -1;

    imgDesc.image_type = GlTexture::getClMemObjectType(target);
    imgDesc.image_height = 13;
    imgDesc.image_width = 15;
    imgDesc.image_depth = 7;

    updateImgInfoAndForceGmm();

    auto glImage = std::unique_ptr<Image>(GlTexture::createSharedGlTexture(&clContext, 0u, target, miplevel, 0, &retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t actualHeight = 0;
    size_t actualWidth = 0;
    size_t actualDepth = 0;

    glImage->getImageInfo(CL_IMAGE_HEIGHT, sizeof(size_t), &actualHeight, nullptr);
    glImage->getImageInfo(CL_IMAGE_WIDTH, sizeof(size_t), &actualWidth, nullptr);
    glImage->getImageInfo(CL_IMAGE_DEPTH, sizeof(size_t), &actualDepth, nullptr);

    EXPECT_EQ(13u, actualHeight);
    EXPECT_EQ(15u, actualWidth);
    EXPECT_EQ(7u, actualDepth);
    EXPECT_EQ(gmm->gmmResourceInfo->getMaxLod() + 1, glImage->getImageDesc().num_mip_levels);
    EXPECT_EQ(glImage->peekBaseMipLevel(), 0);
}

TEST_F(CreateFromGlTextureTests, GivenGlTextureTargetAndMipLevelNonNegativeWhenCreateIsCalledThenImageFromChosenMipLevelIsCreated) {
    unsigned int target = GL_TEXTURE_3D;
    cl_GLint miplevel = 2;

    imgDesc.image_type = GlTexture::getClMemObjectType(target);
    imgDesc.image_height = 13;
    imgDesc.image_width = 15;
    imgDesc.image_depth = 7;

    updateImgInfoAndForceGmm();

    auto glImage = std::unique_ptr<Image>(GlTexture::createSharedGlTexture(&clContext, 0u, target, miplevel, 0, &retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t actualHeight = 0;
    size_t actualWidth = 0;
    size_t actualDepth = 0;

    glImage->getImageInfo(CL_IMAGE_HEIGHT, sizeof(size_t), &actualHeight, nullptr);
    glImage->getImageInfo(CL_IMAGE_WIDTH, sizeof(size_t), &actualWidth, nullptr);
    glImage->getImageInfo(CL_IMAGE_DEPTH, sizeof(size_t), &actualDepth, nullptr);

    EXPECT_EQ(3u, actualHeight);
    EXPECT_EQ(3u, actualWidth);
    EXPECT_EQ(1u, actualDepth);
    EXPECT_GE(1u, glImage->getImageDesc().num_mip_levels);
    EXPECT_EQ(glImage->peekBaseMipLevel(), 2);
}

TEST_F(CreateFromGlTextureTests, GivenGlTextureWhenCreateIsCalledThenAllocationTypeIsSharedImage) {
    unsigned int target = GL_TEXTURE_3D;
    cl_GLint miplevel = 2;

    imgDesc.image_type = GlTexture::getClMemObjectType(target);
    imgDesc.image_height = 13;
    imgDesc.image_width = 15;
    imgDesc.image_depth = 7;

    updateImgInfoAndForceGmm();

    auto glImage = std::unique_ptr<Image>(GlTexture::createSharedGlTexture(&clContext, 0u, target, miplevel, 0, &retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_NE(nullptr, glImage->getGraphicsAllocation());
    EXPECT_EQ(GraphicsAllocation::AllocationType::SHARED_IMAGE, glImage->getGraphicsAllocation()->getAllocationType());
}
} // namespace NEO
