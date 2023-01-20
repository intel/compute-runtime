/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include "drm/xe_drm.h"

using namespace NEO;
using NEO::PrelimI915::drm_syncobj_create;
using NEO::PrelimI915::drm_syncobj_destroy;
using NEO::PrelimI915::drm_syncobj_wait;

struct MockIoctlHelperXe : IoctlHelperXe {
    using IoctlHelperXe::xeDecanonize;
    using IoctlHelperXe::xeGetBindOpName;
    using IoctlHelperXe::xeGetClassName;
    using IoctlHelperXe::xeGetengineClassName;
};

TEST(IoctlHelperXeTest, givenXeDrmVersionsWhenGettingIoctlHelperThenValidIoctlHelperIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingAnyMethodThenDummyValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    ASSERT_NE(nullptr, xeIoctlHelper);

    auto verifyDrmParamString = [&xeIoctlHelper](const char *string, DrmParam drmParam) {
        EXPECT_STREQ(string, xeIoctlHelper->getDrmParamString(drmParam).c_str());
    };

    auto verifyIoctlString = [&xeIoctlHelper](DrmIoctl drmIoctl, const char *string) {
        EXPECT_STREQ(string, xeIoctlHelper->getIoctlString(drmIoctl).c_str());
    };

    auto verifyDrmGetParamValue = [&xeIoctlHelper](auto value, DrmParam drmParam) {
        EXPECT_EQ(value, xeIoctlHelper->getDrmParamValue(drmParam));
    };

    MemRegionsVec memRegions{};
    uint32_t handle = 0u;
    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, 0u, handle, {}, -1));

    EXPECT_TRUE(xeIoctlHelper->isVmBindAvailable());

    EXPECT_FALSE(xeIoctlHelper->isSetPairAvailable());

    EXPECT_EQ(CacheRegion::None, xeIoctlHelper->closAlloc());

    EXPECT_EQ(0u, xeIoctlHelper->closAllocWays(CacheRegion::None, 0u, 0u));

    EXPECT_EQ(CacheRegion::None, xeIoctlHelper->closFree(CacheRegion::None));

    EXPECT_EQ(0, xeIoctlHelper->waitUserFence(0, 0, 0, 0, 0, 0));

    EXPECT_EQ(0u, xeIoctlHelper->getAtomicAdvise(false));

    EXPECT_EQ(0u, xeIoctlHelper->getPreferredLocationAdvise());

    EXPECT_FALSE(xeIoctlHelper->setVmBoAdvise(0, 0, nullptr));

    EXPECT_FALSE(xeIoctlHelper->setVmPrefetch(0, 0, 0, 0));

    EXPECT_EQ(0u, xeIoctlHelper->getDirectSubmissionFlag());

    StackVec<uint32_t, 2> bindExtHandles;
    EXPECT_EQ(nullptr, xeIoctlHelper->prepareVmBindExt(bindExtHandles));

    EXPECT_EQ(0u, xeIoctlHelper->getFlagsForVmBind(false, false, false));

    std::vector<QueryItem> queryItems;
    std::vector<DistanceInfo> distanceInfos;
    EXPECT_EQ(0, xeIoctlHelper->queryDistances(queryItems, distanceInfos));
    EXPECT_EQ(0u, distanceInfos.size());

    EXPECT_EQ(PRELIM_I915_UFENCE_WAIT_SOFT, xeIoctlHelper->getWaitUserFenceSoftFlag());

    EXPECT_EQ(0, xeIoctlHelper->execBuffer(nullptr, 0, 0));

    EXPECT_FALSE(xeIoctlHelper->completionFenceExtensionSupported(false));

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getHasPageFaultParamId());

    EXPECT_EQ(nullptr, xeIoctlHelper->createVmControlExtRegion({}));

    EXPECT_EQ(0u, xeIoctlHelper->getFlagsForVmCreate(false, false, false));

    GemContextCreateExt gcc;
    EXPECT_EQ(0u, xeIoctlHelper->createContextWithAccessCounters(gcc));

    EXPECT_EQ(0u, xeIoctlHelper->createCooperativeContext(gcc));

    VmBindExtSetPatT vmBindExtSetPat{};
    EXPECT_NO_THROW(xeIoctlHelper->fillVmBindExtSetPat(vmBindExtSetPat, 0, 0));

    VmBindExtUserFenceT vmBindExtUserFence{};
    EXPECT_NO_THROW(xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, 0, 0, 0));

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getCopyClassSaturatePCIECapability());

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getCopyClassSaturateLinkCapability());

    EXPECT_EQ(0u, xeIoctlHelper->getVmAdviseAtomicAttribute());

    VmBindParams vmBindParams{};
    EXPECT_EQ(-1, xeIoctlHelper->vmBind(vmBindParams));

    EXPECT_EQ(-1, xeIoctlHelper->vmUnbind(vmBindParams));

    std::array<uint64_t, 12u> properties;
    EXPECT_FALSE(xeIoctlHelper->getEuStallProperties(properties, 0, 0, 0, 0, 0));

    EXPECT_EQ(0u, xeIoctlHelper->getEuStallFdParameter());

    std::string uuid{};
    auto registerUuidResult = xeIoctlHelper->registerUuid(uuid, 0, 0, 0);
    EXPECT_EQ(0, registerUuidResult.retVal);
    EXPECT_EQ(0u, registerUuidResult.handle);

    auto registerStringClassUuidResult = xeIoctlHelper->registerStringClassUuid(uuid, 0, 0);
    EXPECT_EQ(0, registerStringClassUuidResult.retVal);
    EXPECT_EQ(0u, registerStringClassUuidResult.handle);

    EXPECT_EQ(0, xeIoctlHelper->unregisterUuid(0));

    EXPECT_FALSE(xeIoctlHelper->isContextDebugSupported());

    EXPECT_EQ(0, xeIoctlHelper->setContextDebugFlag(0));

    EXPECT_FALSE(xeIoctlHelper->isDebugAttachAvailable());

    // Default no translation:
    verifyDrmGetParamValue(static_cast<int>(DrmParam::ExecRender), DrmParam::ExecRender);
    // test exception:
    verifyDrmGetParamValue(NEO::PrelimI915::I915_MEMORY_CLASS_DEVICE, DrmParam::MemoryClassDevice);
    verifyDrmGetParamValue(NEO::PrelimI915::I915_MEMORY_CLASS_SYSTEM, DrmParam::MemoryClassSystem);
    verifyDrmGetParamValue(NEO::PrelimI915::I915_ENGINE_CLASS_RENDER, DrmParam::EngineClassRender);
    verifyDrmGetParamValue(NEO::PrelimI915::I915_ENGINE_CLASS_COPY, DrmParam::EngineClassCopy);
    verifyDrmGetParamValue(NEO::PrelimI915::I915_ENGINE_CLASS_VIDEO, DrmParam::EngineClassVideo);
    verifyDrmGetParamValue(NEO::PrelimI915::I915_ENGINE_CLASS_VIDEO_ENHANCE, DrmParam::EngineClassVideoEnhance);
    verifyDrmGetParamValue(NEO::PrelimI915::I915_ENGINE_CLASS_COMPUTE, DrmParam::EngineClassCompute);
    verifyDrmGetParamValue(NEO::PrelimI915::I915_ENGINE_CLASS_INVALID, DrmParam::EngineClassInvalid);

    // Expect stringify
    verifyDrmParamString("ContextCreateExtSetparam", DrmParam::ContextCreateExtSetparam);
    verifyDrmParamString("ContextCreateExtSetparam", DrmParam::ContextCreateExtSetparam);
    verifyDrmParamString("ContextCreateFlagsUseExtensions", DrmParam::ContextCreateFlagsUseExtensions);
    verifyDrmParamString("ContextEnginesExtLoadBalance", DrmParam::ContextEnginesExtLoadBalance);
    verifyDrmParamString("ContextParamEngines", DrmParam::ContextParamEngines);
    verifyDrmParamString("ContextParamGttSize", DrmParam::ContextParamGttSize);
    verifyDrmParamString("ContextParamPersistence", DrmParam::ContextParamPersistence);
    verifyDrmParamString("ContextParamPriority", DrmParam::ContextParamPriority);
    verifyDrmParamString("ContextParamRecoverable", DrmParam::ContextParamRecoverable);
    verifyDrmParamString("ContextParamSseu", DrmParam::ContextParamSseu);
    verifyDrmParamString("ContextParamVm", DrmParam::ContextParamVm);
    verifyDrmParamString("EngineClassRender", DrmParam::EngineClassRender);
    verifyDrmParamString("EngineClassCompute", DrmParam::EngineClassCompute);
    verifyDrmParamString("EngineClassCopy", DrmParam::EngineClassCopy);
    verifyDrmParamString("EngineClassVideo", DrmParam::EngineClassVideo);
    verifyDrmParamString("EngineClassVideoEnhance", DrmParam::EngineClassVideoEnhance);
    verifyDrmParamString("EngineClassInvalid", DrmParam::EngineClassInvalid);
    verifyDrmParamString("EngineClassInvalidNone", DrmParam::EngineClassInvalidNone);
    verifyDrmParamString("ExecBlt", DrmParam::ExecBlt);
    verifyDrmParamString("ExecDefault", DrmParam::ExecDefault);
    verifyDrmParamString("ExecNoReloc", DrmParam::ExecNoReloc);
    verifyDrmParamString("ExecRender", DrmParam::ExecRender);
    verifyDrmParamString("MemoryClassDevice", DrmParam::MemoryClassDevice);
    verifyDrmParamString("MemoryClassSystem", DrmParam::MemoryClassSystem);
    verifyDrmParamString("MmapOffsetWb", DrmParam::MmapOffsetWb);
    verifyDrmParamString("MmapOffsetWc", DrmParam::MmapOffsetWc);
    verifyDrmParamString("ParamChipsetId", DrmParam::ParamChipsetId);
    verifyDrmParamString("ParamRevision", DrmParam::ParamRevision);
    verifyDrmParamString("ParamHasExecSoftpin", DrmParam::ParamHasExecSoftpin);
    verifyDrmParamString("ParamHasPooledEu", DrmParam::ParamHasPooledEu);
    verifyDrmParamString("ParamHasScheduler", DrmParam::ParamHasScheduler);
    verifyDrmParamString("ParamEuTotal", DrmParam::ParamEuTotal);
    verifyDrmParamString("ParamSubsliceTotal", DrmParam::ParamSubsliceTotal);
    verifyDrmParamString("ParamMinEuInPool", DrmParam::ParamMinEuInPool);
    verifyDrmParamString("ParamCsTimestampFrequency", DrmParam::ParamCsTimestampFrequency);
    verifyDrmParamString("ParamHasVmBind", DrmParam::ParamHasVmBind);
    verifyDrmParamString("ParamHasPageFault", DrmParam::ParamHasPageFault);
    verifyDrmParamString("QueryEngineInfo", DrmParam::QueryEngineInfo);
    verifyDrmParamString("QueryHwconfigTable", DrmParam::QueryHwconfigTable);
    verifyDrmParamString("QueryComputeSlices", DrmParam::QueryComputeSlices);
    verifyDrmParamString("QueryMemoryRegions", DrmParam::QueryMemoryRegions);
    verifyDrmParamString("QueryTopologyInfo", DrmParam::QueryTopologyInfo);
    verifyDrmParamString("SchedulerCapPreemption", DrmParam::SchedulerCapPreemption);
    verifyDrmParamString("TilingNone", DrmParam::TilingNone);
    verifyDrmParamString("TilingY", DrmParam::TilingY);

    verifyIoctlString(DrmIoctl::SyncobjCreate, "DRM_IOCTL_SYNCOBJ_CREATE");
    verifyIoctlString(DrmIoctl::SyncobjWait, "DRM_IOCTL_SYNCOBJ_WAIT");
    verifyIoctlString(DrmIoctl::SyncobjDestroy, "DRM_IOCTL_SYNCOBJ_DESTROY");
    verifyIoctlString(DrmIoctl::GemClose, "DRM_IOCTL_GEM_CLOSE");
    verifyIoctlString(DrmIoctl::GemVmCreate, "DRM_IOCTL_XE_VM_CREATE");
    verifyIoctlString(DrmIoctl::GemVmDestroy, "DRM_IOCTL_XE_VM_DESTROY");
    verifyIoctlString(DrmIoctl::GemMmapOffset, "DRM_IOCTL_XE_GEM_MMAP_OFFSET");
    verifyIoctlString(DrmIoctl::GemCreate, "DRM_IOCTL_XE_GEM_CREATE");
    verifyIoctlString(DrmIoctl::GemExecbuffer2, "DRM_IOCTL_XE_EXEC");
    verifyIoctlString(DrmIoctl::GemVmBind, "DRM_IOCTL_XE_VM_BIND");
    verifyIoctlString(DrmIoctl::Query, "DRM_IOCTL_XE_DEVICE_QUERY");
    verifyIoctlString(DrmIoctl::GemContextCreateExt, "DRM_IOCTL_XE_ENGINE_CREATE");
    verifyIoctlString(DrmIoctl::GemContextDestroy, "DRM_IOCTL_XE_ENGINE_DESTROY");
    verifyIoctlString(DrmIoctl::GemWaitUserFence, "DRM_IOCTL_XE_WAIT_USER_FENCE");
    verifyIoctlString(DrmIoctl::PrimeFdToHandle, "DRM_IOCTL_PRIME_FD_TO_HANDLE");
    verifyIoctlString(DrmIoctl::PrimeHandleToFd, "DRM_IOCTL_PRIME_HANDLE_TO_FD");
    verifyIoctlString(DrmIoctl::RegRead, "DRM_IOCTL_XE_MMIO");

    EXPECT_TRUE(xeIoctlHelper->completionFenceExtensionSupported(true));

    EXPECT_EQ(static_cast<uint32_t>(XE_NEO_VMCREATE_DISABLESCRATCH_FLAG |
                                    XE_NEO_VMCREATE_ENABLEPAGEFAULT_FLAG |
                                    XE_NEO_VMCREATE_USEVMBIND_FLAG),
              xeIoctlHelper->getFlagsForVmCreate(true, true, true));

    EXPECT_EQ(static_cast<uint64_t>(XE_NEO_BIND_CAPTURE_FLAG |
                                    XE_NEO_BIND_IMMEDIATE_FLAG |
                                    XE_NEO_BIND_MAKERESIDENT_FLAG),
              xeIoctlHelper->getFlagsForVmBind(true, true, true));

    uint32_t fabricId = 0, latency = 0, bandwidth = 0;
    EXPECT_FALSE(xeIoctlHelper->getFabricLatency(fabricId, latency, bandwidth));
}

