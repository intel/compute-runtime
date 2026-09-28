/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/linux/ioctl_helper_i915_tests.h"

#include <fcntl.h>

using namespace NEO;

extern std::vector<uint64_t> getEngineInfo(const std::vector<EngineCapabilities> &inputEngines);

INSTANTIATE_TYPED_TEST_SUITE_P(Prelim, IoctlHelperI915TablesTest, ::testing::Types<IoctlHelperPrelim20>);

struct MockIoctlHelperPrelim : public IoctlHelperPrelim20 {
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
};

struct IoctlPrelimHelperTests : ::testing::Test {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    MockIoctlHelperPrelim ioctlHelper{*drm};
};

HWTEST2_F(IoctlPrelimHelperTests, whenGettingIoctlRequestValueThenPropertValueIsReturned, IsAtMostXeCore) {
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::queryContextHealth), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GET_RESET_STATS));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmBind), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_BIND));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmUnbind), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemWaitUserFence), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemCreateExt), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmAdvise), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmPrefetch), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::uuidRegister), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_UUID_REGISTER));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::uuidUnregister), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::debuggerOpen), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemClosReserve), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemClosFree), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemCacheReserve), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE));
}

HWTEST2_F(IoctlPrelimHelperTests, whenGettingDrmParamStringThenProperStringIsReturned, IsAtMostXeCore) {
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramHasVmBind).c_str(), "PRELIM_I915_PARAM_HAS_VM_BIND");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramHasPageFault).c_str(), "PRELIM_I915_PARAM_HAS_PAGE_FAULT");
}

HWTEST2_F(IoctlPrelimHelperTests, whenGettingIoctlRequestStringThenProperStringIsReturned, IsAtMostXeCore) {
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::queryContextHealth).c_str(), "PRELIM_DRM_IOCTL_I915_GET_RESET_STATS");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmBind).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmUnbind).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemWaitUserFence).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemCreateExt).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmAdvise).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmPrefetch).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::uuidRegister).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_REGISTER");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::uuidUnregister).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::debuggerOpen).c_str(), "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemClosReserve).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemClosFree).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemCacheReserve).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE");
}

HWTEST2_F(IoctlPrelimHelperTests, whenGettingDrmParamValueThenPropertValueIsReturned, IsAtMostXeCore) {
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassCompute), static_cast<int>(prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramHasVmBind), static_cast<int>(PRELIM_I915_PARAM_HAS_VM_BIND));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramHasPageFault), static_cast<int>(PRELIM_I915_PARAM_HAS_PAGE_FAULT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryHwconfigTable), static_cast<int>(PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryComputeSlices), static_cast<int>(PRELIM_DRM_I915_QUERY_COMPUTE_SUBSLICES));
}

HWTEST2_F(IoctlPrelimHelperTests, givenEmptyRegionInstanceClassWhenCreatingVmControlRegionExtThenNullptrIsReturned, IsAtMostXeCore) {
    std::optional<MemoryClassInstance> regionInstanceClass{};

    EXPECT_FALSE(regionInstanceClass.has_value());
    EXPECT_EQ(nullptr, ioctlHelper.createVmControlExtRegion(regionInstanceClass));
}

HWTEST2_F(IoctlPrelimHelperTests, givenValidRegionInstanceClassWhenCreatingVmControlRegionExtThenProperStructIsReturned, IsAtMostXeCore) {
    std::optional<MemoryClassInstance> regionInstanceClass = MemoryClassInstance{prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE, 2};

    EXPECT_TRUE(regionInstanceClass.has_value());

    auto retVal = ioctlHelper.createVmControlExtRegion(regionInstanceClass);

    EXPECT_NE(nullptr, retVal);

    auto regionExt = reinterpret_cast<prelim_drm_i915_gem_vm_region_ext *>(retVal.get());

    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_GEM_VM_CONTROL_EXT_REGION), regionExt->base.name);
    EXPECT_EQ(static_cast<uint32_t>(prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE), regionExt->region.memory_class);
    EXPECT_EQ(2u, regionExt->region.memory_instance);
}

HWTEST2_F(IoctlPrelimHelperTests, whenGettingFlagsForVmCreateThenProperValueIsReturned, IsAtMostXeCore) {
    for (auto &disableScratch : ::testing::Bool()) {
        for (auto &enablePageFault : ::testing::Bool()) {
            for (auto &useVmBind : ::testing::Bool()) {
                auto flags = ioctlHelper.getFlagsForVmCreate(disableScratch, enablePageFault, useVmBind);
                if (disableScratch) {
                    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH), (flags & PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH));
                }
                if (enablePageFault) {
                    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT), (flags & PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT));
                }
                if (useVmBind) {
                    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_CREATE_FLAGS_USE_VM_BIND), (flags & PRELIM_I915_VM_CREATE_FLAGS_USE_VM_BIND));
                }
                if (disableScratch || enablePageFault || useVmBind) {
                    EXPECT_NE(0u, flags);
                } else {
                    EXPECT_EQ(0u, flags);
                }
            }
        }
    }
}

