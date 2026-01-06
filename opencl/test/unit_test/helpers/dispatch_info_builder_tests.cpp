/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

namespace NEO {

using namespace SplitDispatch;

class DispatchInfoBuilderFixture : public ContextFixture, public ClDeviceFixture {
    using ContextFixture::setUp;

  public:
    DispatchInfoBuilderFixture() {}
    void clearCrossThreadData() {
        memset(pCrossThreadData, 0, sizeof(pCrossThreadData));
    }

  protected:
    void setUp() {
        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        pKernelInfo = std::make_unique<MockKernelInfo>();

        pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
        pKernelInfo->kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;

        pKernelInfo->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 1024;
        pKernelInfo->setPrintfSurface(sizeof(uintptr_t), 0);

        pKernelInfo->addArgBuffer(0, 0x10, sizeof(void *));
        pKernelInfo->addArgBuffer(1, 0x30, sizeof(void *));
        pKernelInfo->addArgBuffer(2, 0x50, sizeof(void *));

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
        pKernel->setKernelArgHandler(0, &Kernel::setArgBuffer);

        pKernel->slmTotalSize = 128;
        pKernel->isBuiltIn = true;
    }

    void tearDown() {
        delete pKernel;
        delete pProgram;

        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    std::unique_ptr<MockKernelInfo> pKernelInfo;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    char pCrossThreadData[128];
};

typedef Test<DispatchInfoBuilderFixture> DispatchInfoBuilderTest;

template <SplitDispatch::Dim dim, SplitDispatch::SplitMode mode>
class DispatchInfoBuilderMock : DispatchInfoBuilder<dim, mode> {
  public:
    using DispatchInfoBuilder<dim, mode>::DispatchInfoBuilder;
    void pushSplit(const DispatchInfo &dispatchInfo, MultiDispatchInfo &outMdi) {
        DispatchInfoBuilder<dim, mode>::pushSplit(dispatchInfo, outMdi);
    }
};

TEST_F(DispatchInfoBuilderTest, Given1dWhenSplittingMultiDispatchInfoThenMultiDispatchInfo) {
    MultiDispatchInfo multiDispatchInfo;

    auto diBuilder = new DispatchInfoBuilderMock<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfo dispatchInfo;
    diBuilder->pushSplit(dispatchInfo, multiDispatchInfo);
    EXPECT_TRUE(multiDispatchInfo.empty());

    delete diBuilder;
}

TEST_F(DispatchInfoBuilderTest, WhenGettingDimensionThenCorrectDimensionIsReturned) {
    MultiDispatchInfo mdi1D, mdi2D, mdi3D;

    DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::noSplit> *diBuilder1D = new DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder1D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::noSplit> *diBuilder2D = new DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder2D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> *diBuilder3D = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder3D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    diBuilder1D->setDispatchGeometry(Vec3<size_t>(1, 0, 0), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder1D->bake(mdi1D);
    for (auto &dispatchInfo : mdi1D) {
        EXPECT_EQ(1u, dispatchInfo.getDim());
    }

    diBuilder2D->setDispatchGeometry(Vec3<size_t>(1, 2, 0), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder2D->bake(mdi2D);
    for (auto &dispatchInfo : mdi2D) {
        EXPECT_EQ(2u, dispatchInfo.getDim());
    }

    diBuilder3D->setDispatchGeometry(Vec3<size_t>(1, 2, 3), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder3D->bake(mdi3D);
    for (auto &dispatchInfo : mdi3D) {
        EXPECT_EQ(3u, dispatchInfo.getDim());
    }

    delete diBuilder3D;
    delete diBuilder2D;
    delete diBuilder1D;
}

TEST_F(DispatchInfoBuilderTest, WhenGettingGwsThenCorrectValuesAreReturned) {
    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    MultiDispatchInfo mdi0, mdi1, mdi2, mdi3;

    diBuilder->setDispatchGeometry(Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi0);
    EXPECT_TRUE(mdi0.empty());

    diBuilder->setDispatchGeometry(Vec3<size_t>(1, 0, 0), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi1);
    for (auto &dispatchInfo : mdi1) {
        EXPECT_EQ(1u, dispatchInfo.getGWS().x);
        EXPECT_EQ(1u, dispatchInfo.getGWS().y);
        EXPECT_EQ(1u, dispatchInfo.getGWS().z);
        EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
        EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
    }

    diBuilder->setDispatchGeometry(Vec3<size_t>(1, 2, 0), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi2);
    for (auto &dispatchInfo : mdi2) {
        EXPECT_EQ(1u, dispatchInfo.getGWS().x);
        EXPECT_EQ(2u, dispatchInfo.getGWS().y);
        EXPECT_EQ(1u, dispatchInfo.getGWS().z);
        EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
        EXPECT_EQ(2u, dispatchInfo.getActualWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
    }

    diBuilder->setDispatchGeometry(Vec3<size_t>(1, 2, 3), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi3);
    for (auto &dispatchInfo : mdi3) {
        EXPECT_EQ(1u, dispatchInfo.getGWS().x);
        EXPECT_EQ(2u, dispatchInfo.getGWS().y);
        EXPECT_EQ(3u, dispatchInfo.getGWS().z);
        EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
        EXPECT_EQ(2u, dispatchInfo.getActualWorkgroupSize().y);
        EXPECT_EQ(3u, dispatchInfo.getActualWorkgroupSize().z);
    }

    delete diBuilder;
}

TEST_F(DispatchInfoBuilderTest, WhenGettingElwsThenCorrectValuesAreReturned) {
    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    MultiDispatchInfo mdi0, mdi1, mdi2, mdi3;

    diBuilder->setDispatchGeometry(Vec3<size_t>(0, 0, 0), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi0);
    EXPECT_TRUE(mdi0.empty());

    diBuilder->setDispatchGeometry(Vec3<size_t>(1, 0, 0), Vec3<size_t>(1, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi1);
    for (auto &dispatchInfo : mdi1) {
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().x);
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().z);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
    }

    diBuilder->setDispatchGeometry(Vec3<size_t>(1, 1, 0), Vec3<size_t>(1, 2, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi2);
    for (auto &dispatchInfo : mdi2) {
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().x);
        EXPECT_EQ(2u, dispatchInfo.getEnqueuedWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().z);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
        EXPECT_EQ(2u, dispatchInfo.getLocalWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
    }

    diBuilder->setDispatchGeometry(Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 2, 3), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi3);
    for (auto &dispatchInfo : mdi3) {
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().x);
        EXPECT_EQ(2u, dispatchInfo.getEnqueuedWorkgroupSize().y);
        EXPECT_EQ(3u, dispatchInfo.getEnqueuedWorkgroupSize().z);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
        EXPECT_EQ(2u, dispatchInfo.getLocalWorkgroupSize().y);
        EXPECT_EQ(3u, dispatchInfo.getLocalWorkgroupSize().z);
    }

    delete diBuilder;
}

TEST_F(DispatchInfoBuilderTest, WhenGettingLwsThenCorrectValuesAreReturned) {
    auto diBuilder = std::make_unique<DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder);

    MultiDispatchInfo mdi0, mdi1, mdi2, mdi3;

    diBuilder->setKernel(pKernel);
    diBuilder->setDispatchGeometry(Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi0);
    EXPECT_TRUE(mdi0.empty());

    diBuilder->setKernel(pKernel);
    diBuilder->setDispatchGeometry(Vec3<size_t>(4, 0, 0), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi1);
    for (auto &dispatchInfo : mdi1) {
        EXPECT_EQ(4u, dispatchInfo.getLocalWorkgroupSize().x);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
    }

    diBuilder->setKernel(pKernel);
    diBuilder->setDispatchGeometry(Vec3<size_t>(4, 4, 0), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi2);
    for (auto &dispatchInfo : mdi2) {
        EXPECT_EQ(4u, dispatchInfo.getLocalWorkgroupSize().x);
        EXPECT_EQ(4u, dispatchInfo.getLocalWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
    }

    diBuilder->setKernel(pKernel);
    diBuilder->setDispatchGeometry(Vec3<size_t>(4, 4, 4), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi3);
    for (auto &dispatchInfo : mdi3) {
        EXPECT_EQ(4u, dispatchInfo.getLocalWorkgroupSize().y);
        EXPECT_EQ(4u, dispatchInfo.getLocalWorkgroupSize().z);
        EXPECT_EQ(4u, dispatchInfo.getLocalWorkgroupSize().x);
    }
}

TEST_F(DispatchInfoBuilderTest, GivenNoSplitWhenCheckingIfBuiltinThenReturnTrue) {
    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    diBuilder->setKernel(pKernel);
    diBuilder->setDispatchGeometry(Vec3<size_t>(256, 256, 256), Vec3<size_t>(16, 16, 16), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo multiDispatchInfo;
    diBuilder->bake(multiDispatchInfo);

    for (auto &dispatchInfo : multiDispatchInfo) {
        ASSERT_EQ(pKernel, dispatchInfo.getKernel());
        EXPECT_TRUE(dispatchInfo.getKernel()->isBuiltInKernel());
    }

    delete diBuilder;
}

TEST_F(DispatchInfoBuilderTest, GivenSplitWhenCheckingIfBuiltinThenReturnTrue) {
    DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::kernelSplit> *diBuilder1D = new DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::kernelSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder1D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::kernelSplit> *diBuilder2D = new DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::kernelSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder2D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::kernelSplit> *diBuilder3D = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::kernelSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder3D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    // 1D
    diBuilder1D->setKernel(RegionCoordX::left, pKernel);
    diBuilder1D->setDispatchGeometry(RegionCoordX::left, Vec3<size_t>(256, 0, 0), Vec3<size_t>(16, 0, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi1D;
    diBuilder1D->bake(mdi1D);

    for (auto &dispatchInfo : mdi1D) {
        EXPECT_EQ(pKernel, dispatchInfo.getKernel());
        EXPECT_TRUE(dispatchInfo.getKernel()->isBuiltInKernel());
    }

    // 2D
    diBuilder2D->setKernel(RegionCoordX::left, RegionCoordY::bottom, pKernel);
    diBuilder2D->setDispatchGeometry(RegionCoordX::left, RegionCoordY::bottom, Vec3<size_t>(256, 256, 0), Vec3<size_t>(16, 16, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi2D;
    diBuilder2D->bake(mdi2D);
    for (auto &dispatchInfo : mdi2D) {
        EXPECT_EQ(pKernel, dispatchInfo.getKernel());
        EXPECT_TRUE(dispatchInfo.getKernel()->isBuiltInKernel());
    }

    // 3D
    diBuilder3D->setKernel(RegionCoordX::right, RegionCoordY::bottom, RegionCoordZ::back, pKernel);
    diBuilder3D->setDispatchGeometry(RegionCoordX::right, RegionCoordY::bottom, RegionCoordZ::back, Vec3<size_t>(256, 256, 256), Vec3<size_t>(16, 16, 16), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi3D;
    diBuilder3D->bake(mdi3D);

    for (auto &dispatchInfo : mdi3D) {
        EXPECT_EQ(pKernel, dispatchInfo.getKernel());
        EXPECT_TRUE(dispatchInfo.getKernel()->isBuiltInKernel());
    }

    delete diBuilder3D;
    delete diBuilder2D;
    delete diBuilder1D;
}

TEST_F(DispatchInfoBuilderTest, GivenNoSplitWhenGettingWalkerInfoThenCorrectValuesAreReturned) {
    DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::walkerSplit> *diBuilder1D = new DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder1D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::walkerSplit> *diBuilder2D = new DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder2D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::walkerSplit> *diBuilder3D = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder3D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    // 1D
    diBuilder1D->setKernel(pKernel);
    diBuilder1D->setDispatchGeometry(Vec3<size_t>(256, 0, 0), Vec3<size_t>(16, 0, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi1D;
    diBuilder1D->bake(mdi1D);
    EXPECT_EQ(1u, mdi1D.size());

    const DispatchInfo *di1D = mdi1D.begin();
    EXPECT_EQ(pKernel, di1D->getKernel());
    EXPECT_EQ(256u, di1D->getGWS().x);
    EXPECT_EQ(1u, di1D->getGWS().y);
    EXPECT_EQ(1u, di1D->getGWS().z);
    EXPECT_EQ(16u, di1D->getEnqueuedWorkgroupSize().x);
    EXPECT_EQ(1u, di1D->getEnqueuedWorkgroupSize().y);
    EXPECT_EQ(1u, di1D->getEnqueuedWorkgroupSize().z);
    EXPECT_EQ(0u, di1D->getOffset().x);
    EXPECT_EQ(0u, di1D->getOffset().y);
    EXPECT_EQ(0u, di1D->getOffset().z);
    EXPECT_EQ(16u, di1D->getLocalWorkgroupSize().x);
    EXPECT_EQ(1u, di1D->getLocalWorkgroupSize().y);
    EXPECT_EQ(1u, di1D->getLocalWorkgroupSize().z);
    EXPECT_EQ(256u, di1D->getActualWorkgroupSize().x);
    EXPECT_EQ(1u, di1D->getActualWorkgroupSize().y);
    EXPECT_EQ(1u, di1D->getActualWorkgroupSize().z);
    EXPECT_EQ(16u, di1D->getTotalNumberOfWorkgroups().x);
    EXPECT_EQ(1u, di1D->getTotalNumberOfWorkgroups().y);
    EXPECT_EQ(1u, di1D->getTotalNumberOfWorkgroups().z);
    EXPECT_EQ(16u, di1D->getNumberOfWorkgroups().x);
    EXPECT_EQ(1u, di1D->getNumberOfWorkgroups().y);
    EXPECT_EQ(1u, di1D->getNumberOfWorkgroups().z);
    EXPECT_EQ(0u, di1D->getStartOfWorkgroups().x);
    EXPECT_EQ(0u, di1D->getStartOfWorkgroups().y);
    EXPECT_EQ(0u, di1D->getStartOfWorkgroups().z);

    // 2D
    diBuilder2D->setKernel(pKernel);
    diBuilder2D->setDispatchGeometry(Vec3<size_t>(256, 256, 0), Vec3<size_t>(16, 16, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi2D;
    diBuilder2D->bake(mdi2D);
    EXPECT_EQ(1u, mdi2D.size());

    const DispatchInfo *di2D = mdi2D.begin();
    EXPECT_EQ(pKernel, di2D->getKernel());
    EXPECT_EQ(256u, di2D->getGWS().x);
    EXPECT_EQ(256u, di2D->getGWS().y);
    EXPECT_EQ(1u, di2D->getGWS().z);
    EXPECT_EQ(16u, di2D->getEnqueuedWorkgroupSize().x);
    EXPECT_EQ(16u, di2D->getEnqueuedWorkgroupSize().y);
    EXPECT_EQ(1u, di2D->getEnqueuedWorkgroupSize().z);
    EXPECT_EQ(0u, di2D->getOffset().x);
    EXPECT_EQ(0u, di2D->getOffset().y);
    EXPECT_EQ(0u, di2D->getOffset().z);
    EXPECT_EQ(16u, di2D->getLocalWorkgroupSize().x);
    EXPECT_EQ(16u, di2D->getLocalWorkgroupSize().y);
    EXPECT_EQ(1u, di2D->getLocalWorkgroupSize().z);
    EXPECT_EQ(16u, di2D->getTotalNumberOfWorkgroups().x);
    EXPECT_EQ(16u, di2D->getTotalNumberOfWorkgroups().y);
    EXPECT_EQ(1u, di2D->getTotalNumberOfWorkgroups().z);
    EXPECT_EQ(16u, di2D->getNumberOfWorkgroups().x);
    EXPECT_EQ(16u, di2D->getNumberOfWorkgroups().y);
    EXPECT_EQ(1u, di2D->getNumberOfWorkgroups().z);
    EXPECT_EQ(0u, di2D->getStartOfWorkgroups().x);
    EXPECT_EQ(0u, di2D->getStartOfWorkgroups().y);
    EXPECT_EQ(0u, di2D->getStartOfWorkgroups().z);

    // 3D
    diBuilder3D->setKernel(pKernel);
    diBuilder3D->setDispatchGeometry(Vec3<size_t>(256, 256, 256), Vec3<size_t>(16, 16, 16), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi3D;
    diBuilder3D->bake(mdi3D);
    EXPECT_EQ(1u, mdi3D.size());

    const DispatchInfo *di3D = mdi3D.begin();
    EXPECT_EQ(pKernel, di3D->getKernel());
    EXPECT_EQ(256u, di3D->getGWS().x);
    EXPECT_EQ(256u, di3D->getGWS().y);
    EXPECT_EQ(256u, di3D->getGWS().z);
    EXPECT_EQ(16u, di3D->getEnqueuedWorkgroupSize().x);
    EXPECT_EQ(16u, di3D->getEnqueuedWorkgroupSize().y);
    EXPECT_EQ(16u, di3D->getEnqueuedWorkgroupSize().z);
    EXPECT_EQ(0u, di3D->getOffset().x);
    EXPECT_EQ(0u, di3D->getOffset().y);
    EXPECT_EQ(0u, di3D->getOffset().z);
    EXPECT_EQ(16u, di3D->getLocalWorkgroupSize().x);
    EXPECT_EQ(16u, di3D->getLocalWorkgroupSize().y);
    EXPECT_EQ(16u, di3D->getLocalWorkgroupSize().z);
    EXPECT_EQ(16u, di3D->getTotalNumberOfWorkgroups().x);
    EXPECT_EQ(16u, di3D->getTotalNumberOfWorkgroups().y);
    EXPECT_EQ(16u, di3D->getTotalNumberOfWorkgroups().z);
    EXPECT_EQ(16u, di3D->getNumberOfWorkgroups().x);
    EXPECT_EQ(16u, di3D->getNumberOfWorkgroups().y);
    EXPECT_EQ(16u, di3D->getNumberOfWorkgroups().z);
    EXPECT_EQ(0u, di3D->getStartOfWorkgroups().x);
    EXPECT_EQ(0u, di3D->getStartOfWorkgroups().y);
    EXPECT_EQ(0u, di3D->getStartOfWorkgroups().z);

    delete diBuilder3D;
    delete diBuilder2D;
    delete diBuilder1D;
}

TEST_F(DispatchInfoBuilderTest, GivenSplitWhenGettingWalkerInfoThenCorrectValuesAreReturned) {
    DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::walkerSplit> *diBuilder1D = new DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder1D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::walkerSplit> *diBuilder2D = new DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder2D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::walkerSplit> *diBuilder3D = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder3D); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    // 1D
    diBuilder1D->setKernel(pKernel);
    diBuilder1D->setDispatchGeometry(Vec3<size_t>(256, 0, 0), Vec3<size_t>(15, 0, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi1D;
    diBuilder1D->bake(mdi1D);
    EXPECT_EQ(2u, mdi1D.size());

    auto dispatchId = 0;
    for (auto &dispatchInfo : mdi1D) {
        EXPECT_EQ(pKernel, dispatchInfo.getKernel());
        EXPECT_EQ(256u, dispatchInfo.getGWS().x);
        EXPECT_EQ(1u, dispatchInfo.getGWS().y);
        EXPECT_EQ(1u, dispatchInfo.getGWS().z);
        EXPECT_EQ(15u, dispatchInfo.getEnqueuedWorkgroupSize().x);
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().z);
        EXPECT_EQ(0u, dispatchInfo.getOffset().x);
        EXPECT_EQ(0u, dispatchInfo.getOffset().y);
        EXPECT_EQ(0u, dispatchInfo.getOffset().z);
        EXPECT_EQ(18u, dispatchInfo.getTotalNumberOfWorkgroups().x);
        EXPECT_EQ(1u, dispatchInfo.getTotalNumberOfWorkgroups().y);
        EXPECT_EQ(1u, dispatchInfo.getTotalNumberOfWorkgroups().z);
        switch (dispatchId) {
        case 0:
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(1u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(1u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 1:
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(1u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(1u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        }
        dispatchId++;
    }

    // 2D
    diBuilder2D->setKernel(pKernel);
    diBuilder2D->setDispatchGeometry(Vec3<size_t>(256, 256, 0), Vec3<size_t>(15, 15, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi2D;
    diBuilder2D->bake(mdi2D);
    EXPECT_EQ(4u, mdi2D.size());

    dispatchId = 0;
    for (auto &dispatchInfo : mdi2D) {
        EXPECT_EQ(pKernel, dispatchInfo.getKernel());
        EXPECT_EQ(256u, dispatchInfo.getGWS().x);
        EXPECT_EQ(256u, dispatchInfo.getGWS().y);
        EXPECT_EQ(1u, dispatchInfo.getGWS().z);
        EXPECT_EQ(15u, dispatchInfo.getEnqueuedWorkgroupSize().x);
        EXPECT_EQ(15u, dispatchInfo.getEnqueuedWorkgroupSize().y);
        EXPECT_EQ(1u, dispatchInfo.getEnqueuedWorkgroupSize().z);
        EXPECT_EQ(0u, dispatchInfo.getOffset().x);
        EXPECT_EQ(0u, dispatchInfo.getOffset().y);
        EXPECT_EQ(0u, dispatchInfo.getOffset().z);
        EXPECT_EQ(18u, dispatchInfo.getTotalNumberOfWorkgroups().x);
        EXPECT_EQ(18u, dispatchInfo.getTotalNumberOfWorkgroups().y);
        EXPECT_EQ(1u, dispatchInfo.getTotalNumberOfWorkgroups().z);
        switch (dispatchId) {
        case 0:
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(1u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 1:
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(1u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 2:
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(1u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 3:
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(1u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        }
        dispatchId++;
    }

    // 3D
    diBuilder3D->setKernel(pKernel);
    diBuilder3D->setDispatchGeometry(Vec3<size_t>(256, 256, 256), Vec3<size_t>(15, 15, 15), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdi3D;
    diBuilder3D->bake(mdi3D);
    EXPECT_EQ(8u, mdi3D.size());

    dispatchId = 0;
    for (auto &dispatchInfo : mdi3D) {
        EXPECT_EQ(pKernel, dispatchInfo.getKernel());
        EXPECT_EQ(256u, dispatchInfo.getGWS().x);
        EXPECT_EQ(256u, dispatchInfo.getGWS().y);
        EXPECT_EQ(256u, dispatchInfo.getGWS().z);
        EXPECT_EQ(15u, dispatchInfo.getEnqueuedWorkgroupSize().x);
        EXPECT_EQ(15u, dispatchInfo.getEnqueuedWorkgroupSize().y);
        EXPECT_EQ(15u, dispatchInfo.getEnqueuedWorkgroupSize().z);
        EXPECT_EQ(0u, dispatchInfo.getOffset().x);
        EXPECT_EQ(0u, dispatchInfo.getOffset().y);
        EXPECT_EQ(0u, dispatchInfo.getOffset().z);
        EXPECT_EQ(18u, dispatchInfo.getTotalNumberOfWorkgroups().x);
        EXPECT_EQ(18u, dispatchInfo.getTotalNumberOfWorkgroups().y);
        EXPECT_EQ(18u, dispatchInfo.getTotalNumberOfWorkgroups().z);
        switch (dispatchId) {
        case 0:
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 1:
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 2:
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 3:
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 4:
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 5:
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 6:
            EXPECT_EQ(255u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(15u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(17u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(0u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        case 7:
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getActualWorkgroupSize().z);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().x);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().y);
            EXPECT_EQ(1u, dispatchInfo.getLocalWorkgroupSize().z);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().x);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().y);
            EXPECT_EQ(18u, dispatchInfo.getNumberOfWorkgroups().z);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().x);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().y);
            EXPECT_EQ(17u, dispatchInfo.getStartOfWorkgroups().z);
            break;
        }
        dispatchId++;
    }
    delete diBuilder3D;
    delete diBuilder2D;
    delete diBuilder1D;
}

TEST_F(DispatchInfoBuilderTest, GivenSplit1dWhenSettingDispatchGeometryThenMdiSizeIsCorrect) {
    DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::walkerSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    diBuilder->setDispatchGeometry(Vec3<size_t>(0, 0, 0), Vec3<size_t>(2, 0, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize0;
    diBuilder->bake(mdiSize0);
    EXPECT_EQ(0u, mdiSize0.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(2, 0, 0), Vec3<size_t>(2, 0, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize1;
    diBuilder->bake(mdiSize1);
    EXPECT_EQ(1u, mdiSize1.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(3, 0, 0), Vec3<size_t>(2, 0, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize2;
    diBuilder->bake(mdiSize2);
    EXPECT_EQ(2u, mdiSize2.size());

    delete diBuilder;
}

TEST_F(DispatchInfoBuilderTest, GivenSplit2dWhenSettingDispatchGeometryThenMdiSizeIsCorrect) {
    DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::walkerSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    diBuilder->setDispatchGeometry(Vec3<size_t>(0, 0, 0), Vec3<size_t>(2, 2, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize00;
    diBuilder->bake(mdiSize00);
    EXPECT_EQ(0u, mdiSize00.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(2, 2, 0), Vec3<size_t>(2, 2, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize11;
    diBuilder->bake(mdiSize11);
    EXPECT_EQ(1u, mdiSize11.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(3, 2, 0), Vec3<size_t>(2, 2, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize21;
    diBuilder->bake(mdiSize21);
    EXPECT_EQ(2u, mdiSize21.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(2, 3, 0), Vec3<size_t>(2, 2, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize12;
    diBuilder->bake(mdiSize12);
    EXPECT_EQ(2u, mdiSize12.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(3, 3, 0), Vec3<size_t>(2, 2, 0), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize22;
    diBuilder->bake(mdiSize22);
    EXPECT_EQ(4u, mdiSize22.size());

    delete diBuilder;
}

TEST_F(DispatchInfoBuilderTest, GivenSplit3dWhenSettingDispatchGeometryThenMdiSizeIsCorrect) {
    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::walkerSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::walkerSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    diBuilder->setDispatchGeometry(Vec3<size_t>(0, 0, 0), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize000;
    diBuilder->bake(mdiSize000);
    EXPECT_EQ(0u, mdiSize000.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(2, 2, 2), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize111;
    diBuilder->bake(mdiSize111);
    EXPECT_EQ(1u, mdiSize111.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(3, 2, 2), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize211;
    diBuilder->bake(mdiSize211);
    EXPECT_EQ(2u, mdiSize211.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(2, 3, 2), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize121;
    diBuilder->bake(mdiSize121);
    EXPECT_EQ(2u, mdiSize121.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(2, 2, 3), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize112;
    diBuilder->bake(mdiSize112);
    EXPECT_EQ(2u, mdiSize112.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(3, 3, 2), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize221;
    diBuilder->bake(mdiSize221);
    EXPECT_EQ(4u, mdiSize221.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(3, 2, 3), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize212;
    diBuilder->bake(mdiSize212);
    EXPECT_EQ(4u, mdiSize212.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(2, 3, 3), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize122;
    diBuilder->bake(mdiSize122);
    EXPECT_EQ(4u, mdiSize122.size());

    diBuilder->setDispatchGeometry(Vec3<size_t>(3, 3, 3), Vec3<size_t>(2, 2, 2), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo mdiSize222;
    diBuilder->bake(mdiSize222);
    EXPECT_EQ(8u, mdiSize222.size());

    delete diBuilder;
}

TEST_F(DispatchInfoBuilderTest, WhenSettingKernelArgThenAddressesAreCorrect) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    Buffer *buffer = new MockBuffer();
    auto val = (cl_mem)buffer;
    auto pVal = &val;

    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    diBuilder->setKernel(pKernel);
    diBuilder->setDispatchGeometry(Vec3<size_t>(256, 256, 256), Vec3<size_t>(16, 16, 16), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo multiDispatchInfo;

    diBuilder->bake(multiDispatchInfo);
    clearCrossThreadData();

    EXPECT_EQ(CL_SUCCESS, diBuilder->setArg(0, sizeof(cl_mem *), pVal));
    char data[128];
    void *svmPtr = &data;
    EXPECT_EQ(CL_SUCCESS, diBuilder->setArgSvm(1, sizeof(svmPtr), svmPtr, nullptr, 0u));
    MockGraphicsAllocation svmAlloc(svmPtr, 128);
    EXPECT_EQ(CL_SUCCESS, diBuilder->setArgSvmAlloc(2, svmPtr, &svmAlloc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto crossthreadOffset0 = pKernelInfo->argAsPtr(0).stateless;
        EXPECT_EQ(buffer->getCpuAddress(), *reinterpret_cast<void **>((dispatchInfo.getKernel()->getCrossThreadData() + crossthreadOffset0)));
        auto crossthreadOffset1 = pKernelInfo->argAsPtr(1).stateless;
        EXPECT_EQ(svmPtr, *(reinterpret_cast<void **>(dispatchInfo.getKernel()->getCrossThreadData() + crossthreadOffset1)));
        auto crossthreadOffset2 = pKernelInfo->argAsPtr(2).stateless;
        EXPECT_EQ(svmPtr, *(reinterpret_cast<void **>(dispatchInfo.getKernel()->getCrossThreadData() + crossthreadOffset2)));
    }

    delete buffer;
    delete diBuilder;
}

TEST_F(DispatchInfoBuilderTest, GivenSplitWhenSettingKernelArgThenAddressesAreCorrect) {
    DispatchInfoBuilder<SplitDispatch::Dim::d1D, SplitDispatch::SplitMode::kernelSplit> builder1D(*pClDevice);
    DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::kernelSplit> builder2D(*pClDevice);
    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::kernelSplit> builder3D(*pClDevice);

    Buffer *buffer = new MockBuffer();
    auto val = (cl_mem)buffer;
    auto pVal = &val;

    char data[128];
    void *svmPtr = &data;

    builder1D.setKernel(pKernel);
    builder2D.setKernel(pKernel);
    builder3D.setKernel(pKernel);

    Vec3<size_t> gws(256, 256, 256);
    Vec3<size_t> elws(16, 16, 16);
    Vec3<size_t> offset(0, 0, 0);

    builder1D.setDispatchGeometry(SplitDispatch::RegionCoordX::left, gws, elws, offset);
    builder2D.setDispatchGeometry(SplitDispatch::RegionCoordX::left, SplitDispatch::RegionCoordY::top, gws, elws, offset);
    builder3D.setDispatchGeometry(SplitDispatch::RegionCoordX::left, SplitDispatch::RegionCoordY::top, SplitDispatch::RegionCoordZ::front, gws, elws, offset);
    MultiDispatchInfo mdi1D;
    MultiDispatchInfo mdi2D;
    MultiDispatchInfo mdi3D;

    builder1D.bake(mdi1D);
    builder1D.bake(mdi2D);
    builder1D.bake(mdi3D);

    // Set arg
    clearCrossThreadData();
    builder1D.setArg(SplitDispatch::RegionCoordX::left, static_cast<uint32_t>(0), sizeof(cl_mem *), pVal);
    for (auto &dispatchInfo : mdi1D) {
        EXPECT_EQ(buffer->getCpuAddress(), *reinterpret_cast<void **>((dispatchInfo.getKernel()->getCrossThreadData() + 0x10)));
    }
    clearCrossThreadData();
    builder2D.setArg(SplitDispatch::RegionCoordX::left, SplitDispatch::RegionCoordY::top, static_cast<uint32_t>(0), sizeof(cl_mem *), pVal);
    for (auto &dispatchInfo : mdi2D) {
        EXPECT_EQ(buffer->getCpuAddress(), *reinterpret_cast<void **>((dispatchInfo.getKernel()->getCrossThreadData() + 0x10)));
    }
    clearCrossThreadData();
    builder3D.setArg(SplitDispatch::RegionCoordX::left, SplitDispatch::RegionCoordY::top, SplitDispatch::RegionCoordZ::front, static_cast<uint32_t>(0), sizeof(cl_mem *), pVal);
    for (auto &dispatchInfo : mdi3D) {
        EXPECT_EQ(buffer->getCpuAddress(), *reinterpret_cast<void **>((dispatchInfo.getKernel()->getCrossThreadData() + 0x10)));
    }

    // Set arg SVM
    clearCrossThreadData();
    builder1D.setArgSvm(SplitDispatch::RegionCoordX::left, 1, sizeof(svmPtr), svmPtr, nullptr, 0u);
    for (auto &dispatchInfo : mdi1D) {
        EXPECT_EQ(svmPtr, *(reinterpret_cast<void **>(dispatchInfo.getKernel()->getCrossThreadData() + 0x30)));
    }
    clearCrossThreadData();
    builder2D.setArgSvm(SplitDispatch::RegionCoordX::left, SplitDispatch::RegionCoordY::top, 1, sizeof(svmPtr), svmPtr, nullptr, 0u);
    for (auto &dispatchInfo : mdi2D) {
        EXPECT_EQ(svmPtr, *(reinterpret_cast<void **>(dispatchInfo.getKernel()->getCrossThreadData() + 0x30)));
    }
    clearCrossThreadData();
    builder3D.setArgSvm(SplitDispatch::RegionCoordX::left, SplitDispatch::RegionCoordY::top, SplitDispatch::RegionCoordZ::front, 1, sizeof(svmPtr), svmPtr, nullptr, 0u);
    for (auto &dispatchInfo : mdi3D) {
        EXPECT_EQ(svmPtr, *(reinterpret_cast<void **>(dispatchInfo.getKernel()->getCrossThreadData() + 0x30)));
    }

    delete buffer;
}

TEST_F(DispatchInfoBuilderTest, GivenInvalidInputWhenSettingKernelArgThenInvalidMemObjectErrorIsReturned) {

    char *buffer = new char[sizeof(Buffer)];
    auto val = (cl_mem)buffer;
    auto pVal = &val;

    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    diBuilder->setKernel(pKernel);
    diBuilder->setDispatchGeometry(Vec3<size_t>(256, 256, 256), Vec3<size_t>(16, 16, 16), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo multiDispatchInfo;

    diBuilder->bake(multiDispatchInfo);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, diBuilder->setArg(0, sizeof(cl_mem *), pVal));
    EXPECT_EQ(CL_SUCCESS, diBuilder->setArgSvm(1, sizeof(void *), nullptr, nullptr, 0u));

    delete diBuilder;
    delete[] buffer;
}

TEST_F(DispatchInfoBuilderTest, GivenNullKernelWhenSettingKernelArgThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    Buffer *buffer = new MockBuffer();
    auto val = (cl_mem)buffer;
    auto pVal = &val;
    char data[128];
    void *svmPtr = &data;
    MockGraphicsAllocation svmAlloc(svmPtr, 128);

    DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit> *diBuilder = new DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    diBuilder->setDispatchGeometry(Vec3<size_t>(256, 256, 256), Vec3<size_t>(16, 16, 16), Vec3<size_t>(0, 0, 0));
    MultiDispatchInfo multiDispatchInfo;

    diBuilder->bake(multiDispatchInfo);
    EXPECT_EQ(CL_SUCCESS, diBuilder->setArg(0, sizeof(cl_mem *), pVal));
    EXPECT_EQ(CL_SUCCESS, diBuilder->setArgSvm(1, sizeof(svmPtr), svmPtr, nullptr, 0u));
    EXPECT_EQ(CL_SUCCESS, diBuilder->setArgSvmAlloc(2, svmPtr, &svmAlloc));

    delete diBuilder;
    delete buffer;
}

TEST_F(DispatchInfoBuilderTest, WhenDimensionIsNotSetThenProperDimensionIsReturned) {
    MultiDispatchInfo mdi;

    auto diBuilder = std::make_unique<DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder);

    diBuilder->setDispatchGeometry(0u, Vec3<size_t>(128, 4, 1), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi);
    for (auto &dispatchInfo : mdi) {
        EXPECT_EQ(2u, dispatchInfo.getDim());
    }
}

TEST_F(DispatchInfoBuilderTest, WhengDimensionIsNotMatchingGWSThenDimensionPassedAsArgumentIsReturned) {
    MultiDispatchInfo mdi;

    auto diBuilder = std::make_unique<DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::noSplit>>(*pClDevice);
    ASSERT_NE(nullptr, diBuilder);

    diBuilder->setDispatchGeometry(2u, Vec3<size_t>(128, 1, 1), Vec3<size_t>(0, 0, 0), Vec3<size_t>(0, 0, 0));
    diBuilder->bake(mdi);
    for (auto &dispatchInfo : mdi) {
        EXPECT_EQ(2u, dispatchInfo.getDim());
    }
}

} // namespace NEO
