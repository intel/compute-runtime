/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/utilities/directory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/os_interface/linux/xe/mock_drm_xe.h"
#include "shared/test/common/os_interface/linux/xe/mock_ioctl_helper_xe.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
};

static uint32_t ccsMode = 1u;

static ssize_t mockWrite(int fd, const void *buf, size_t count) {
    std::memcpy(&ccsMode, buf, count);
    return count;
}

struct DrmMockXeCcs : public DrmMockCustom {
    using Drm::engineInfo;

    void testMode(int f, int a = 0) {
        forceIoctlAnswer = f;
        setIoctlAnswer = a;
    }
    int ioctl(DrmIoctl request, void *arg) override {
        int ret = -1;
        ioctlCalled = true;
        if (forceIoctlAnswer) {
            return setIoctlAnswer;
        }
        switch (request) {
        case DrmIoctl::gemVmCreate: {
            struct drm_xe_vm_create *v = static_cast<struct drm_xe_vm_create *>(arg);
            v->vm_id = testValueVmId;
            ret = 0;
        } break;
        case DrmIoctl::gemUserptr: {
            ret = 0;
        } break;
        case DrmIoctl::gemClose: {
            auto gemClose = reinterpret_cast<GemClose *>(arg);
            passedGemClose = *gemClose;
            gemCloseCalled++;
            ret = 0;
        } break;
        case DrmIoctl::gemVmDestroy: {
            struct drm_xe_vm_destroy *v = static_cast<struct drm_xe_vm_destroy *>(arg);
            if (v->vm_id == testValueVmId)
                ret = 0;
        } break;
        case DrmIoctl::gemMmapOffset: {
            gemMmapOffsetCalled++;

            if (forceMmapOffsetFail) {
                ret = -1;
                break;
            }
            struct drm_xe_gem_mmap_offset *v = static_cast<struct drm_xe_gem_mmap_offset *>(arg);
            v->offset = v->handle;
            ret = 0;

        } break;
        case DrmIoctl::primeFdToHandle: {
            PrimeHandle *v = static_cast<PrimeHandle *>(arg);
            if (v->fileDescriptor == testValuePrime) {
                v->handle = testValuePrime;
                ret = 0;
            }
        } break;
        case DrmIoctl::primeHandleToFd: {
            PrimeHandle *v = static_cast<PrimeHandle *>(arg);
            if (v->handle == testValuePrime) {
                v->fileDescriptor = testValuePrime;
                ret = 0;
            }
        } break;
        case DrmIoctl::gemCreate: {
            ioctlCnt.gemCreate++;
            auto createParams = static_cast<drm_xe_gem_create *>(arg);
            this->createParamsSize = createParams->size;
            this->createParamsPlacement = createParams->placement;
            this->createParamsFlags = createParams->flags;
            this->createParamsHandle = createParams->handle = testValueGemCreate;
            this->createParamsCpuCaching = createParams->cpu_caching;
            if (0 == this->createParamsSize || 0 == this->createParamsPlacement || 0 == this->createParamsCpuCaching) {
                return EINVAL;
            }
            ret = 0;
        } break;
        case DrmIoctl::getparam:
            ret = -2;
            break;
        case DrmIoctl::getResetStats: {
            auto execQueueProperty = static_cast<drm_xe_exec_queue_get_property *>(arg);
            EXPECT_EQ(execQueueProperty->property, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_GET_PROPERTY_BAN));
            execQueueProperty->value = execQueueBanPropertyReturn;
            ret = 0;
        } break;
        case DrmIoctl::query: {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            switch (deviceQuery->query) {
            case DRM_XE_DEVICE_QUERY_CONFIG:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryConfig, sizeof(queryConfig));
                }
                deviceQuery->size = sizeof(queryConfig);
                break;
            case DRM_XE_DEVICE_QUERY_ENGINES:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryEngines, sizeof(queryEngines));
                }
                deviceQuery->size = sizeof(queryEngines);
                break;
            case DRM_XE_DEVICE_QUERY_MEM_REGIONS:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryMemUsage, sizeof(queryMemUsage));
                }
                deviceQuery->size = sizeof(queryMemUsage);
                break;
            case DRM_XE_DEVICE_QUERY_GT_LIST:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryGtList.begin(), sizeof(queryGtList[0]) * queryGtList.size());
                }
                deviceQuery->size = static_cast<uint32_t>(sizeof(queryGtList[0]) * queryGtList.size());
                break;
            case DRM_XE_DEVICE_QUERY_GT_TOPOLOGY:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryTopology.data(), queryTopology.size());
                }
                deviceQuery->size = static_cast<unsigned int>(queryTopology.size());
                break;
            case DRM_XE_DEVICE_QUERY_ENGINE_CYCLES:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryEngineCycles, sizeof(queryEngineCycles));
                }
                deviceQuery->size = sizeof(queryEngineCycles);
                break;
            };
            ret = 0;
        } break;
        case DrmIoctl::gemVmBind: {
            ret = gemVmBindReturn;
            auto vmBindInput = static_cast<drm_xe_vm_bind *>(arg);
            vmBindInputs.push_back(*vmBindInput);

            if (vmBindInput->num_syncs == 1) {
                auto &syncInput = reinterpret_cast<drm_xe_sync *>(vmBindInput->syncs)[0];
                syncInputs.push_back(syncInput);
            }
        } break;

        case DrmIoctl::gemWaitUserFence: {
            ret = waitUserFenceReturn;
            auto waitUserFenceInput = static_cast<drm_xe_wait_user_fence *>(arg);
            waitUserFenceInputs.push_back(*waitUserFenceInput);
            handleUserFenceWaitExtensions(waitUserFenceInput);
        } break;

        case DrmIoctl::gemContextCreateExt: {
            auto queueCreate = static_cast<drm_xe_exec_queue_create *>(arg);
            latestExecQueueCreate = *queueCreate;
            latestQueueEngineClassInstances.clear();

            auto instances = reinterpret_cast<drm_xe_engine_class_instance *>(queueCreate->instances);

            for (uint16_t i = 0; i < queueCreate->num_placements; i++) {
                latestQueueEngineClassInstances.push_back(instances[i]);
            }

            auto extension = queueCreate->extensions;
            while (extension) {
                auto ext = reinterpret_cast<drm_xe_user_extension *>(extension);
                if (ext->name == DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY) {
                    auto setProperty = reinterpret_cast<drm_xe_ext_set_property *>(ext);
                    execQueueProperties.push_back(*setProperty);
                }
                handleContextCreateExtensions(ext);
                extension = ext->next_extension;
            }
            queueCreate->exec_queue_id = ++mockExecQueueId;
            ret = 0;
        } break;
        case DrmIoctl::gemContextDestroy: {
            gemDestroyContextCalled++;
            ret = 0;
        } break;
        case DrmIoctl::perfOpen: {
            ret = 0;
        } break;
        case DrmIoctl::gemContextSetparam:
        case DrmIoctl::gemContextGetparam:

        default:
            break;
        }
        return ret;
    }
    virtual void handleUserFenceWaitExtensions(drm_xe_wait_user_fence *userFenceWait) {}
    virtual void handleContextCreateExtensions(drm_xe_user_extension *extension) {}

    void addMockedQueryTopologyData(uint16_t gtId, uint16_t maskType, uint32_t nBytes, const std::vector<uint8_t> &mask) {
        ASSERT_EQ(nBytes, mask.size());

        auto additionalSize = 8u + nBytes;
        auto oldSize = queryTopology.size();
        auto newSize = oldSize + additionalSize;
        queryTopology.resize(newSize, 0u);

        uint8_t *dataPtr = queryTopology.data() + oldSize;

        drm_xe_query_topology_mask *topo = reinterpret_cast<drm_xe_query_topology_mask *>(dataPtr);
        topo->gt_id = gtId;
        topo->type = maskType;
        topo->num_bytes = nBytes;

        memcpy_s(reinterpret_cast<void *>(topo->mask), nBytes, mask.data(), nBytes);
    }

    int forceIoctlAnswer = 0;
    int setIoctlAnswer = 0;
    int gemVmBindReturn = 0;
    GemClose passedGemClose{};
    int gemCloseCalled = 0;
    int gemMmapOffsetCalled = 0;
    int gemDestroyContextCalled = 0;

    uint64_t queryConfig[7]{}; // 1 qword for num params and 1 qwords per param
    uint32_t mockExecQueueId = 1234;
    static constexpr int32_t mockMaxExecQueuePriority = 3;
    static constexpr int32_t mockDefaultCxlType = 0;
    static constexpr uint32_t mockTimestampFrequency = 12500000;
    static_assert(sizeof(drm_xe_engine) == 4 * sizeof(uint64_t), "");
    uint64_t queryEngines[52]{}; // 1 qword for num engines and 4 qwords per engine
    static_assert(sizeof(drm_xe_mem_region) == 11 * sizeof(uint64_t), "");
    uint64_t queryMemUsage[34]{}; // 1 qword for num regions and 11 qwords per region
    static_assert(sizeof(drm_xe_gt) == 12 * sizeof(uint64_t), "");
    StackVec<uint64_t, 49> queryGtList{}; // 1 qword for num gts and 12 qwords per gt
    alignas(64) std::vector<uint8_t> queryTopology;
    static_assert(sizeof(drm_xe_query_engine_cycles) == 5 * sizeof(uint64_t), "");
    uint64_t queryEngineCycles[5]{}; // 1 qword for eci and 4 qwords
    StackVec<drm_xe_wait_user_fence, 1> waitUserFenceInputs;
    StackVec<drm_xe_vm_bind, 1> vmBindInputs;
    StackVec<drm_xe_sync, 1> syncInputs;
    StackVec<drm_xe_ext_set_property, 1> execQueueProperties;
    drm_xe_exec_queue_create latestExecQueueCreate = {};
    std::vector<drm_xe_engine_class_instance> latestQueueEngineClassInstances;

    int waitUserFenceReturn = 0;
    int execQueueBanPropertyReturn = 0;
    uint32_t createParamsFlags = 0u;
    uint16_t createParamsCpuCaching = 0u;
    uint32_t createParamsPlacement = 0u;
    bool ioctlCalled = false;
    bool forceMmapOffsetFail = false;

    DrmMockXeCcs(RootDeviceEnvironment &rootDeviceEnvironment)
        : DrmMockCustom(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {}

    virtual void initInstance() {
        this->reset();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
        this->ioctlExpected.contextCreate = static_cast<int>(gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment).size());
        this->ioctlExpected.contextDestroy = this->ioctlExpected.contextCreate.load();

        this->ioctlHelper = std::make_unique<MockIoctlHelperXe>(*this);

        this->createVirtualMemoryAddressSpace(NEO::GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()));
        this->isVmBindAvailable();

        auto xeQueryConfig = reinterpret_cast<drm_xe_query_config *>(this->queryConfig);
        xeQueryConfig->num_params = 6;
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] = 0; // this should be queried by ioctl sys call
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_VA_BITS] = 48;
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY] = mockMaxExecQueuePriority;

        constexpr drm_xe_gt tile0MainGt = {
            .type = DRM_XE_QUERY_GT_TYPE_MAIN,
            .tile_id = 0,
            .gt_id = 0,
            .pad = {0},
            .reference_clock = mockTimestampFrequency,
            .near_mem_regions = 0b100,
            .far_mem_regions = 0x011,
        };

        constexpr drm_xe_gt tile1MediaGt = {
            .type = DRM_XE_QUERY_GT_TYPE_MEDIA,
            .tile_id = 1,
            .gt_id = 1,
            .pad = {0},
            .reference_clock = mockTimestampFrequency,
            .near_mem_regions = 0b001,
            .far_mem_regions = 0x110,
        };

        constexpr drm_xe_gt tile1MainGt = {
            .type = DRM_XE_QUERY_GT_TYPE_MAIN,
            .tile_id = 1,
            .gt_id = 2,
            .pad = {0},
            .reference_clock = mockTimestampFrequency,
            .near_mem_regions = 0b010,
            .far_mem_regions = 0x101,
        };

        constexpr drm_xe_gt tile2MainGt = {
            .type = DRM_XE_QUERY_GT_TYPE_MAIN,
            .tile_id = 2,
            .gt_id = 3,
            .pad = {0},
            .reference_clock = mockTimestampFrequency,
            .near_mem_regions = 0b100,
            .far_mem_regions = 0x011,
        };

        auto xeQueryEngines = reinterpret_cast<drm_xe_query_engines *>(this->queryEngines);
        xeQueryEngines->num_engines = 11;
        xeQueryEngines->engines[0] = {{DRM_XE_ENGINE_CLASS_RENDER, 0, tile0MainGt.gt_id}, {}};
        xeQueryEngines->engines[1] = {{DRM_XE_ENGINE_CLASS_COPY, 1, tile0MainGt.gt_id}, {}};
        xeQueryEngines->engines[2] = {{DRM_XE_ENGINE_CLASS_COPY, 2, tile0MainGt.gt_id}, {}};
        xeQueryEngines->engines[3] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 3, tile0MainGt.gt_id}, {}};
        xeQueryEngines->engines[4] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 4, tile0MainGt.gt_id}, {}};
        xeQueryEngines->engines[5] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 5, tile1MainGt.gt_id}, {}};
        xeQueryEngines->engines[6] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 6, tile1MainGt.gt_id}, {}};
        xeQueryEngines->engines[7] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 7, tile1MainGt.gt_id}, {}};
        xeQueryEngines->engines[8] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 8, tile1MainGt.gt_id}, {}};
        xeQueryEngines->engines[9] = {{DRM_XE_ENGINE_CLASS_VIDEO_DECODE, 9, tile1MainGt.gt_id}, {}};
        xeQueryEngines->engines[10] = {{DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, 10, tile0MainGt.gt_id}, {}};

        auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(this->queryMemUsage);
        xeQueryMemUsage->num_mem_regions = 3;
        xeQueryMemUsage->mem_regions[0] = {
            DRM_XE_MEM_REGION_CLASS_VRAM,  // class
            1,                             // instance
            MemoryConstants::pageSize,     // min page size
            2 * MemoryConstants::gigaByte, // total size
            MemoryConstants::megaByte      // used size
        };
        xeQueryMemUsage->mem_regions[1] = {
            DRM_XE_MEM_REGION_CLASS_SYSMEM, // class
            0,                              // instance
            MemoryConstants::pageSize,      // min page size
            MemoryConstants::gigaByte,      // total size
            MemoryConstants::kiloByte       // used size
        };
        xeQueryMemUsage->mem_regions[2] = {
            DRM_XE_MEM_REGION_CLASS_VRAM,  // class
            2,                             // instance
            MemoryConstants::pageSize,     // min page size
            4 * MemoryConstants::gigaByte, // total size
            MemoryConstants::gigaByte      // used size
        };

        this->queryGtList.resize(1 + (6 * 12)); // 1 qword for num gts and 12 qwords per gt
        auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(this->queryGtList.begin());
        xeQueryGtList->num_gt = 4;
        xeQueryGtList->gt_list[0] = tile0MainGt;
        xeQueryGtList->gt_list[1] = tile1MediaGt;
        xeQueryGtList->gt_list[2] = tile1MainGt;
        xeQueryGtList->gt_list[3] = tile2MainGt;
        this->reset();
    }
};