HWTEST2_F(IoctlPrelimHelperTests, whenGettingFlagsForVmBindThenProperValuesAreReturned, IsAtMostXeCore) {
    for (auto &bindCapture : ::testing::Bool()) {
        for (auto &bindImmediate : ::testing::Bool()) {
            for (auto &bindMakeResident : ::testing::Bool()) {
                for (auto &bindLockedMemory : ::testing::Bool()) {
                    for (auto &readOnlyResource : ::testing::Bool()) {
                        auto flags = ioctlHelper.getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident, bindLockedMemory, readOnlyResource, false);
                        if (bindCapture) {
                            EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_CAPTURE, (flags & PRELIM_I915_GEM_VM_BIND_CAPTURE));
                        }
                        if (bindImmediate) {
                            EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_IMMEDIATE, (flags & PRELIM_I915_GEM_VM_BIND_IMMEDIATE));
                        }
                        if (bindMakeResident || bindLockedMemory) {
                            EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT, (flags & PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT));
                        }
                        if (readOnlyResource) {
                            EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_READONLY, (flags & PRELIM_I915_GEM_VM_BIND_READONLY));
                        }
                        if (flags == 0) {
                            EXPECT_FALSE(bindCapture);
                            EXPECT_FALSE(bindImmediate);
                            EXPECT_FALSE(bindMakeResident);
                            EXPECT_FALSE(bindLockedMemory);
                            EXPECT_FALSE(readOnlyResource);
                        }
                    }
                }
            }
        }
    }
}

HWTEST2_F(IoctlPrelimHelperTests, givenIoctlHelperisVmBindPatIndexExtSupportedReturnsTrue, IsAtMostXeCore) {
    ASSERT_EQ(true, ioctlHelper.isVmBindPatIndexExtSupported());
}

HWTEST2_F(IoctlPrelimHelperTests, givenIoctlHelperSetVmSharedSystemMemAdviseReturnsTrue, IsAtMostXeCore) {
    ASSERT_EQ(true, ioctlHelper.setVmSharedSystemMemAdvise(0u, 0u, 0u, 0u, {0u, 0u}, 0u));
}

HWTEST2_F(IoctlPrelimHelperTests, givenIoctlHelperGetVmSharedSystemAtomicAttributeReturnsDefaultNone, IsAtMostXeCore) {
    ASSERT_EQ(AtomicAccessMode::none, ioctlHelper.getVmSharedSystemAtomicAttribute(0u, 0u, 0u));
}

HWTEST2_F(IoctlPrelimHelperTests, whenGettingVmBindExtFromHandlesThenProperStructsAreReturned, IsAtMostXeCore) {
    StackVec<uint32_t, 2> bindExtHandles;
    bindExtHandles.push_back(1u);
    bindExtHandles.push_back(2u);
    bindExtHandles.push_back(3u);
    auto retVal = ioctlHelper.prepareVmBindExt(bindExtHandles, 0);
    auto vmBindExt = reinterpret_cast<prelim_drm_i915_vm_bind_ext_uuid *>(retVal.get());

    for (size_t i = 0; i < bindExtHandles.size(); i++) {
        EXPECT_EQ(bindExtHandles[i], vmBindExt[i].uuid_handle);
        EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_UUID), vmBindExt[i].base.name);
    }

    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[1]), vmBindExt[0].base.next_extension);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[2]), vmBindExt[1].base.next_extension);
}

HWTEST2_F(IoctlPrelimHelperTests, givenPrelimsWhenGetDirectSubmissionFlagThenCorrectValueReturned, IsAtMostXeCore) {
    EXPECT_EQ(PRELIM_I915_CONTEXT_CREATE_FLAGS_LONG_RUNNING, ioctlHelper.getDirectSubmissionFlag());
}

HWTEST2_F(IoctlPrelimHelperTests, givenPrelimsWhenTranslateToEngineCapsThenReturnSameData, IsAtMostXeCore) {
    std::vector<EngineCapabilities> expectedEngines(2);
    expectedEngines[0] = {{static_cast<uint16_t>(ioctlHelper.getDrmParamValue(DrmParam::engineClassRender)), 0}, {true, false}};
    expectedEngines[1] = {{static_cast<uint16_t>(ioctlHelper.getDrmParamValue(DrmParam::engineClassCopy)), 1}, {false, true}};

    auto engineInfo = getEngineInfo(expectedEngines);

    auto engines = ioctlHelper.translateToEngineCaps(engineInfo);
    EXPECT_EQ(2u, engines.size());
    for (uint32_t i = 0; i < engines.size(); i++) {
        EXPECT_EQ(expectedEngines[i].engine.engineClass, engines[i].engine.engineClass);
        EXPECT_EQ(expectedEngines[i].engine.engineInstance, engines[i].engine.engineInstance);
        EXPECT_EQ(expectedEngines[i].capabilities.copyClassSaturateLink, engines[i].capabilities.copyClassSaturateLink);
        EXPECT_EQ(expectedEngines[i].capabilities.copyClassSaturatePCIE, engines[i].capabilities.copyClassSaturatePCIE);
    }
}

