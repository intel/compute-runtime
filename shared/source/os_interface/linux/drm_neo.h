/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/memory_manager/definitions/engine_limits.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/os_interface.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct GT_SYSTEM_INFO;

namespace aub_stream {
enum EngineType : uint32_t;
}

namespace NEO {
constexpr uint32_t contextPrivateParamBoost = 0x80000000;
constexpr uint32_t chunkingModeShared = 1;
constexpr uint32_t chunkingModeDevice = 2;

enum class AllocationType;
enum class CachePolicy : uint32_t;
enum class CacheRegion : uint16_t;
enum class SubmissionStatus : uint32_t;

class BufferObject;
class ReleaseHelper;
class DeviceFactory;
class MemoryInfo;
class OsContext;
class OsContextLinux;
class Gmm;
class GraphicsAllocation;
struct CacheInfo;
struct EngineInfo;
struct HardwareInfo;
struct RootDeviceEnvironment;
struct SystemInfo;

struct DeviceDescriptor {
    unsigned short deviceId;
    const HardwareInfo *pHwInfo;
    void (*setupHardwareInfo)(HardwareInfo *, bool, const ReleaseHelper *);
    const char *devName;
};

extern const DeviceDescriptor deviceDescriptorTable[];

class Drm : public DriverModel {
    friend DeviceFactory;

  public:
    static constexpr DriverModelType driverModelType = DriverModelType::drm;

    static SubmissionStatus getSubmissionStatusFromReturnCode(int32_t retCode);

    ~Drm() override;

    virtual int ioctl(DrmIoctl request, void *arg);

    unsigned int getDeviceHandle() const override {
        return 0;
    }
    PhysicalDevicePciSpeedInfo getPciSpeedInfo() const override;
    int enableTurboBoost();
    int getEuTotal(int &euTotal);
    int getSubsliceTotal(int &subsliceTotal);

    int getMaxGpuFrequency(HardwareInfo &hwInfo, int &maxGpuFrequency);
    int getEnabledPooledEu(int &enabled);
    int getMinEuInPool(int &minEUinPool);
    int getTimestampFrequency(int &frequency);
    int getOaTimestampFrequency(int &frequency);

    MOCKABLE_VIRTUAL int queryGttSize(uint64_t &gttSizeOutput, bool alignUpToFullRange);
    bool isPreemptionSupported() const { return preemptionSupported; }

    MOCKABLE_VIRTUAL void checkPreemptionSupport();
    inline int getFileDescriptor() const { return hwDeviceId->getFileDescriptor(); }
    int queryAdapterBDF();
    MOCKABLE_VIRTUAL int createDrmVirtualMemory(uint32_t &drmVmId);
    void destroyDrmVirtualMemory(uint32_t drmVmId);
    MOCKABLE_VIRTUAL int createDrmContext(uint32_t drmVmId, bool isDirectSubmissionRequested, bool isCooperativeContextRequested);
    void destroyDrmContext(uint32_t drmContextId);
    int queryVmId(uint32_t drmContextId, uint32_t &vmId);
    void setLowPriorityContextParam(uint32_t drmContextId);

    MOCKABLE_VIRTUAL unsigned int bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType);