TEST(IoctlHelperXeTest, whenGettingIoctlRequestValueThenPropertValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    ASSERT_NE(nullptr, xeIoctlHelper);

    auto verifyIoctlRequestValue = [&xeIoctlHelper](auto value, DrmIoctl drmIoctl) {
        EXPECT_EQ(xeIoctlHelper->getIoctlRequestValue(drmIoctl), static_cast<unsigned int>(value));
    };

    verifyIoctlRequestValue(DRM_IOCTL_XE_EXEC, DrmIoctl::GemExecbuffer2);
    verifyIoctlRequestValue(DRM_IOCTL_XE_WAIT_USER_FENCE, DrmIoctl::GemWaitUserFence);
    verifyIoctlRequestValue(DRM_IOCTL_XE_GEM_CREATE, DrmIoctl::GemCreate);
    verifyIoctlRequestValue(DRM_IOCTL_XE_VM_BIND, DrmIoctl::GemVmBind);
    verifyIoctlRequestValue(DRM_IOCTL_XE_VM_CREATE, DrmIoctl::GemVmCreate);
    verifyIoctlRequestValue(DRM_IOCTL_SYNCOBJ_CREATE, DrmIoctl::SyncobjCreate);
    verifyIoctlRequestValue(DRM_IOCTL_SYNCOBJ_WAIT, DrmIoctl::SyncobjWait);
    verifyIoctlRequestValue(DRM_IOCTL_SYNCOBJ_DESTROY, DrmIoctl::SyncobjDestroy);
    verifyIoctlRequestValue(DRM_IOCTL_GEM_CLOSE, DrmIoctl::GemClose);
    verifyIoctlRequestValue(DRM_IOCTL_XE_VM_DESTROY, DrmIoctl::GemVmDestroy);
    verifyIoctlRequestValue(DRM_IOCTL_XE_GEM_MMAP_OFFSET, DrmIoctl::GemMmapOffset);
    verifyIoctlRequestValue(DRM_IOCTL_XE_DEVICE_QUERY, DrmIoctl::Query);
    verifyIoctlRequestValue(DRM_IOCTL_XE_ENGINE_CREATE, DrmIoctl::GemContextCreateExt);
    verifyIoctlRequestValue(DRM_IOCTL_XE_ENGINE_DESTROY, DrmIoctl::GemContextDestroy);
    verifyIoctlRequestValue(DRM_IOCTL_PRIME_FD_TO_HANDLE, DrmIoctl::PrimeFdToHandle);
    verifyIoctlRequestValue(DRM_IOCTL_PRIME_HANDLE_TO_FD, DrmIoctl::PrimeHandleToFd);
    verifyIoctlRequestValue(DRM_IOCTL_XE_MMIO, DrmIoctl::RegRead);

    EXPECT_THROW(xeIoctlHelper->getIoctlRequestValue(DrmIoctl::DebuggerOpen), std::runtime_error);
}

