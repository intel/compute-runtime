/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/ioctl_strings.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

using namespace NEO;

extern std::map<unsigned long, const char *> ioctlCodeStringMap;
extern std::map<int, const char *> ioctlParamCodeStringMap;
extern std::vector<uint8_t> getRegionInfo(const std::vector<MemoryRegion> &inputRegions);
extern std::vector<uint8_t> getEngineInfo(const std::vector<EngineCapabilities> &inputEngines);

struct IoctlPrelimHelperTests : ::testing::Test {
    IoctlHelperPrelim20 ioctlHelper{};
};

TEST_F(IoctlPrelimHelperTests, givenIoctlWhenParseToStringThenProperStringIsReturned) {
    for (auto &ioctlCodeString : ioctlCodeStringMap) {
        EXPECT_STREQ(IoctlToStringHelper::getIoctlString(ioctlCodeString.first).c_str(), ioctlCodeString.second);
    }
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_VM_BIND).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_UUID_REGISTER).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_REGISTER");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN).c_str(), "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(DRM_IOCTL_I915_GEM_MMAP_GTT).c_str(), "DRM_IOCTL_I915_GEM_MMAP_GTT");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(DRM_IOCTL_I915_GEM_MMAP_OFFSET).c_str(), "DRM_IOCTL_I915_GEM_MMAP_OFFSET");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(DRM_IOCTL_I915_GEM_VM_CREATE).c_str(), "DRM_IOCTL_I915_GEM_VM_CREATE");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(DRM_IOCTL_I915_GEM_VM_DESTROY).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
    EXPECT_STREQ(IoctlToStringHelper::getIoctlString(DRM_IOCTL_I915_GEM_VM_DESTROY).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
}

TEST_F(IoctlPrelimHelperTests, givenIoctlParamWhenParseToStringThenProperStringIsReturned) {
    for (auto &ioctlParamCodeString : ioctlParamCodeStringMap) {
        EXPECT_STREQ(IoctlToStringHelper::getIoctlParamString(ioctlParamCodeString.first).c_str(), ioctlParamCodeString.second);
    }
    EXPECT_STREQ(IoctlToStringHelper::getIoctlParamString(PRELIM_I915_PARAM_HAS_VM_BIND).c_str(), "PRELIM_I915_PARAM_HAS_VM_BIND");
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenTranslateToMemoryRegionsThenReturnSameData) {
    std::vector<MemoryRegion> expectedMemRegions(2);
    expectedMemRegions[0].region.memoryClass = PRELIM_I915_MEMORY_CLASS_SYSTEM;
    expectedMemRegions[0].region.memoryInstance = 0;
    expectedMemRegions[0].probedSize = 1024;
    expectedMemRegions[1].region.memoryClass = PRELIM_I915_MEMORY_CLASS_DEVICE;
    expectedMemRegions[1].region.memoryInstance = 0;
    expectedMemRegions[1].probedSize = 1024;

    auto regionInfo = getRegionInfo(expectedMemRegions);

    auto memRegions = ioctlHelper.translateToMemoryRegions(regionInfo);
    EXPECT_EQ(2u, memRegions.size());
    for (uint32_t i = 0; i < memRegions.size(); i++) {
        EXPECT_EQ(expectedMemRegions[i].region.memoryClass, memRegions[i].region.memoryClass);
        EXPECT_EQ(expectedMemRegions[i].region.memoryInstance, memRegions[i].region.memoryInstance);
        EXPECT_EQ(expectedMemRegions[i].probedSize, memRegions[i].probedSize);
        EXPECT_EQ(expectedMemRegions[i].unallocatedSize, memRegions[i].unallocatedSize);
    }
}

TEST_F(IoctlPrelimHelperTests, givenEmptyRegionInstanceClassWhenCreatingVmControlRegionExtThenNullptrIsReturned) {
    std::optional<MemoryClassInstance> regionInstanceClass{};

    EXPECT_FALSE(regionInstanceClass.has_value());
    EXPECT_EQ(nullptr, ioctlHelper.createVmControlExtRegion(regionInstanceClass));
}

TEST_F(IoctlPrelimHelperTests, givenValidRegionInstanceClassWhenCreatingVmControlRegionExtThenProperStructIsReturned) {
    std::optional<MemoryClassInstance> regionInstanceClass = MemoryClassInstance{PRELIM_I915_MEMORY_CLASS_DEVICE, 2};

    EXPECT_TRUE(regionInstanceClass.has_value());

    auto retVal = ioctlHelper.createVmControlExtRegion(regionInstanceClass);

    EXPECT_NE(nullptr, retVal);

    auto regionExt = reinterpret_cast<prelim_drm_i915_gem_vm_region_ext *>(retVal.get());

    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_GEM_VM_CONTROL_EXT_REGION), regionExt->base.name);
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_MEMORY_CLASS_DEVICE), regionExt->region.memory_class);
    EXPECT_EQ(2u, regionExt->region.memory_instance);
}

