/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/utilities/api_intercept.h"
#include "shared/source/utilities/stackvec.h"

#include "drm/i915_drm.h"
#include "engine_limits.h"
#include "engine_node.h"
#include "igfxfmid.h"

#include <array>
#include <cerrno>
#include <fcntl.h>
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

class Drm {
    friend DeviceFactory;

  public:
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
    int getDeviceRevID(int &revId);
    int getExecSoftPin(int &execSoftPin);
    int enableTurboBoost();
    int getEuTotal(int &euTotal);
    int getSubsliceTotal(int &subsliceTotal);

    int getMaxGpuFrequency(HardwareInfo &hwInfo, int &maxGpuFrequency);
    int getEnabledPooledEu(int &enabled);
    int getMinEuInPool(int &minEUinPool);

    int queryGttSize(uint64_t &gttSizeOutput);
    bool isPreemptionSupported() const { return preemptionSupported; }

    MOCKABLE_VIRTUAL void checkPreemptionSupport();
    inline int getFileDescriptor() const { return hwDeviceId->getFileDescriptor(); }
    ADAPTER_BDF getAdapterBDF() const;
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
    void setupSystemInfo(HardwareInfo *hwInfo, SystemInfo &sysInfo);
    void setupCacheInfo(const HardwareInfo &hwInfo);

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

    void setDirectSubmissionActive(bool value) { this->directSubmissionActive = value; }
    bool isDirectSubmissionActive() { return this->directSubmissionActive; }

    MOCKABLE_VIRTUAL bool isVmBindAvailable();
    MOCKABLE_VIRTUAL bool registerResourceClasses();

    MOCKABLE_VIRTUAL uint32_t registerResource(ResourceClass classType, const void *data, size_t size);
    MOCKABLE_VIRTUAL void unregisterResource(uint32_t handle);
    MOCKABLE_VIRTUAL uint32_t registerIsaCookie(uint32_t isaHandle);

    MOCKABLE_VIRTUAL bool isDebugAttachAvailable();

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

    static Drm *create(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);
    static void overrideBindSupport(bool &useVmBind);
    std::string getPciPath() {
        return hwDeviceId->getPciPath();
    }

    void waitForBind(uint32_t vmHandleId);
    uint64_t getNextFenceVal(uint32_t vmHandleId) { return ++fenceVal[vmHandleId]; }
    uint64_t *getFenceAddr(uint32_t vmHandleId) { return &pagingFence[vmHandleId]; }

    void setNewResourceBound(bool value) { this->newResourceBound = value; };
    bool getNewResourceBound() { return this->newResourceBound; };

    const std::vector<int> &getSliceMappings(uint32_t deviceIndex);

  protected:
    struct TopologyMapping {
        std::vector<int> sliceIndices;
    };
    int getQueueSliceCount(drm_i915_gem_context_param_sseu *sseu);
    bool translateTopologyInfo(const drm_i915_query_topology_info *queryTopologyInfo, QueryTopologyData &data, TopologyMapping &mapping);
    std::string generateUUID();
    std::string generateElfUUID(const void *data);
    bool sliceCountChangeSupported = false;
    drm_i915_gem_context_param_sseu sseu{};
    bool preemptionSupported = false;
    bool nonPersistentContextsSupported = false;
    bool requirePerContextVM = false;
    bool bindAvailable = false;
    bool directSubmissionActive = false;
    bool contextDebugSupported = false;
    bool newResourceBound = false;
    std::once_flag checkBindOnce;
    std::unique_ptr<HwDeviceId> hwDeviceId;
    int deviceId = 0;
    int revisionId = 0;
    GTTYPE eGtType = GTTYPE_UNDEFINED;
    RootDeviceEnvironment &rootDeviceEnvironment;
    uint64_t uuid = 0;

    Drm(std::unique_ptr<HwDeviceId> hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment);
    std::unique_ptr<SystemInfo> systemInfo;
    std::unique_ptr<CacheInfo> cacheInfo;
    std::unique_ptr<EngineInfo> engineInfo;
    std::unique_ptr<MemoryInfo> memoryInfo;
    std::vector<uint32_t> virtualMemoryIds;

    std::array<uint64_t, EngineLimits::maxHandleCount> pagingFence;
    std::array<uint64_t, EngineLimits::maxHandleCount> fenceVal;

    std::unordered_map<uint32_t, TopologyMapping> topologyMap;

    std::string getSysFsPciPath();
    std::unique_ptr<uint8_t[]> query(uint32_t queryId, uint32_t queryItemFlags, int32_t &length);

    StackVec<uint32_t, size_t(ResourceClass::MaxSize)> classHandles;

    std::unordered_map<unsigned long, std::pair<long long, uint64_t>> ioctlStatistics;
    void printIoctlStatistics();
    std::string ioctlToString(unsigned long request);
    std::string ioctlToStringImpl(unsigned long request);

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
  private:
    int getParamIoctl(int param, int *dstValue);
};
} // namespace NEO