HWTEST2_F(IoctlPrelimHelperTests, givenPrelimsWhenGettingFlagForWaitUserFenceSoftThenProperFlagIsReturned, IsAtMostXeCore) {
    EXPECT_EQ(PRELIM_I915_UFENCE_WAIT_SOFT, ioctlHelper.getWaitUserFenceSoftFlag());
}

HWTEST2_F(IoctlPrelimHelperTests, givenValidInputWhenFillVmBindSetPatThenProperValuesAreSet, IsAtMostXeCore) {
    VmBindExtSetPatT vmBindExtSetPat{};
    prelim_drm_i915_vm_bind_ext_set_pat prelimVmBindExtSetPat{};

    uint64_t expectedPatIndex = 2;
    uint64_t expectedNextExtension = 3;
    ioctlHelper.fillVmBindExtSetPat(vmBindExtSetPat, expectedPatIndex, expectedNextExtension);

    memcpy_s(&prelimVmBindExtSetPat, sizeof(prelimVmBindExtSetPat), vmBindExtSetPat, sizeof(vmBindExtSetPat));

    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_SET_PAT), prelimVmBindExtSetPat.base.name);
    EXPECT_EQ(expectedPatIndex, prelimVmBindExtSetPat.pat_index);
    EXPECT_EQ(expectedNextExtension, prelimVmBindExtSetPat.base.next_extension);
}

HWTEST2_F(IoctlPrelimHelperTests, givenValidInputWhenFillVmBindUserFenceThenProperValuesAreSet, IsAtMostXeCore) {
    VmBindExtUserFenceT vmBindExtUserFence{};
    prelim_drm_i915_vm_bind_ext_user_fence prelimVmBindExtUserFence{};

    uint64_t expectedAddress = 0xdead;
    uint64_t expectedValue = 0xc0de;
    uint64_t expectedNextExtension = 1234;
    uint64_t expectedSize = sizeof(prelimVmBindExtUserFence.base) + sizeof(uint64_t) * 3;
    ioctlHelper.fillVmBindExtUserFence(vmBindExtUserFence, expectedAddress, expectedValue, expectedNextExtension);

    memcpy_s(&prelimVmBindExtUserFence, sizeof(prelimVmBindExtUserFence), vmBindExtUserFence, sizeof(vmBindExtUserFence));

    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_USER_FENCE), prelimVmBindExtUserFence.base.name);
    EXPECT_EQ(expectedAddress, prelimVmBindExtUserFence.addr);
    EXPECT_EQ(expectedValue, prelimVmBindExtUserFence.val);
    EXPECT_EQ(expectedNextExtension, prelimVmBindExtUserFence.base.next_extension);
    EXPECT_EQ(expectedSize, sizeof(prelimVmBindExtUserFence));
}

HWTEST2_F(IoctlPrelimHelperTests, givenPrelimWhenCallingIsEuStallSupportedThenTrueIsReturned, IsAtMostXeCore) {
    EXPECT_TRUE(ioctlHelper.isEuStallSupported());
}

HWTEST2_F(IoctlPrelimHelperTests, givenPrelimWhenCallingPerfOpenEuStallStreamWithInvalidArgumentsThenFailureReturned, IsAtMostXeCore) {
    int32_t invalidStream = -1;
    DrmMock *mockDrm = reinterpret_cast<DrmMock *>(drm.get());
    mockDrm->failPerfOpen = true;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(ioctlHelper.perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidStream));
}

HWTEST2_F(IoctlPrelimHelperTests, givenPrelimWhenGettingEuStallFdParameterThenCorrectIoctlValueIsReturned, IsAtMostXeCore) {
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_PERF_FLAG_FD_EU_STALL), ioctlHelper.getEuStallFdParameter());
}

HWTEST2_F(IoctlPrelimHelperTests, givenIoctlHelperWhenCallingoverrideMaxSlicesSupportedThenResultIsFalse, IsAtMostXeCore) {
    EXPECT_TRUE(ioctlHelper.overrideMaxSlicesSupported());
}

struct MockIoctlHelperPrelim20 : IoctlHelperPrelim20 {
    using IoctlHelperPrelim20::createGemExt;
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    int ioctl(DrmIoctl request, void *arg) override {
        ioctlCallCount++;
        if (request == DrmIoctl::gemCreateExt) {
            lastGemCreateContainedMemPolicy = checkWhetherGemCreateExtContainsMemPolicy(arg);
            if (overrideGemCreateExtReturnValue.has_value()) {
                return *overrideGemCreateExtReturnValue;
            }
        }
        if (request == DrmIoctl::queryContextHealth) {
            EXPECT_EQ(static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GET_RESET_STATS), getIoctlRequestValue(request));
            resetStatsPrelimCalled++;
            if (overrideResetStatsPrelim.has_value()) {
                *static_cast<prelim_drm_i915_reset_stats *>(arg) = overrideResetStatsPrelim.value();
                return overrideResetStatsPrelimReturnValue;
            }
        }