    MOCKABLE_VIRTUAL int getErrno();
    bool setQueueSliceCount(uint64_t sliceCount);
    void checkQueueSliceSupport();
    uint64_t getSliceMask(uint64_t sliceCount);
    MOCKABLE_VIRTUAL bool querySystemInfo();
    MOCKABLE_VIRTUAL bool queryEngineInfo();
    MOCKABLE_VIRTUAL bool sysmanQueryEngineInfo();
    MOCKABLE_VIRTUAL bool queryEngineInfo(bool isSysmanEnabled);
    MOCKABLE_VIRTUAL bool queryMemoryInfo();
    bool queryTopology(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData);
    bool createVirtualMemoryAddressSpace(uint32_t vmCount);
    void destroyVirtualMemoryAddressSpace();
    uint32_t getVirtualMemoryAddressSpace(uint32_t vmId) const;
    MOCKABLE_VIRTUAL int bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo, const bool forcePagingFence);
    MOCKABLE_VIRTUAL int unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo);
    int setupHardwareInfo(const DeviceDescriptor *, bool);
    void setupSystemInfo(HardwareInfo *hwInfo, SystemInfo *sysInfo);
    void setupCacheInfo(const HardwareInfo &hwInfo);
    MOCKABLE_VIRTUAL void getPrelimVersion(std::string &prelimVersion);
    MOCKABLE_VIRTUAL int getEuDebugSysFsEnable();

    PhysicalDevicePciBusInfo getPciBusInfo() const override;
    bool isGpuHangDetected(OsContext &osContext) override;
    MOCKABLE_VIRTUAL bool checkResetStatus(OsContext &osContext);

    bool areNonPersistentContextsSupported() const { return nonPersistentContextsSupported; }
    void checkNonPersistentContextsSupport();
    void setNonPersistentContext(uint32_t drmContextId);
    bool isPerContextVMRequired() const {
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
    bool isDirectSubmissionActive() const { return this->directSubmissionActive; }
    MOCKABLE_VIRTUAL void setSharedSystemAllocEnable(bool value) { this->sharedSystemAllocEnable = value; }
    MOCKABLE_VIRTUAL bool isSharedSystemAllocEnabled() const { return this->sharedSystemAllocEnable; }
    void adjustSharedSystemMemCapabilities();

    MOCKABLE_VIRTUAL bool isSetPairAvailable();
    MOCKABLE_VIRTUAL bool getSetPairAvailable() { return setPairAvailable; }
    MOCKABLE_VIRTUAL bool isChunkingAvailable();
    MOCKABLE_VIRTUAL bool getChunkingAvailable() { return chunkingAvailable; }
    MOCKABLE_VIRTUAL uint32_t getChunkingMode() { return chunkingMode; }
    uint32_t getMinimalSizeForChunking() { return minimalChunkingSize; }

    MOCKABLE_VIRTUAL bool useVMBindImmediate() const;

    MOCKABLE_VIRTUAL bool isVmBindAvailable();
    MOCKABLE_VIRTUAL bool registerResourceClasses();
    MOCKABLE_VIRTUAL void setPageFaultSupported(bool value) { this->pageFaultSupported = value; }
    MOCKABLE_VIRTUAL void queryPageFaultSupport();
    bool hasPageFaultSupport() const;
    bool hasKmdMigrationSupport() const;
    bool checkToDisableScratchPage() { return disableScratch; }
    unsigned int getGpuFaultCheckThreshold() const { return gpuFaultCheckThreshold; }
    void configureScratchPagePolicy();
    void configureGpuFaultCheckThreshold();

    bool checkGpuPageFaultRequired() {
        return (checkToDisableScratchPage() && getGpuFaultCheckThreshold() != 0);
    }
    MOCKABLE_VIRTUAL bool resourceRegistrationEnabled();
    MOCKABLE_VIRTUAL uint32_t registerResource(DrmResourceClass classType, const void *data, size_t size);
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

    IoctlHelper *getIoctlHelper() const {
        return ioctlHelper.get();
    }

    const RootDeviceEnvironment &getRootDeviceEnvironment() const {
        return rootDeviceEnvironment;
    }
    const HardwareInfo *getHardwareInfo() const override;
    static bool isDrmSupported(int fileDescriptor);

    static Drm *create(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);
    static void overrideBindSupport(bool &useVmBind);
    std::string getPciPath() { return hwDeviceId->getPciPath(); }
    std::string getDeviceNode() { return hwDeviceId->getDeviceNode(); }

    std::pair<uint64_t, uint64_t> getFenceAddressAndValToWait(uint32_t vmHandleId, bool isLocked);
    void waitForBind(uint32_t vmHandleId);
    uint64_t getNextFenceVal(uint32_t vmHandleId) { return fenceVal[vmHandleId] + 1; }
    void incFenceVal(uint32_t vmHandleId) { fenceVal[vmHandleId]++; }
    uint64_t *getFenceAddr(uint32_t vmHandleId) { return &pagingFence[vmHandleId]; }

    int waitHandle(uint32_t waitHandle, int64_t timeout);
    enum class ValueWidth : uint32_t {
        u8,
        u16,
        u32,
        u64
    };
    MOCKABLE_VIRTUAL int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt,
                                       uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait);

    int waitOnUserFences(OsContextLinux &osContext, uint64_t address, uint64_t value, uint32_t numActiveTiles, int64_t timeout, uint32_t postSyncOffset, bool userInterrupt,
                         uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait);

    void setNewResourceBoundToVM(BufferObject *bo, uint32_t vmHandleId);

    const std::vector<int> &getSliceMappings(uint32_t deviceIndex);

    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevices(ExecutionEnvironment &executionEnvironment);
    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevice(ExecutionEnvironment &executionEnvironment, std::string &osPciPath);
    static std::vector<std::unique_ptr<HwDeviceId>> discoverDevices(ExecutionEnvironment &executionEnvironment, std::string &osPciPath);

    [[nodiscard]] std::unique_lock<std::mutex> lockBindFenceMutex();

    void setPciDomain(uint32_t domain) {
        pciDomain = domain;
    }

    MOCKABLE_VIRTUAL std::vector<uint64_t> getMemoryRegions();

    MOCKABLE_VIRTUAL bool completionFenceSupport();

    MOCKABLE_VIRTUAL uint32_t notifyFirstCommandQueueCreated(const void *data, size_t size);
    MOCKABLE_VIRTUAL void notifyLastCommandQueueDestroyed(uint32_t handle);

    uint64_t getPatIndex(Gmm *gmm, AllocationType allocationType, CacheRegion cacheRegion, CachePolicy cachePolicy, bool closEnabled, bool isSystemMemory) const;
    bool isVmBindPatIndexProgrammingSupported() const { return vmBindPatIndexProgrammingSupported; }
    MOCKABLE_VIRTUAL bool getDeviceMemoryMaxClockRateInMhz(uint32_t tileId, uint32_t &clkRate);
    MOCKABLE_VIRTUAL bool getDeviceMemoryPhysicalSizeInBytes(uint32_t tileId, uint64_t &physicalSize);
    void cleanup() override;
    bool readSysFsAsString(const std::string &relativeFilePath, std::string &readString);
    MOCKABLE_VIRTUAL std::string getSysFsPciPath();
    MOCKABLE_VIRTUAL std::string getSysFsPciPathBaseName();
    std::unique_ptr<HwDeviceIdDrm> &getHwDeviceId() { return hwDeviceId; }

    template <typename DataType>
    std::vector<DataType> query(uint32_t queryId, uint32_t queryItemFlags);
    static std::string getDrmVersion(int fileDescriptor);
    MOCKABLE_VIRTUAL uint32_t getAggregatedProcessCount() const;
    uint32_t getVmIdForContext(OsContext &osContext, uint32_t vmHandleId) const;
    MOCKABLE_VIRTUAL void setSharedSystemAllocAddressRange(uint64_t value) { this->sharedSystemAllocAddressRange = value; }
    MOCKABLE_VIRTUAL uint64_t getSharedSystemAllocAddressRange() const { return this->sharedSystemAllocAddressRange; }

  protected:
    Drm() = delete;
    Drm(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment);

    int waitOnUserFencesImpl(const OsContextLinux &osContext, uint64_t address, uint64_t value, uint32_t numActiveTiles, int64_t timeout, uint32_t postSyncOffset, bool userInterrupt,
                             uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait);

    int getQueueSliceCount(GemContextParamSseu *sseu);
    std::string generateUUID();
    std::string generateElfUUID(const void *data);
    void printIoctlStatistics();
    void setupIoctlHelper(const PRODUCT_FAMILY productFamily);
    void queryAndSetVmBindPatIndexProgrammingSupport();
    bool queryDeviceIdAndRevision();
    static uint64_t alignUpGttSize(uint64_t inputGttSize);