class CcsModeXeFixture {
  public:
    void setUp() {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());

        drm = new DrmMockXeCcs(*executionEnvironment->rootDeviceEnvironments[0]);
        drm->initInstance();

        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
        osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    }

    void tearDown() {
    }

    DrmMockXeCcs *drm;
    OSInterface *osInterface;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

using CcsModeXeTest = Test<CcsModeXeFixture>;

TEST_F(CcsModeXeTest, GivenXeDrmAndInvalidCcsModeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    // On Xe, path is /sys/class/drm/card0/device/tile*/gt*/ccs_mode
    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("unknown");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    DebugManagerStateRestore restorer;
    executionEnvironment->configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("abc");
    executionEnvironment->configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("");
    executionEnvironment->configureCcsMode();
    EXPECT_EQ(1u, ccsMode);

    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndValidCcsModeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsProperlyConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    // On Xe, path is /sys/class/drm/card0/device/tile*/gt*/ccs_mode
    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");
    directoryFilesMap["/sys/class/drm"].push_back("version");

    directoryFilesMap["/sys/class/drm/card0/device/tile"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile"].push_back("/sys/class/drm/card0/device/tile0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    EXPECT_EQ(2u, ccsMode);
    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndValidCcsModeAndOpenSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {"/sys/class/drm/card0"};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {"/sys/class/drm/card0/device/tile0/gt0"};

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = -ENOENT;
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndValidCcsModeAndOpenSysCallFailsWithNoPermissionsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");

    directoryFilesMap["/sys/class/drm/card0/device/tile"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile"].push_back("/sys/class/drm/card0/device/tile0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = -EPERM;
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    DebugManagerStateRestore restorer;

    StreamCapture capture;
    capture.captureStdout();
    testing::internal::CaptureStderr();

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    std::string stdOutString = capture.getCapturedStdout();
    std::string stdErrString = testing::internal::GetCapturedStderr();
    std::string expectedOutput = "No read and write permissions for /sys/class/drm/card0/device/tile0/gt0/ccs_mode, System administrator needs to grant permissions to allow modification of this file from user space\n";

    EXPECT_EQ(1u, ccsMode);
    EXPECT_STREQ(stdOutString.c_str(), expectedOutput.c_str());
    EXPECT_STREQ(stdErrString.c_str(), expectedOutput.c_str());
    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndNumCCSFlagSetToCurrentConfigurationWhenConfigureCcsModeIsCalledThenVerifyWriteCallIsNotInvoked) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");

    directoryFilesMap["/sys/class/drm/card0/device/tile"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile"].push_back("/sys/class/drm/card0/device/tile0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    SysCalls::writeFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        SysCalls::writeFuncCalled++;
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("1");
    executionEnvironment->configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    EXPECT_EQ(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndValidCcsModeAndOpenSysCallFailsWithNoAccessWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");

    directoryFilesMap["/sys/class/drm/card0/device/tile"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile"].push_back("/sys/class/drm/card0/device/tile0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = -EACCES;
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    DebugManagerStateRestore restorer;

    StreamCapture capture;
    capture.captureStdout();
    testing::internal::CaptureStderr();

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    std::string stdOutString = capture.getCapturedStdout();
    std::string stdErrString = testing::internal::GetCapturedStderr();
    std::string expectedOutput = "No read and write permissions for /sys/class/drm/card0/device/tile0/gt0/ccs_mode, System administrator needs to grant permissions to allow modification of this file from user space\n";

    EXPECT_EQ(1u, ccsMode);
    EXPECT_STREQ(stdOutString.c_str(), expectedOutput.c_str());
    EXPECT_STREQ(stdErrString.c_str(), expectedOutput.c_str());
    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndNumCCSFlagSetToCurrentConfigurationAndReadSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");

    directoryFilesMap["/sys/class/drm/card0/device/tile"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile"].push_back("/sys/class/drm/card0/device/tile0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    SysCalls::writeFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        SysCalls::writeFuncCalled++;
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        return -1;
    });

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    EXPECT_EQ(1u, ccsMode);
    EXPECT_EQ(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndValidCcsModeAndWriteSysCallFailsWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");

    directoryFilesMap["/sys/class/drm/card0/device/tile"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile"].push_back("/sys/class/drm/card0/device/tile0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    SysCalls::writeFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        SysCalls::writeFuncCalled++;
        errno = -EAGAIN;
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
    executionEnvironment->configureCcsMode();

    EXPECT_NE(2u, ccsMode);
    EXPECT_NE(0u, SysCalls::writeFuncCalled);
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndWriteSysCallFailsWithEbusyForFirstTimeWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsConfiguredInSecondIteration) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");

    directoryFilesMap["/sys/class/drm/card0/device/tile"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile"].push_back("/sys/class/drm/card0/device/tile0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    SysCalls::writeFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        if (SysCalls::writeFuncCalled++ == 0u) {
            errno = -EBUSY;
            return -1;
        }
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    {
        DebugManagerStateRestore restorer;
        debugManager.flags.ZEX_NUMBER_OF_CCS.set("4");
        executionEnvironment->configureCcsMode();

        EXPECT_EQ(4u, ccsMode);
        EXPECT_NE(0u, SysCalls::writeFuncCalled);
    }
    SysCalls::writeFuncCalled = 0u;
    directoryFilesMap.clear();
}

TEST_F(CcsModeXeTest, GivenXeDrmAndValidCcsModeAndOpenCallFailsOnRestoreWhenConfigureCcsModeIsCalledThenVerifyCcsModeIsNotRestored) {
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);

    // On Xe, path is /sys/class/drm/card0/device/tile*/gt*/ccs_mode
    directoryFilesMap["/sys/class/drm"] = {};
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card0");
    directoryFilesMap["/sys/class/drm"].push_back("/sys/class/drm/card1");

    directoryFilesMap["/sys/class/drm/card0/device/tile"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile"].push_back("/sys/class/drm/card0/device/tile0");

    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"] = {};
    directoryFilesMap["/sys/class/drm/card0/device/tile0/gt"].push_back("/sys/class/drm/card0/device/tile0/gt0");

    SysCalls::openFuncCalled = 0u;
    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (SysCalls::openFuncCalled == 1u) {
            return 1;
        }
        return -1;
    });

    VariableBackup<decltype(SysCalls::sysCallsWrite)> writeBackup(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return mockWrite(fd, buf, count);
    });

    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int fd, void *buf, size_t count) -> ssize_t {
        memcpy(buf, &ccsMode, count);
        return count;
    });

    {
        DebugManagerStateRestore restorer;
        debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");
        executionEnvironment->configureCcsMode();

        EXPECT_EQ(2u, ccsMode);
    }

    EXPECT_NE(1u, ccsMode);
    directoryFilesMap.clear();
}