        return IoctlHelperPrelim20::ioctl(request, arg);
    }
    int ioctl(int fd, DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::perfDisable) {
            if (failPerfDisable) {
                return -1;
            }
        }
        if (request == DrmIoctl::perfEnable) {
            if (failPerfEnable) {
                return -1;
            }
        }
        return IoctlHelperPrelim20::ioctl(fd, request, arg);
    }
    bool checkWhetherGemCreateExtContainsMemPolicy(void *arg) {
        auto &gemCreateExt = *reinterpret_cast<prelim_drm_i915_gem_create_ext *>(arg);
        auto pExtensionBase = reinterpret_cast<i915_user_extension *>(gemCreateExt.extensions);
        while (pExtensionBase != nullptr) {
            if (pExtensionBase->name == PRELIM_I915_GEM_CREATE_EXT_MEMORY_POLICY) {
                auto lastPolicy = reinterpret_cast<prelim_drm_i915_gem_create_ext_memory_policy *>(pExtensionBase);
                lastPolicyMode = lastPolicy->mode;
                lastPolicyFlags = lastPolicy->flags;
                lastPolicyNodeMask.clear();
                auto nodeMaskPtr = reinterpret_cast<unsigned long *>(lastPolicy->nodemask_ptr);
                for (auto i = 0u; i < lastPolicy->nodemask_max; i++) {
                    lastPolicyNodeMask.push_back(nodeMaskPtr[i]);
                }
                return true;
            }
            pExtensionBase = reinterpret_cast<i915_user_extension *>(pExtensionBase->next_extension);
        }
        return false;
    }
    size_t ioctlCallCount = 0;
    bool lastGemCreateContainedMemPolicy = false;
    bool failPerfDisable = false;
    bool failPerfEnable = false;
    bool failPerfOpen = false;
    std::optional<int> overrideGemCreateExtReturnValue{};
    uint32_t lastPolicyMode = 0;
    uint32_t lastPolicyFlags = 0;
    std::vector<unsigned long> lastPolicyNodeMask{};
    std::optional<prelim_drm_i915_reset_stats> overrideResetStatsPrelim{};
    int overrideResetStatsPrelimReturnValue = 0;
    size_t resetStatsPrelimCalled = 0;
};

using IoctlPrelimHelperCreateGemExtTests = ::testing::Test;

HWTEST2_F(IoctlPrelimHelperCreateGemExtTests, givenPrelimWhenCreateGemExtWithMemPolicyThenMemPolicyExtensionsIsAdded, IsAtMostXeCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOCreateDestroyResult.set(true);
    StreamCapture capture;
    capture.captureStdout();

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}};
    uint32_t numOfChunks = 0;
    std::vector<unsigned long> memPolicy;
    memPolicy.push_back(1);
    uint32_t memPolicyMode = 0;
    mockIoctlHelper.overrideGemCreateExtReturnValue = 0;
    mockIoctlHelper.initialize();
    auto ret = mockIoctlHelper.createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, memPolicyMode, memPolicy, false);

    std::string output = capture.getCapturedStdout();
    std::string expectedSubstring("memory policy:");
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(mockIoctlHelper.lastGemCreateContainedMemPolicy);
    EXPECT_EQ(0u, mockIoctlHelper.lastPolicyFlags);
    EXPECT_EQ(memPolicyMode, mockIoctlHelper.lastPolicyMode);
    EXPECT_EQ(memPolicy, mockIoctlHelper.lastPolicyNodeMask);
    EXPECT_TRUE(output.find(expectedSubstring) != std::string::npos);
}

HWTEST2_F(IoctlPrelimHelperCreateGemExtTests, givenPrelimWhenCreateGemExtWithMemPolicyAndChunkingThenMemPolicyExtensionsIsAdded, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}};
    uint32_t numOfChunks = 2;
    size_t size = 4 * MemoryConstants::pageSize64k;
    std::vector<unsigned long> memPolicy;
    memPolicy.push_back(1);
    uint32_t memPolicyMode = 0;
    mockIoctlHelper.overrideGemCreateExtReturnValue = 0;
    mockIoctlHelper.initialize();
    auto ret = mockIoctlHelper.createGemExt(memClassInstance, size, handle, 0, {}, -1, true, numOfChunks, memPolicyMode, memPolicy, false);

    EXPECT_EQ(0, ret);
    EXPECT_TRUE(mockIoctlHelper.lastGemCreateContainedMemPolicy);
    EXPECT_EQ(0u, mockIoctlHelper.lastPolicyFlags);
    EXPECT_EQ(memPolicyMode, mockIoctlHelper.lastPolicyMode);
    EXPECT_EQ(memPolicy, mockIoctlHelper.lastPolicyNodeMask);
}

