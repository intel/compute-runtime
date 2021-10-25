/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/memory_manager/definitions/engine_limits.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/api_intercept.h"
#include "shared/source/utilities/stackvec.h"

#include "drm/i915_drm.h"
#include "engine_node.h"
#include "igfxfmid.h"

#include <array>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

struct GT_SYSTEM_INFO;

namespace NEO {
#define I915_CONTEXT_PRIVATE_PARAM_BOOST 0x80000000

class BufferObject;
class DeviceFactory;
class OsContext;
struct HardwareInfo;
struct RootDeviceEnvironment;
struct SystemInfo;

struct DeviceDescriptor { // NOLINT(clang-analyzer-optin.performance.Padding)
    unsigned short deviceId;
    const HardwareInfo *pHwInfo;
    void (*setupHardwareInfo)(HardwareInfo *, bool);
    GTTYPE eGtType;
    const char *devName;
};

extern const DeviceDescriptor deviceDescriptorTable[];

namespace IoctlHelper {
std::string getIoctlParamString(int param);
std::string getIoctlParamStringRemaining(int param);
std::string getIoctlString(unsigned long request);
std::string getIoctlStringRemaining(unsigned long request);
} // namespace IoctlHelper

class Drm : public DriverModel {
    friend DeviceFactory;

  public:
    static constexpr DriverModelType driverModelType = DriverModelType::DRM;

    enum class ResourceClass : uint32_t {
        Elf,
        Isa,
        ModuleHeapDebugArea,
        ContextSaveArea,
        SbaTrackingBuffer,
        MaxSize
    };

    struct QueryTopologyData {
        int sliceCount;
        int subSliceCount;
        int euCount;

        int maxSliceCount;
        int maxSubSliceCount;
        int maxEuCount;
    };

    virtual ~Drm();

    virtual int ioctl(unsigned long request, void *arg);

    int getDeviceID(int &devId);
    unsigned int getDeviceHandle() const override {
        return 0;
    }
    int getDeviceRevID(int &revId);
    int getExecSoftPin(int &execSoftPin);
    int enableTurboBoost();
    int getEuTotal(int &euTotal);
    int getSubsliceTotal(int &subsliceTotal);

    int getMaxGpuFrequency(HardwareInfo &hwInfo, int &maxGpuFrequency);
    int getEnabledPooledEu(int &enabled);
    int getMinEuInPool(int &minEUinPool);
    int getTimestampFrequency(int &frequency);

    int queryGttSize(uint64_t &gttSizeOutput);
    bool isPreemptionSupported() const { return preemptionSupported; }

    MOCKABLE_VIRTUAL void checkPreemptionSupport();
    inline int getFileDescriptor() const { return hwDeviceId->getFileDescriptor(); }
    ADAPTER_BDF getAdapterBDF() const {
        return adapterBDF;
    }
    int queryAdapterBDF();
    int createDrmVirtualMemory(uint32_t &drmVmId);
    void destroyDrmVirtualMemory(uint32_t drmVmId);
    uint32_t createDrmContext(uint32_t drmVmId, bool isSpecialContextRequested);
    void appendDrmContextFlags(drm_i915_gem_context_create_ext &gcc, bool isSpecialContextRequested);
    void destroyDrmContext(uint32_t drmContextId);
    int queryVmId(uint32_t drmContextId, uint32_t &vmId);
    void setLowPriorityContextParam(uint32_t drmContextId);

    unsigned int bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType, bool engineInstancedDevice);