TEST_F(IoctlPrelimHelperTests, whenGettingFlagsForVmCreateThenProperValueIsReturned) {
    for (auto &disableScratch : ::testing::Bool()) {
        for (auto &enablePageFault : ::testing::Bool()) {
            auto flags = ioctlHelper.getFlagsForVmCreate(disableScratch, enablePageFault);
            if (disableScratch) {
                EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH), (flags & PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH));
            }
            if (enablePageFault) {
                EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT), (flags & PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT));
            }
            if (disableScratch || enablePageFault) {
                EXPECT_NE(0u, flags);
            } else {
                EXPECT_EQ(0u, flags);
            }
        }
    }
}

TEST_F(IoctlPrelimHelperTests, whenGettingFlagsForVmBindThenProperValuesAreReturned) {
    for (auto &bindCapture : ::testing::Bool()) {
        for (auto &bindImmediate : ::testing::Bool()) {
            for (auto &bindMakeResident : ::testing::Bool()) {
                auto flags = ioctlHelper.getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident);
                if (bindCapture) {
                    EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_CAPTURE, (flags & PRELIM_I915_GEM_VM_BIND_CAPTURE));
                }
                if (bindImmediate) {
                    EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_IMMEDIATE, (flags & PRELIM_I915_GEM_VM_BIND_IMMEDIATE));
                }
                if (bindMakeResident) {
                    EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT, (flags & PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT));
                }
                if (flags == 0) {
                    EXPECT_FALSE(bindCapture);
                    EXPECT_FALSE(bindImmediate);
                    EXPECT_FALSE(bindMakeResident);
                }
            }
        }
    }
}

TEST_F(IoctlPrelimHelperTests, whenGettingVmBindExtFromHandlesThenProperStructsAreReturned) {
    StackVec<uint32_t, 2> bindExtHandles;
    bindExtHandles.push_back(1u);
    bindExtHandles.push_back(2u);
    bindExtHandles.push_back(3u);
    auto retVal = ioctlHelper.prepareVmBindExt(bindExtHandles);
    auto vmBindExt = reinterpret_cast<prelim_drm_i915_vm_bind_ext_uuid *>(retVal.get());

    for (size_t i = 0; i < bindExtHandles.size(); i++) {
        EXPECT_EQ(bindExtHandles[i], vmBindExt[i].uuid_handle);
        EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_UUID), vmBindExt[i].base.name);
    }

    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[1]), vmBindExt[0].base.next_extension);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[2]), vmBindExt[1].base.next_extension);
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetHwConfigIoctlValThenCorrectValueReturned) {
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE), ioctlHelper.getHwConfigIoctlVal());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetDirectSubmissionFlagThenCorrectValueReturned) {
    EXPECT_EQ(PRELIM_I915_CONTEXT_CREATE_FLAGS_ULLS, ioctlHelper.getDirectSubmissionFlag());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetMemRegionsIoctlValThenCorrectValueReturned) {
    EXPECT_EQ(PRELIM_DRM_I915_QUERY_MEMORY_REGIONS, ioctlHelper.getMemRegionsIoctlVal());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetEngineInfoIoctlValThenCorrectValueReturned) {
    EXPECT_EQ(PRELIM_DRM_I915_QUERY_ENGINE_INFO, ioctlHelper.getEngineInfoIoctlVal());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenTranslateToEngineCapsThenReturnSameData) {
    std::vector<EngineCapabilities> expectedEngines(2);
    expectedEngines[0] = {{I915_ENGINE_CLASS_RENDER, 0}, 0};
    expectedEngines[1] = {{I915_ENGINE_CLASS_COPY, 1}, 0};

    auto engineInfo = getEngineInfo(expectedEngines);

    auto engines = ioctlHelper.translateToEngineCaps(engineInfo);
    EXPECT_EQ(2u, engines.size());
    for (uint32_t i = 0; i < engines.size(); i++) {
        EXPECT_EQ(expectedEngines[i].engine.engineClass, engines[i].engine.engineClass);
        EXPECT_EQ(expectedEngines[i].engine.engineInstance, engines[i].engine.engineInstance);
        EXPECT_EQ(expectedEngines[i].capabilities, engines[i].capabilities);
    }
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGettingFlagForWaitUserFenceSoftThenProperFlagIsReturned) {
    EXPECT_EQ(PRELIM_I915_UFENCE_WAIT_SOFT, ioctlHelper.getWaitUserFenceSoftFlag());
}