HWTEST2_F(IoctlPrelimHelperCreateGemExtTests, givenPairHandleWhenCreateGemExtThenSetPairExtensionIsChainedWithAndWithoutVmPrivateExtension, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto ioctlHelper = drm.getIoctlHelper();
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    constexpr int32_t pairHandle = 7;

    for (auto vmId : {std::optional<uint32_t>{}, std::optional<uint32_t>{5u}}) {
        drm.context.receivedCreateGemExt.reset();
        uint32_t handle = 0;
        EXPECT_EQ(0, ioctlHelper->createGemExt(memClassInstance, 1024, handle, 0, vmId, pairHandle, false, 0, std::nullopt, std::nullopt, std::nullopt));

        ASSERT_TRUE(drm.context.receivedCreateGemExt);
        EXPECT_EQ(vmId, drm.context.receivedCreateGemExt->vmPrivateExt.vmId);
        ASSERT_TRUE(drm.context.receivedCreateGemExt->pairSetParamExt);
        EXPECT_NE(0u, drm.context.receivedCreateGemExt->pairSetParamExt->param & PRELIM_I915_PARAM_SET_PAIR);
    }
}

HWTEST2_F(IoctlPrelimHelperTests, whenGettingAtomicAdviseThenNonAtomicMapsToNoneAndAtomicMapsToSystem, IsAtMostXeCore) {
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_ADVISE_ATOMIC_NONE), ioctlHelper.getAtomicAdvise(true));
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_ADVISE_ATOMIC_SYSTEM), ioctlHelper.getAtomicAdvise(false));
}

using IoctlPrelimHelperPerfTests = ::testing::Test;

HWTEST2_F(IoctlPrelimHelperPerfTests, givenCalltoPerfDisableEuStallStreamWithValidStreamButCloseFailsThenFailureReturned, IsAtMostXeCore) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fileDescriptor) -> int {
        return -1;
    });

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.initialize();
    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncRetVal = -1;
    int32_t invalidFd = 10;
    EXPECT_FALSE(mockIoctlHelper.perfDisableEuStallStream(&invalidFd));
    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(10, NEO::SysCalls::closeFuncArgPassed);
    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
    NEO::SysCalls::closeFuncRetVal = 0;
}

HWTEST2_F(IoctlPrelimHelperPerfTests, givenCalltoPerfDisableEuStallStreamWithInvalidStreamThenFailureIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.initialize();
    int32_t invalidFd = -1;
    mockIoctlHelper.failPerfDisable = true;
    EXPECT_FALSE(mockIoctlHelper.perfDisableEuStallStream(&invalidFd));
}

HWTEST2_F(IoctlPrelimHelperPerfTests, givenCalltoPerfOpenEuStallStreamWithInvalidStreamWithEnableSetToFailThenFailureReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.initialize();
    int32_t invalidFd = -1;
    mockIoctlHelper.failPerfEnable = true;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(mockIoctlHelper.perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidFd));
}

HWTEST2_F(IoctlPrelimHelperPerfTests, givenCalltoPerfDisableEuStallStreamWithValidStreamThenSuccessIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.initialize();
    int32_t validFd = -1;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_TRUE(mockIoctlHelper.perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));
    EXPECT_TRUE(mockIoctlHelper.perfDisableEuStallStream(&validFd));
}

class DrmMockIoctl : public DrmMock {
  public:
    DrmMockIoctl(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    }
    int handleRemainingRequests(DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::query) {

            Query *query = static_cast<Query *>(arg);
            QueryItem *queryItem = reinterpret_cast<QueryItem *>(query->itemsPtr);
            PrelimI915::prelim_drm_i915_query_fabric_info *info =
                reinterpret_cast<PrelimI915::prelim_drm_i915_query_fabric_info *>(queryItem->dataPtr);

            info->latency = mockLatency;
            info->bandwidth = mockBandwidth;
            return mockIoctlReturn;
        }
        return 0;
    }
    int mockIoctlReturn = 0;
    uint32_t mockLatency = 10;
    uint32_t mockBandwidth = 100;
};

using IoctlPrelimHelperFabricLatencyTest = ::testing::Test;

HWTEST2_F(IoctlPrelimHelperFabricLatencyTest, givenPrelimWhenGettingFabricLatencyThenSuccessIsReturned, IsAtMostXeCore) {

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMockIoctl> drm = std::make_unique<DrmMockIoctl>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t latency = std::numeric_limits<uint32_t>::max(), fabricId = 0, bandwidth = 0;
    EXPECT_TRUE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
    EXPECT_NE(latency, std::numeric_limits<uint32_t>::max());
    EXPECT_NE(bandwidth, 0u);
}

HWTEST2_F(IoctlPrelimHelperFabricLatencyTest, givenPrelimWhenGettingFabricLatencyAndIoctlFailsThenErrorIsReturned, IsAtMostXeCore) {

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMockIoctl> drm = std::make_unique<DrmMockIoctl>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t latency = 0, fabricId = 0, bandwidth = 0;
    drm->mockIoctlReturn = 1;
    EXPECT_FALSE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
}