int verifTopology(std::vector<uint8_t> *gen, std::vector<uint8_t> *org) {
    if (gen->size() == org->size()) {
        if (equal(gen->begin(), gen->end(), org->begin())) {
            return 1;
        }
    }
    return 0;
}

int testTopoDg1(IoctlHelperXe *xeioctl) {
    std::vector<uint8_t> geomDss = {0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> computeDss = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> euDss = {0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> topologyFakei915 = {
        0x00, 0x00, 0x01, 0x00, 0x06, 0x00, 0x10, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00,
        0x01, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    std::vector<uint8_t> i915Topo = xeioctl->xeRebuildi915Topology(&geomDss, &computeDss, &euDss);
    return verifTopology(&i915Topo, &topologyFakei915);
}

int testTopoPvc(IoctlHelperXe *xeioctl) {
    std::vector<uint8_t> geomDss = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> computeDss = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    std::vector<uint8_t> euDss = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> topologyFakei915 = {
        0x00, 0x00, 0x01, 0x00, 0x40, 0x00, 0x08, 0x00,
        0x01, 0x00, 0x08, 0x00, 0x09, 0x00, 0x01, 0x00,
        0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff};

    std::vector<uint8_t> i915Topo = xeioctl->xeRebuildi915Topology(&geomDss, &computeDss, &euDss);
    return verifTopology(&i915Topo, &topologyFakei915);
}

int testTopoDg2(IoctlHelperXe *xeioctl) {
    std::vector<uint8_t> geomDss = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> computeDss = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> euDss = {0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    std::vector<uint8_t> topologyFakei915 = {
        0x00, 0x00, 0x01, 0x00, 0x20, 0x00, 0x10, 0x00,
        0x01, 0x00, 0x04, 0x00, 0x05, 0x00, 0x02, 0x00,
        0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff};

    std::vector<uint8_t> i915Topo = xeioctl->xeRebuildi915Topology(&geomDss, &computeDss, &euDss);
    return verifTopology(&i915Topo, &topologyFakei915);
}

TEST(IoctlHelperXeTest, verifyPublicFunctions) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    auto mockXeIoctlHelper = static_cast<MockIoctlHelperXe *>(xeIoctlHelper.get());

    auto verifyXeClassName = [&mockXeIoctlHelper](const char *name, auto xeClass) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetClassName(xeClass));
    };

    auto verifyXeOpBindName = [&mockXeIoctlHelper](const char *name, auto bind) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetBindOpName(bind));
    };

    auto verifyXeEngineClassName = [&mockXeIoctlHelper](const char *name, auto engineClass) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetengineClassName(engineClass));
    };

    verifyXeClassName("rcs", DRM_XE_ENGINE_CLASS_RENDER);
    verifyXeClassName("bcs", DRM_XE_ENGINE_CLASS_COPY);
    verifyXeClassName("vcs", DRM_XE_ENGINE_CLASS_VIDEO_DECODE);
    verifyXeClassName("vecs", DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE);
    verifyXeClassName("ccs", DRM_XE_ENGINE_CLASS_COMPUTE);

    verifyXeOpBindName("MAP", XE_VM_BIND_OP_MAP);
    verifyXeOpBindName("UNMAP", XE_VM_BIND_OP_UNMAP);
    verifyXeOpBindName("MAP_USERPTR", XE_VM_BIND_OP_MAP_USERPTR);
    verifyXeOpBindName("AS_MAP", XE_VM_BIND_OP_MAP | XE_VM_BIND_FLAG_ASYNC);
    verifyXeOpBindName("AS_MAP_USERPTR", XE_VM_BIND_OP_MAP_USERPTR | XE_VM_BIND_FLAG_ASYNC);
    verifyXeOpBindName("unknown_OP", -1);

    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_RENDER", DRM_XE_ENGINE_CLASS_RENDER);
    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_COPY", DRM_XE_ENGINE_CLASS_COPY);
    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_VIDEO_DECODE", DRM_XE_ENGINE_CLASS_VIDEO_DECODE);
    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE", DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE);
    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_COMPUTE", DRM_XE_ENGINE_CLASS_COMPUTE);
    verifyXeEngineClassName("?", 0xffffffff);

    // Default is 48b
    EXPECT_EQ(0xffffffa10000ul, mockXeIoctlHelper->xeDecanonize(0xffffffffffa10000));

    Query query{};
    QueryItem queryItem{};
    queryItem.queryId = 999999;
    queryItem.length = 0;
    queryItem.flags = 0;
    query.itemsPtr = reinterpret_cast<uint64_t>(&queryItem);
    query.numItems = 1;

    EXPECT_EQ(-1, mockXeIoctlHelper->ioctl(DrmIoctl::Query, &query));
    queryItem.queryId = xeIoctlHelper->getDrmParamValue(DrmParam::QueryHwconfigTable);
    mockXeIoctlHelper->ioctl(DrmIoctl::Query, &query);
    EXPECT_EQ(0, queryItem.length);

    memset(&queryItem, 0, sizeof(queryItem));
    queryItem.queryId = xeIoctlHelper->getDrmParamValue(DrmParam::QueryEngineInfo);
    mockXeIoctlHelper->ioctl(DrmIoctl::Query, &query);
    EXPECT_EQ(0, queryItem.length);

    memset(&queryItem, 0, sizeof(queryItem));
    queryItem.queryId = xeIoctlHelper->getDrmParamValue(DrmParam::QueryTopologyInfo);
    mockXeIoctlHelper->ioctl(DrmIoctl::Query, &query);
    EXPECT_EQ(0, queryItem.length);

    EXPECT_EQ(1, testTopoDg1(mockXeIoctlHelper));
    EXPECT_EQ(1, testTopoDg2(mockXeIoctlHelper));
    EXPECT_EQ(1, testTopoPvc(mockXeIoctlHelper));

    std::vector<uint8_t> empty = {};
    std::vector<uint8_t> verif1 = mockXeIoctlHelper->xeRebuildi915Topology(&empty, &empty, &empty);
    EXPECT_EQ(0u, verif1.size());
    std::vector<uint8_t> zero = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> verif2 = mockXeIoctlHelper->xeRebuildi915Topology(&zero, &zero, &zero);
    EXPECT_EQ(0u, verif2.size());
}