TEST_F(IoctlPrelimHelperTests, whenCreateVmBindSetPatThenValidPointerIsReturned) {
    EXPECT_NE(nullptr, ioctlHelper.createVmBindExtSetPat());
}

TEST_F(IoctlPrelimHelperTests, whenCreateVmBindSyncFenceThenValidPointerIsReturned) {
    EXPECT_NE(nullptr, ioctlHelper.createVmBindExtSyncFence());
}

TEST_F(IoctlPrelimHelperTests, givenNullptrWhenFillVmBindSetPatThenUnrecoverableIsThrown) {
    std::unique_ptr<uint8_t[]> vmBindExtSetPat{};
    EXPECT_THROW(ioctlHelper.fillVmBindExtSetPat(vmBindExtSetPat, 0u, 0u), std::runtime_error);
}

TEST_F(IoctlPrelimHelperTests, givenNullptrWhenFillVmBindSyncFenceThenUnrecoverableIsThrown) {
    std::unique_ptr<uint8_t[]> vmBindExtSyncFence{};
    EXPECT_THROW(ioctlHelper.fillVmBindExtSyncFence(vmBindExtSyncFence, 0u, 0u, 0u), std::runtime_error);
}

TEST_F(IoctlPrelimHelperTests, givenValidInputWhenFillVmBindSetPatThenProperValuesAreSet) {
    std::unique_ptr<uint8_t[]> vmBindExtSetPat{};
    prelim_drm_i915_vm_bind_ext_set_pat prelimVmBindExtSetPat{};
    vmBindExtSetPat.reset(reinterpret_cast<uint8_t *>(&prelimVmBindExtSetPat));

    uint64_t expectedPatIndex = 2;
    uint64_t expectedNextExtension = 3;
    ioctlHelper.fillVmBindExtSetPat(vmBindExtSetPat, expectedPatIndex, expectedNextExtension);
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_SET_PAT), prelimVmBindExtSetPat.base.name);
    EXPECT_EQ(expectedPatIndex, prelimVmBindExtSetPat.pat_index);
    EXPECT_EQ(expectedNextExtension, prelimVmBindExtSetPat.base.next_extension);

    vmBindExtSetPat.release();
}

TEST_F(IoctlPrelimHelperTests, givenValidInputWhenFillVmBindSyncFenceThenProperValuesAreSet) {
    std::unique_ptr<uint8_t[]> vmBindExtSyncFence{};
    prelim_drm_i915_vm_bind_ext_sync_fence prelimVmBindExtSyncFence{};
    vmBindExtSyncFence.reset(reinterpret_cast<uint8_t *>(&prelimVmBindExtSyncFence));

    uint64_t expectedAddress = 0xdead;
    uint64_t expectedValue = 0xc0de;
    uint64_t expectedNextExtension = 1234;
    ioctlHelper.fillVmBindExtSyncFence(vmBindExtSyncFence, expectedAddress, expectedValue, expectedNextExtension);
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_SYNC_FENCE), prelimVmBindExtSyncFence.base.name);
    EXPECT_EQ(expectedAddress, prelimVmBindExtSyncFence.addr);
    EXPECT_EQ(expectedValue, prelimVmBindExtSyncFence.val);
    EXPECT_EQ(expectedNextExtension, prelimVmBindExtSyncFence.base.next_extension);

    vmBindExtSyncFence.release();
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenGettingEuStallPropertiesThenCorrectPropertiesAreReturned) {
    std::array<uint64_t, 10u> properties = {};
    EXPECT_TRUE(ioctlHelper.getEuStallProperties(properties, 0x101, 0x102, 0x103, 1));
    EXPECT_EQ(properties[0], PRELIM_DRM_I915_EU_STALL_PROP_BUF_SZ);
    EXPECT_EQ(properties[1], 0x101u);
    EXPECT_EQ(properties[2], PRELIM_DRM_I915_EU_STALL_PROP_SAMPLE_RATE);
    EXPECT_EQ(properties[3], 0x102u);
    EXPECT_EQ(properties[4], PRELIM_DRM_I915_EU_STALL_PROP_POLL_PERIOD);
    EXPECT_EQ(properties[5], 0x103u);
    EXPECT_EQ(properties[6], PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_CLASS);
    EXPECT_EQ(properties[7], PRELIM_I915_ENGINE_CLASS_COMPUTE);
    EXPECT_EQ(properties[8], PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_INSTANCE);
    EXPECT_EQ(properties[9], 1u);
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenGettingEuStallFdParameterThenCorrectIoctlValueIsReturned) {
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_PERF_FLAG_FD_EU_STALL), ioctlHelper.getEuStallFdParameter());
}