HWTEST2_F(IoctlPrelimHelperFabricLatencyTest, givenPrelimWhenGettingFabricLatencyAndIoctlSetsZeroForLatencyThenErrorIsReturned, IsAtMostXeCore) {

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMockIoctl> drm = std::make_unique<DrmMockIoctl>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t latency = 0, fabricId = 0, bandwidth = 0;
    drm->mockIoctlReturn = 0;
    drm->mockLatency = 0;
    drm->mockBandwidth = 10;
    EXPECT_FALSE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
}

HWTEST2_F(IoctlPrelimHelperFabricLatencyTest, givenPrelimWhenGettingFabricLatencyAndIoctlSetsZeroForBandwidthThenErrorIsReturned, IsAtMostXeCore) {

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMockIoctl> drm = std::make_unique<DrmMockIoctl>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t latency = 0, fabricId = 0, bandwidth = 0;
    drm->mockIoctlReturn = 0;
    drm->mockLatency = 10;
    drm->mockBandwidth = 0;
    EXPECT_FALSE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
}

HWTEST2_F(IoctlPrelimHelperTests, whenChangingBufferBindingThenWaitIsNeededOnlyBeforeBind, IsAtMostXeCore) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_TRUE(ioctlHelper.requiresUserFenceSetup(true));
    EXPECT_FALSE(ioctlHelper.requiresUserFenceSetup(false));
}

HWTEST2_F(IoctlPrelimHelperTests, whenChangingBufferBindingAndForcingFenceWaitThenCallReturnsTrueForBindAndUnbind, IsAtMostXeCore) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    debugManager.flags.EnableUserFenceUponUnbind.set(1);
    EXPECT_TRUE(ioctlHelper.requiresUserFenceSetup(true));
    EXPECT_TRUE(ioctlHelper.requiresUserFenceSetup(false));
}

HWTEST2_F(IoctlPrelimHelperTests, whenChangingBufferBindingAndNotForcingFenceWaitThenCallReturnsTrueForBindOnly, IsAtMostXeCore) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    debugManager.flags.EnableUserFenceUponUnbind.set(0);
    EXPECT_TRUE(ioctlHelper.requiresUserFenceSetup(true));
    EXPECT_FALSE(ioctlHelper.requiresUserFenceSetup(false));
}

HWTEST2_F(IoctlPrelimHelperTests, whenGettingPreferredLocationRegionThenReturnCorrectMemoryClassAndInstance, IsAtMostXeCore) {
    DebugManagerStateRestore restorer;

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_EQ(std::nullopt, ioctlHelper.getPreferredLocationRegion(PreferredLocation::none, 0));

    auto region = ioctlHelper.getPreferredLocationRegion(PreferredLocation::system, 0);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::memoryClassSystem), region->memoryClass);
    EXPECT_EQ(0u, region->memoryInstance);

    region = ioctlHelper.getPreferredLocationRegion(PreferredLocation::device, 1);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::memoryClassDevice), region->memoryClass);
    EXPECT_EQ(1u, region->memoryInstance);

    region = ioctlHelper.getPreferredLocationRegion(PreferredLocation::clear, 1);
    EXPECT_EQ(static_cast<uint16_t>(-1), region->memoryClass);
    EXPECT_EQ(0u, region->memoryInstance);

    debugManager.flags.SetVmAdvisePreferredLocation.set(3);
    region = ioctlHelper.getPreferredLocationRegion(PreferredLocation::none, 1);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::memoryClassDevice), region->memoryClass);
    EXPECT_EQ(1u, region->memoryInstance);
}

HWTEST2_F(IoctlPrelimHelperTests, WhenQueryHwIpVersionAndSetupIpVersionAreCalledThenIpVersionIsCorrect, IsAtMostXeCore) {
    auto &hwInfo = *drm->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &compilerProductHelper = drm->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);

    hwInfo.ipVersion.value = ioctlHelper.queryHwIpVersion(hwInfo.platform.eProductFamily);
    ioctlHelper.setupIpVersion();
    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

HWTEST2_F(IoctlPrelimHelperTests, whenGetContextHealthIsCalledThenPrelimStatusAndFaultAreTranslated, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    prelim_drm_i915_reset_stats prelimResetStats{};
    prelimResetStats.ctx_id = 0;
    prelimResetStats.status = I915_RESET_STATS_BANNED;
    prelimResetStats.batch_active = 1;
    prelimResetStats.fault.addr = 0x1234;
    prelimResetStats.fault.flags = I915_RESET_STATS_FAULT_VALID;
    mockIoctlHelper.overrideResetStatsPrelim = prelimResetStats;

    ContextHealth contextHealth{};
    contextHealth.contextId = 0;

    EXPECT_EQ(0, mockIoctlHelper.getContextHealth(contextHealth));
    EXPECT_TRUE(contextHealth.banned);
    EXPECT_EQ(ContextBanReason::gpuHang, contextHealth.banReason);
    EXPECT_TRUE(contextHealth.faultValid);
    EXPECT_EQ(0x1234u, contextHealth.fault.addr);
    EXPECT_EQ(1u, mockIoctlHelper.resetStatsPrelimCalled);
    EXPECT_EQ(1u, mockIoctlHelper.ioctlCallCount);
}