TEST(IoctlHelperXeTest, whenGettingFileNamesForFrequencyThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto ioctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_STREQ("/device/gt0/freq_max", ioctlHelper->getFileForMaxGpuFrequency().c_str());
    EXPECT_STREQ("/device/gt2/freq_max", ioctlHelper->getFileForMaxGpuFrequencyOfSubDevice(2).c_str());
    EXPECT_STREQ("/device/gt1/freq_rp0", ioctlHelper->getFileForMaxMemoryFrequencyOfSubDevice(1).c_str());
}

inline constexpr int testValueVmId = 0x5764;
inline constexpr int testValueMapOff = 0x7788;
inline constexpr int testValuePrime = 0x4321;

class DrmMockXe : public DrmMockCustom {
  public:
    DrmMockXe(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockCustom(rootDeviceEnvironment){};

    void testMode(int f, int a = 0) {
        forceIoctlAnswer = f;
        setIoctlAnswer = a;
    }
    int ioctl(DrmIoctl request, void *arg) override {
        int ret = -1;
        if (forceIoctlAnswer) {
            return setIoctlAnswer;
        }
        switch (request) {
        case DrmIoctl::RegRead: {
            struct drm_xe_mmio *reg = static_cast<struct drm_xe_mmio *>(arg);
            reg->value = reg->addr;
            ret = 0;
        } break;
        case DrmIoctl::GemVmCreate: {
            struct drm_xe_vm_create *v = static_cast<struct drm_xe_vm_create *>(arg);
            v->vm_id = testValueVmId;
            ret = 0;
        } break;
        case DrmIoctl::GemUserptr:
        case DrmIoctl::GemClose:
            ret = 0;
            break;
        case DrmIoctl::GemVmDestroy: {
            struct drm_xe_vm_destroy *v = static_cast<struct drm_xe_vm_destroy *>(arg);
            if (v->vm_id == testValueVmId)
                ret = 0;
        } break;
        case DrmIoctl::GemMmapOffset: {
            struct drm_xe_gem_mmap_offset *v = static_cast<struct drm_xe_gem_mmap_offset *>(arg);
            if (v->handle == testValueMapOff) {
                v->offset = v->handle;
                ret = 0;
            }
        } break;
        case DrmIoctl::PrimeFdToHandle: {
            PrimeHandle *v = static_cast<PrimeHandle *>(arg);
            if (v->fileDescriptor == testValuePrime) {
                v->handle = testValuePrime;
                ret = 0;
            }
        } break;
        case DrmIoctl::PrimeHandleToFd: {
            PrimeHandle *v = static_cast<PrimeHandle *>(arg);
            if (v->handle == testValuePrime) {
                v->fileDescriptor = testValuePrime;
                ret = 0;
            }
        } break;
        case DrmIoctl::Getparam:
        case DrmIoctl::GetResetStats:
            ret = -2;
            break;
        case DrmIoctl::Query: {
            struct drm_xe_device_query *v = static_cast<struct drm_xe_device_query *>(arg);
            switch (v->query) {
            case DRM_XE_DEVICE_QUERY_ENGINES:
                break;
            }
            ret = 0;
        } break;
        case DrmIoctl::GemContextSetparam:
        case DrmIoctl::GemContextGetparam:
        default:
            break;
        }
        return ret;
    }
    int forceIoctlAnswer = 0;
    int setIoctlAnswer = 0;
};

TEST(IoctlHelperXeTest, whenCallingIoctlThenProperValueIsReturned) {
    int ret;
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    auto mockXeIoctlHelper = static_cast<MockIoctlHelperXe *>(xeIoctlHelper.get());

    drm.reset();
    {
        drm.testMode(1, -1);
        ret = mockXeIoctlHelper->initialize();
        EXPECT_EQ(0, ret);
    }
    drm.testMode(0);
    {
        GemUserPtr test = {};
        test.handle = 1;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemUserptr, &test);
        EXPECT_EQ(0, ret);
        GemClose cl = {};
        cl.handle = test.handle;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemClose, &cl);
        EXPECT_EQ(0, ret);
    }
    {
        RegisterRead test = {};
        test.offset = REG_GLOBAL_TIMESTAMP_LDW;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::RegRead, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(test.offset, test.value);
    }
    {
        GemVmControl test = {};
        test.flags = 3;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemVmCreate, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.vmId), testValueVmId);
    }
    {
        GemVmControl test = {};
        test.vmId = testValueVmId;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemVmDestroy, &test);
        EXPECT_EQ(0, ret);
    }
    {
        GemMmapOffset test = {};
        test.handle = testValueMapOff;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemMmapOffset, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.offset), testValueMapOff);
    }
    {
        PrimeHandle test = {};
        test.fileDescriptor = testValuePrime;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::PrimeFdToHandle, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.handle), testValuePrime);
    }
    {
        PrimeHandle test = {};
        test.handle = testValuePrime;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::PrimeHandleToFd, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.fileDescriptor), testValuePrime);
    }
    {
        ResetStats test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GetResetStats, &test);
        EXPECT_EQ(0, ret);
    }
    {
        Query test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::Query, &test);
        EXPECT_EQ(-1, ret);
    }
    {
        GemContextParam test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemContextSetparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::ContextParamPersistence);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemContextSetparam, &test);
        EXPECT_EQ(0, ret);
        test.param = contextPrivateParamBoost;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemContextSetparam, &test);
        EXPECT_EQ(0, ret);
        test.param = static_cast<int>(DrmParam::ContextParamEngines);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemContextSetparam, &test);
        EXPECT_EQ(-1, ret);
    }
    {
        GemContextParam test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemContextGetparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::ContextParamGttSize);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemContextGetparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0x1ull << 48, test.value);
        test.param = static_cast<int>(DrmParam::ContextParamSseu);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemContextGetparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0x55fdd94d4e40ull, test.value);
        test.param = static_cast<int>(DrmParam::ContextParamPersistence);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemContextGetparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0x1ull, test.value);
    }
    {
        GemClose test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::GemClose, &test);
        EXPECT_EQ(0, ret);
    }
    {
        GetParam test = {};
        int dstvalue;
        test.value = &dstvalue;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::Getparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::ParamChipsetId);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::Getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 0);
        test.param = static_cast<int>(DrmParam::ParamRevision);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::Getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 0);
        test.param = static_cast<int>(DrmParam::ParamHasPageFault);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::Getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 0);
        test.param = static_cast<int>(DrmParam::ParamHasExecSoftpin);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::Getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 1);
        test.param = static_cast<int>(DrmParam::ParamHasScheduler);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::Getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<unsigned int>(dstvalue), 0x80000037);
        test.param = static_cast<int>(DrmParam::ParamCsTimestampFrequency);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::Getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 0);
    }
    EXPECT_THROW(mockXeIoctlHelper->ioctl(DrmIoctl::GemContextCreateExt, NULL), std::runtime_error);
    drm.reset();
}