#pragma pack(1)
    struct PCIConfig {
        uint16_t vendorID;
        uint16_t deviceID;
        uint16_t command;
        uint16_t status;
        uint8_t revision;
        uint8_t progIF;
        uint8_t subclass;
        uint8_t classCode;
        uint8_t cacheLineSize;
        uint8_t latencyTimer;
        uint8_t headerType;
        uint8_t bist;
        uint32_t bar0[6];
        uint32_t cardbusCISPointer;
        uint16_t subsystemVendorID;
        uint16_t subsystemDeviceID;
        uint32_t rom;
        uint8_t capabilities;
        uint8_t reserved[7];
        uint8_t interruptLine;
        uint8_t interruptPIN;
        uint8_t minGrant;
        uint8_t maxLatency;
    };
#pragma pack()

    GemContextParamSseu sseu{};
    ADAPTER_BDF adapterBDF{};
    uint32_t pciDomain = 0;

    struct IoctlStatisticsEntry {
        long long totalTime = 0;
        uint64_t count = 0;
        long long minTime = std::numeric_limits<long long>::max();
        long long maxTime = 0;
    };
    std::unordered_map<DrmIoctl, IoctlStatisticsEntry> ioctlStatistics;

    std::mutex bindFenceMutex;
    std::array<uint64_t, EngineLimits::maxHandleCount> pagingFence;
    std::array<uint64_t, EngineLimits::maxHandleCount> fenceVal;
    std::vector<uint32_t> virtualMemoryIds;

    std::unique_ptr<HwDeviceIdDrm> hwDeviceId;
    std::unique_ptr<IoctlHelper> ioctlHelper;
    std::unique_ptr<SystemInfo> systemInfo;
    std::unique_ptr<CacheInfo> cacheInfo;
    std::unique_ptr<EngineInfo> engineInfo;
    std::unique_ptr<MemoryInfo> memoryInfo;

    std::once_flag checkBindOnce;
    std::once_flag checkSetPairOnce;
    std::once_flag checkChunkingOnce;
    std::once_flag checkCompletionFenceOnce;

    RootDeviceEnvironment &rootDeviceEnvironment;

    bool sliceCountChangeSupported = false;
    bool preemptionSupported = false;
    bool nonPersistentContextsSupported = false;
    bool requirePerContextVM = false;
    bool bindAvailable = false;
    bool directSubmissionActive = false;
    bool sharedSystemAllocEnable = false;
    uint64_t sharedSystemAllocAddressRange = 0lu;
    bool setPairAvailable = false;
    bool chunkingAvailable = false;
    uint32_t chunkingMode = 0;
    uint32_t minimalChunkingSize;
    bool contextDebugSupported = false;
    bool pageFaultSupported = false;
    bool completionFenceSupported = false;
    bool vmBindPatIndexProgrammingSupported = false;
    bool disableScratch = false;
    uint32_t gpuFaultCheckThreshold = 10u;

    std::atomic<uint32_t> gpuFaultCheckCounter{0u};

    bool memoryInfoQueried = false;
    bool engineInfoQueried = false;
    bool systemInfoQueried = false;
    bool topologyQueried = false;

  private:
    int getParamIoctl(DrmParam param, int *dstValue);
};
} // namespace NEO