HWTEST2_F(IoctlPrelimHelperTests, givenHealthyContextWhenGetContextHealthIsCalledThenNoBanIsReported, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.overrideResetStatsPrelim = prelim_drm_i915_reset_stats{};

    ContextHealth contextHealth{};
    contextHealth.contextId = 0;

    EXPECT_EQ(0, mockIoctlHelper.getContextHealth(contextHealth));
    EXPECT_FALSE(contextHealth.banned);
    EXPECT_EQ(ContextBanReason::none, contextHealth.banReason);
    EXPECT_FALSE(contextHealth.faultValid);
}

HWTEST2_F(IoctlPrelimHelperTests, givenNonZeroReturnValueWhenGetContextHealthIsCalledThenPrelimErrorIsReturnedWithoutUpstreamFallback, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.overrideResetStatsPrelim = prelim_drm_i915_reset_stats{};
    mockIoctlHelper.overrideResetStatsPrelimReturnValue = -1;

    ContextHealth contextHealth{};
    contextHealth.contextId = 0;

    EXPECT_EQ(-1, mockIoctlHelper.getContextHealth(contextHealth));
    EXPECT_EQ(ContextBanReason::none, contextHealth.banReason);
    EXPECT_FALSE(contextHealth.banned);
    EXPECT_FALSE(contextHealth.faultValid);
    EXPECT_EQ(1u, mockIoctlHelper.resetStatsPrelimCalled);
    EXPECT_EQ(1u, mockIoctlHelper.ioctlCallCount);
}

HWTEST2_F(IoctlPrelimHelperTests, givenInterruptedPrelimHealthQueryWhenRetriedThenNativeRequestLoggingAndTimingArePreserved, IsAtMostXeCore) {
    struct DrmWithStatistics : DrmMock {
        using Drm::ioctlStatistics;
        using DrmMock::DrmMock;
        int ioctl(DrmIoctl request, void *arg) override {
            return Drm::ioctl(request, arg);
        }
    };
    DrmWithStatistics drmWithStatistics{*executionEnvironment.rootDeviceEnvironments[0]};
    drmWithStatistics.ioctlHelper = std::make_unique<IoctlHelperPrelim20>(drmWithStatistics);
    auto &helper = *drmWithStatistics.ioctlHelper;
    DebugManagerStateRestore restore;
    debugManager.flags.PrintIoctlEntries.set(true);
    debugManager.flags.PrintKmdTimes.set(true);
    VariableBackup<int> errnoBackup(&errno);
    static uint32_t callCount = 0;
    VariableBackup<uint32_t> callCountBackup(&callCount, 0);
    VariableBackup<decltype(SysCalls::sysCallsIoctl)> ioctlBackup(&SysCalls::sysCallsIoctl);
    SysCalls::sysCallsIoctl = [](int, unsigned long request, void *arg) -> int {
        EXPECT_EQ(static_cast<unsigned long>(PRELIM_DRM_IOCTL_I915_GET_RESET_STATS), request);
        auto &stats = *static_cast<prelim_drm_i915_reset_stats *>(arg);
        EXPECT_EQ(7u, stats.ctx_id);
        if (++callCount == 1) {
            errno = EINTR;
            return -1;
        }
        stats.status = I915_RESET_STATS_BANNED;
        return 0;
    };
    StreamCapture capture;
    capture.captureStdout();
    ContextHealth health{};
    health.contextId = 7;

    EXPECT_EQ(0, helper.getContextHealth(health));

    auto output = capture.getCapturedStdout();
    EXPECT_TRUE(health.banned);
    EXPECT_EQ(2u, callCount);
    EXPECT_EQ(2u, drmWithStatistics.ioctlStatistics[DrmIoctl::queryContextHealth].count);
    EXPECT_NE(std::string::npos, output.find("IOCTL PRELIM_DRM_IOCTL_I915_GET_RESET_STATS called\n"));
    EXPECT_NE(std::string::npos, output.find("IOCTL PRELIM_DRM_IOCTL_I915_GET_RESET_STATS returns -1, errno"));
    EXPECT_NE(std::string::npos, output.find("IOCTL PRELIM_DRM_IOCTL_I915_GET_RESET_STATS returns 0\n"));
    debugManager.flags.PrintKmdTimes.set(false);
}