    void setGtType(GTTYPE eGtType) { this->eGtType = eGtType; }
    GTTYPE getGtType() const { return this->eGtType; }
    MOCKABLE_VIRTUAL int getErrno();
    bool setQueueSliceCount(uint64_t sliceCount);
    void checkQueueSliceSupport();
    uint64_t getSliceMask(uint64_t sliceCount);
    MOCKABLE_VIRTUAL bool querySystemInfo();
    MOCKABLE_VIRTUAL bool queryEngineInfo();
    MOCKABLE_VIRTUAL bool sysmanQueryEngineInfo();
    MOCKABLE_VIRTUAL bool queryEngineInfo(bool isSysmanEnabled);
    MOCKABLE_VIRTUAL bool queryMemoryInfo();
    bool queryTopology(const HardwareInfo &hwInfo, QueryTopologyData &data);
    bool createVirtualMemoryAddressSpace(uint32_t vmCount);
    void destroyVirtualMemoryAddressSpace();
    uint32_t getVirtualMemoryAddressSpace(uint32_t vmId);
    int bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo);
    int unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo);
    int setupHardwareInfo(DeviceDescriptor *, bool);
    void setupSystemInfo(HardwareInfo *hwInfo, SystemInfo *sysInfo);
    void setupCacheInfo(const HardwareInfo &hwInfo);

    PhysicalDevicePciBusInfo getPciBusInfo() const override;

    bool areNonPersistentContextsSupported() const { return nonPersistentContextsSupported; }
    void checkNonPersistentContextsSupport();
    void setNonPersistentContext(uint32_t drmContextId);
    bool isPerContextVMRequired() {
        return requirePerContextVM;
    }
    void setPerContextVMRequired(bool required) {
        requirePerContextVM = required;
    }

    void checkContextDebugSupport();
    bool isContextDebugSupported() { return contextDebugSupported; }
    MOCKABLE_VIRTUAL void setContextDebugFlag(uint32_t drmContextId);

    void setUnrecoverableContext(uint32_t drmContextId);

    void setDirectSubmissionActive(bool value) { this->directSubmissionActive = value; }
    bool isDirectSubmissionActive() { return this->directSubmissionActive; }

    MOCKABLE_VIRTUAL bool isVmBindAvailable();
    MOCKABLE_VIRTUAL bool registerResourceClasses();

    MOCKABLE_VIRTUAL uint32_t registerResource(ResourceClass classType, const void *data, size_t size);
    MOCKABLE_VIRTUAL void unregisterResource(uint32_t handle);
    MOCKABLE_VIRTUAL uint32_t registerIsaCookie(uint32_t isaHandle);

    MOCKABLE_VIRTUAL bool isDebugAttachAvailable();

    void setGmmInputArgs(void *args) override;

    SystemInfo *getSystemInfo() const {
        return systemInfo.get();
    }

    CacheInfo *getCacheInfo() const {
        return cacheInfo.get();
    }

    MemoryInfo *getMemoryInfo() const {
        return memoryInfo.get();
    }

    EngineInfo *getEngineInfo() const {
        return engineInfo.get();
    }

    RootDeviceEnvironment &getRootDeviceEnvironment() {
        return rootDeviceEnvironment;
    }

    bool resourceRegistrationEnabled() {
        return classHandles.size() > 0;
    }

    static bool isi915Version(int fd);

    static Drm *create(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);
    static void overrideBindSupport(bool &useVmBind);
    std::string getPciPath() {
        return hwDeviceId->getPciPath();
    }

    void waitForBind(uint32_t vmHandleId);
    uint64_t getNextFenceVal(uint32_t vmHandleId) { return ++fenceVal[vmHandleId]; }
    uint64_t *getFenceAddr(uint32_t vmHandleId) { return &pagingFence[vmHandleId]; }

    int waitHandle(uint32_t waitHandle, int64_t timeout);
    enum class ValueWidth : uint32_t {
        U8,
        U16,
        U32,
        U64
    };
    MOCKABLE_VIRTUAL int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags);

    void setNewResourceBoundToVM(uint32_t vmHandleId);

    const std::vector<int> &getSliceMappings(uint32_t deviceIndex);
    const TopologyMap &getTopologyMap();

    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevices(ExecutionEnvironment &executionEnvironment);

    std::unique_lock<std::mutex> lockBindFenceMutex();

    void setPciDomain(uint32_t domain) {
        pciDomain = domain;
    }

    uint32_t getPciDomain() {
        return pciDomain;
    }

  protected:
    Drm(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment);

    uint32_t createDrmContextExt(drm_i915_gem_context_create_ext &gcc, uint32_t drmVmId, bool isSpecialContextRequested);
    int getQueueSliceCount(drm_i915_gem_context_param_sseu *sseu);
    bool translateTopologyInfo(const drm_i915_query_topology_info *queryTopologyInfo, QueryTopologyData &data, TopologyMapping &mapping);
    std::string generateUUID();
    std::string generateElfUUID(const void *data);
    std::string getSysFsPciPath();
    std::unique_ptr<uint8_t[]> query(uint32_t queryId, uint32_t queryItemFlags, int32_t &length);
    void printIoctlStatistics();

#pragma pack(1)
    struct PCIConfig {
        uint16_t VendorID;
        uint16_t DeviceID;
        uint16_t Command;
        uint16_t Status;
        uint8_t Revision;
        uint8_t ProgIF;
        uint8_t Subclass;
        uint8_t ClassCode;
        uint8_t cacheLineSize;
        uint8_t LatencyTimer;
        uint8_t HeaderType;
        uint8_t BIST;
        uint32_t BAR0[6];
        uint32_t CardbusCISPointer;
        uint16_t SubsystemVendorID;
        uint16_t SubsystemDeviceID;
        uint32_t ROM;
        uint8_t Capabilities;
        uint8_t Reserved[7];
        uint8_t InterruptLine;
        uint8_t InterruptPIN;
        uint8_t MinGrant;
        uint8_t MaxLatency;
    };
#pragma pack()

    drm_i915_gem_context_param_sseu sseu{};
    ADAPTER_BDF adapterBDF{};
    uint32_t pciDomain = 0;

    TopologyMap topologyMap;
    struct IoctlStatisticsEntry {
        long long totalTime = 0;
        uint64_t count = 0;
        long long minTime = std::numeric_limits<long long>::max();
        long long maxTime = 0;
    };
    std::unordered_map<unsigned long, IoctlStatisticsEntry> ioctlStatistics;

    std::mutex bindFenceMutex;
    std::array<uint64_t, EngineLimits::maxHandleCount> pagingFence;
    std::array<uint64_t, EngineLimits::maxHandleCount> fenceVal;
    StackVec<uint32_t, size_t(ResourceClass::MaxSize)> classHandles;
    std::vector<uint32_t> virtualMemoryIds;

    std::unique_ptr<HwDeviceIdDrm> hwDeviceId;
    std::unique_ptr<SystemInfo> systemInfo;
    std::unique_ptr<CacheInfo> cacheInfo;
    std::unique_ptr<EngineInfo> engineInfo;
    std::unique_ptr<MemoryInfo> memoryInfo;

    std::once_flag checkBindOnce;

    RootDeviceEnvironment &rootDeviceEnvironment;
    uint64_t uuid = 0;

    int deviceId = 0;
    int revisionId = 0;
    GTTYPE eGtType = GTTYPE_UNDEFINED;

    bool sliceCountChangeSupported = false;
    bool preemptionSupported = false;
    bool nonPersistentContextsSupported = false;
    bool requirePerContextVM = false;
    bool bindAvailable = false;
    bool directSubmissionActive = false;
    bool contextDebugSupported = false;

  private:
    int getParamIoctl(int param, int *dstValue);
};
} // namespace NEO