HWTEST2_F(IoctlPrelimHelperTests, givenFailedPrelimHealthQueryWhenGettingContextHealthThenErrorIsPropagatedWithoutAnotherIoctl, IsAtMostXeCore) {
    struct DrmWithNativeIoctl : DrmMock {
        using DrmMock::DrmMock;
        int ioctl(DrmIoctl request, void *arg) override {
            return Drm::ioctl(request, arg);
        }
    };
    DrmWithNativeIoctl nativeDrm{*executionEnvironment.rootDeviceEnvironments[0]};
    nativeDrm.ioctlHelper = std::make_unique<IoctlHelperPrelim20>(nativeDrm);
    auto &helper = *nativeDrm.ioctlHelper;
    VariableBackup<int> errnoBackup(&errno);
    static uint32_t callCount = 0;
    VariableBackup<uint32_t> callCountBackup(&callCount, 0);
    VariableBackup<decltype(SysCalls::sysCallsIoctl)> ioctlBackup(&SysCalls::sysCallsIoctl);
    SysCalls::sysCallsIoctl = [](int, unsigned long request, void *arg) -> int {
        ++callCount;
        EXPECT_EQ(static_cast<unsigned long>(PRELIM_DRM_IOCTL_I915_GET_RESET_STATS), request);
        EXPECT_EQ(7u, static_cast<prelim_drm_i915_reset_stats *>(arg)->ctx_id);
        errno = EINVAL;
        return -1;
    };
    ContextHealth health{};
    health.contextId = 7;

    EXPECT_EQ(-1, helper.getContextHealth(health));
    EXPECT_EQ(EINVAL, errno);

    EXPECT_EQ(1u, callCount);
    EXPECT_EQ(ContextBanReason::none, health.banReason);
    EXPECT_FALSE(health.banned);
    EXPECT_FALSE(health.faultValid);
}

HWTEST2_F(IoctlPrelimHelperTests, givenExternalContextWhenQueryingHealthThenNativeRequestsAreRoutedThroughExternalIoctl, IsAtMostXeCore) {
    for (bool failPrelim : {false, true}) {
        uint32_t callCount = 0;
        int handle = 0;
        ExternalCtx ctx{&handle, [&](void *passedHandle, int fd, unsigned long request, void *arg, bool flag) -> int {
                            EXPECT_EQ(&handle, passedHandle);
                            EXPECT_EQ(drm->getFileDescriptor(), fd);
                            EXPECT_FALSE(flag);
                            ++callCount;
                            EXPECT_EQ(static_cast<unsigned long>(PRELIM_DRM_IOCTL_I915_GET_RESET_STATS), request);
                            auto &stats = *static_cast<prelim_drm_i915_reset_stats *>(arg);
                            EXPECT_EQ(7u, stats.ctx_id);
                            stats.status = I915_RESET_STATS_BANNED;
                            return failPrelim ? -1 : 0;
                        }};
        ioctlHelper.setExternalContext(&ctx);
        ContextHealth health{};
        health.contextId = 7;

        EXPECT_EQ(failPrelim ? -1 : 0, ioctlHelper.getContextHealth(health));
        EXPECT_EQ(1u, callCount);
        EXPECT_EQ(!failPrelim, health.banned);
        ioctlHelper.setExternalContext(nullptr);
    }
}

HWTEST2_F(IoctlPrelimHelperTests, GivenIoctlHelperWhenCallingGetTileIdFromGtIdThenExpectedValueIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t gtId = 0;
    EXPECT_EQ(gtId, ioctlHelper.getTileIdFromGtId(gtId));
    gtId = 1;
    EXPECT_EQ(gtId, ioctlHelper.getTileIdFromGtId(gtId));
}

HWTEST2_F(IoctlPrelimHelperTests, GivenIoctlHelperWhenCallingGetGtIdFromTileIdThenExpectedValueIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t tileId = 0u;
    EXPECT_EQ(tileId, ioctlHelper.getGtIdFromTileId(tileId, I915_ENGINE_CLASS_RENDER));
    tileId = 1u;
    EXPECT_EQ(tileId, ioctlHelper.getGtIdFromTileId(tileId, I915_ENGINE_CLASS_VIDEO));
}

TEST(DrmTest, GivenDrmWhenAskedForPreemptionThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = 0;
    pDrm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    pDrm->checkPreemptionSupport();
    EXPECT_TRUE(pDrm->isPreemptionSupported());

    pDrm->storedPreemptionSupport = 0;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    pDrm->storedRetVal = -1;
    pDrm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    pDrm->storedPreemptionSupport = 0;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    delete pDrm;
}

TEST(DrmTest, givenDrmPreemptionEnabledAndLowPriorityEngineWhenCreatingOsContextThenCallSetContextPriorityIoctl) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.preemptionSupported = false;

    OsContextLinux osContext1(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext1.ensureContextInitialized();
    OsContextLinux osContext2(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority}));
    osContext2.ensureContextInitialized();

    EXPECT_EQ(4u, drmMock.receivedContextParamRequestCount);

    drmMock.preemptionSupported = true;

    OsContextLinux osContext3(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext3.ensureContextInitialized();
    EXPECT_EQ(6u, drmMock.receivedContextParamRequestCount);

    OsContextLinux osContext4(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority}));
    osContext4.ensureContextInitialized();
    EXPECT_EQ(9u, drmMock.receivedContextParamRequestCount);
    EXPECT_EQ(drmMock.storedDrmContextId, drmMock.receivedContextParamRequest.contextId);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_PRIORITY), drmMock.receivedContextParamRequest.param);
    EXPECT_EQ(static_cast<uint64_t>(-1023), drmMock.receivedContextParamRequest.value);
    EXPECT_EQ(0u, drmMock.receivedContextParamRequest.size);
}
