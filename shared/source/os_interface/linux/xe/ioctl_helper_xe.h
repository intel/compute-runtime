/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <bitset>
#include <mutex>
#include <optional>

struct drm_xe_engine_class_instance;
struct drm_xe_query_gt_list;
struct drm_xe_query_config;

namespace NEO {

enum class EngineClass : uint16_t;

struct BindInfo {
    uint32_t handle;
    uint64_t userptr;
    uint64_t addr;
    uint64_t size;
};

class IoctlHelperXe : public IoctlHelper {
  public:
    using IoctlHelper::IoctlHelper;
    static std::unique_ptr<IoctlHelperXe> create(Drm &drmArg);
    IoctlHelperXe(Drm &drmArg);
    ~IoctlHelperXe() override;
    int ioctl(DrmIoctl request, void *arg) override;
    int ioctl(int fd, DrmIoctl request, void *arg) override;
    bool initialize() override;
    bool isSetPairAvailable() override;
    bool isChunkingAvailable() override;
    bool isVmBindAvailable() override;
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) override;
    uint32_t createGem(uint64_t size, uint32_t memoryBanks, std::optional<bool> isCoherent) override;
    CacheRegion closAlloc() override;
    uint16_t closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(CacheRegion closIndex) override;
    int waitUserFence(uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags,
                      bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override;
    uint32_t getAtomicAdvise(bool isNonAtomic) override;
    uint32_t getAtomicAccess(AtomicAccessMode mode) override;
    uint32_t getPreferredLocationAdvise() override;
    std::optional<MemoryClassInstance> getPreferredLocationRegion(PreferredLocation memoryLocation, uint32_t memoryInstance) override;
    bool setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) override;
    bool setVmBoAdviseForChunking(int32_t handle, uint64_t start, uint64_t length, uint32_t attribute, void *region) override;
    bool setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) override;
    bool setGemTiling(void *setTiling) override;
    bool getGemTiling(void *setTiling) override;
    uint32_t getDirectSubmissionFlag() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLock, bool readOnlyResource) override;
    int queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, TaskCountType counterValue) override;
    bool completionFenceExtensionSupported(const bool isVmBindAvailable) override;
    std::optional<DrmParam> getHasPageFaultParamId() override;
    std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) override;
    uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) override;
    uint32_t createContextWithAccessCounters(GemContextCreateExt &gcc) override;
    uint32_t createCooperativeContext(GemContextCreateExt &gcc) override;
    void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    void setVmBindUserFence(VmBindParams &vmBind, VmBindExtUserFenceT vmBindUserFence) override;
    uint32_t getVmAdviseAtomicAttribute() override;
    int vmBind(const VmBindParams &vmBindParams) override;
    int vmUnbind(const VmBindParams &vmBindParams) override;
    int getResetStats(ResetStats &resetStats, uint32_t *status, ResetStatsFault *resetStatsFault) override;
    bool getEuStallProperties(std::array<uint64_t, 12u> &properties, uint64_t dssBufferSize, uint64_t samplingRate, uint64_t pollPeriod,
                              uint64_t engineInstance, uint64_t notifyNReports) override;
    bool isEuStallSupported() override;
    uint32_t getEuStallFdParameter() override;
    bool perfOpenEuStallStream(uint32_t euStallFdParameter, std::array<uint64_t, 12u> &properties, int32_t *stream) override;
    bool perfDisableEuStallStream(int32_t *stream) override;
    MOCKABLE_VIRTUAL int perfOpenIoctl(DrmIoctl request, void *arg);
    unsigned int getIoctlRequestValuePerf(DrmIoctl ioctlRequest) const;
    UuidRegisterResult registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) override;
    UuidRegisterResult registerStringClassUuid(const std::string &uuid, uint64_t ptr, uint64_t size) override;
    int unregisterUuid(uint32_t handle) override;
    bool isContextDebugSupported() override;
    int setContextDebugFlag(uint32_t drmContextId) override;
    bool isDebugAttachAvailable() override;
    int getEuDebugSysFsEnable() override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    unsigned int getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const;
    int getDrmParamValue(DrmParam drmParam) const override;
    int getDrmParamValueBase(DrmParam drmParam) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;
    int createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex, bool allocateInterrupt) override;
    std::string getDrmParamString(DrmParam param) const override;
    bool getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) override;
    std::string getFileForMaxGpuFrequency() const override;
    std::string getFileForMaxGpuFrequencyOfSubDevice(int subDeviceId) const override;
    std::string getFileForMaxMemoryFrequencyOfSubDevice(int subDeviceId) const override;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override;
    bool isWaitBeforeBindRequired(bool bind) const override;
    std::unique_ptr<EngineInfo> createEngineInfo(bool isSysmanEnabled) override;
    std::unique_ptr<MemoryInfo> createMemoryInfo() override;
    size_t getLocalMemoryRegionsSize(const MemoryInfo *memoryInfo, uint32_t subDevicesCount, uint32_t deviceBitfield) const override;
    void setupIpVersion() override;
    void getTopologyData(size_t nTiles, std::vector<std::bitset<8>> *geomDss, std::vector<std::bitset<8>> *computeDss, std::vector<std::bitset<8>> *euDss, DrmQueryTopologyData &topologyData, bool &isComputeDssEmpty);
    void getTopologyMap(size_t nTiles, std::vector<std::bitset<8>> *dssInfo, TopologyMap &topologyMap);

    bool setGpuCpuTimes(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    void fillBindInfoForIpcHandle(uint32_t handle, size_t size) override;
    bool getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) override;
    bool isImmediateVmBindRequired() const override;
    void fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) override;
    void logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) override;
    void fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) override;
    void logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) override;
    bool setDomainCpu(uint32_t handle, bool writeEnable) override;
    uint16_t getCpuCachingMode(std::optional<bool> isCoherent, bool allocationInSystemMemory) const;
    void addDebugMetadata(DrmResourceClass type, uint64_t *offset, uint64_t size);
    uint32_t registerResource(DrmResourceClass classType, const void *data, size_t size) override;
    void unregisterResource(uint32_t handle) override;
    void insertEngineToContextParams(ContextParamEngines<> &contextParamEngines, uint32_t engineId, const EngineClassInstance *engineClassInstance, uint32_t tileId, bool hasVirtualEngines) override;
    void registerBOBindHandle(Drm *drm, DrmAllocation *drmAllocation) override;
    bool resourceRegistrationEnabled() override { return true; }
    bool isPreemptionSupported() override { return true; }

  protected:
    static constexpr uint32_t maxContextSetProperties = 4;

    const char *xeGetClassName(int className);
    const char *xeGetBindOperationName(int bindOperation);
    const char *xeGetBindFlagsName(int bindFlags);

    const char *xeGetengineClassName(uint32_t engineClass);
    template <typename DataType>
    std::vector<DataType> queryData(uint32_t queryId);
    virtual int xeWaitUserFence(uint32_t ctxId, uint16_t op, uint64_t addr, uint64_t value, int64_t timeout, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait);
    void setupXeWaitUserFenceStruct(void *arg, uint32_t ctxId, uint16_t op, uint64_t addr, uint64_t value, int64_t timeout);
    int xeVmBind(const VmBindParams &vmBindParams, bool bindOp);
    void xeShowBindTable();
    void updateBindInfo(uint32_t handle, uint64_t userPtr, uint64_t size);
    int debuggerOpenIoctl(DrmIoctl request, void *arg);
    int debuggerMetadataCreateIoctl(DrmIoctl request, void *arg);
    int debuggerMetadataDestroyIoctl(DrmIoctl request, void *arg);
    int getRunaloneExtProperty();
    virtual bool isExtraEngineClassAllowed(uint16_t engineClass) const { return false; }
    virtual uint32_t getCxlType(struct drm_xe_query_config *config) { return 0u; }

    struct UserFenceExtension {
        static constexpr uint32_t tagValue = 0x123987;
        uint32_t tag;
        uint64_t addr;
        uint64_t value;
    };

    uint16_t getDefaultEngineClass(const aub_stream::EngineType &defaultEngineType);
    void setOptionalContextProperties(Drm &drm, void *extProperties, uint32_t &extIndexInOut);
    virtual void setContextProperties(const OsContextLinux &osContext, void *extProperties, uint32_t &extIndexInOut);
    virtual void applyContextFlags(void *execQueueCreate, bool allocateInterrupt){};

    struct GtIpVersion {
        uint16_t major;
        uint16_t minor;
        uint16_t revision;
    };
    bool queryHwIpVersion(GtIpVersion &gtIpVersion);

    int maxExecQueuePriority = 0;
    std::mutex xeLock;
    std::vector<BindInfo> bindInfo;
    std::vector<uint32_t> hwconfig;
    std::vector<drm_xe_engine_class_instance> contextParamEngine;

    std::vector<uint64_t> queryGtListData;
    constexpr static int invalidIndex = -1;
    StackVec<int, 2> gtIdToTileId;
    StackVec<int, 2> tileIdToGtId;
    drm_xe_query_gt_list *xeGtListData = nullptr;

    std::unique_ptr<drm_xe_engine_class_instance> defaultEngine;
    struct DebugMetadata {
        DrmResourceClass type;
        uint64_t offset;
        uint64_t size;
        bool isCookie;
    };

    template <typename... XeLogArgs>
    void xeLog(XeLogArgs &&...args) const;

    struct ExecObjectXe {
        uint64_t gpuAddress;
        uint32_t handle;
    };

    struct ExecBufferXe {
        ExecObjectXe *execObject;
        uint64_t startOffset;
        uint32_t drmContextId;
    };

    struct SupportedFeatures {
        union {
            struct {
                uint32_t vmBindReadOnly : 1;
                uint32_t vmBindImmediate : 1;
                uint32_t reserved : 30;
            } flags;
            uint32_t allFlags = 0;
        };
    } supportedFeatures{};
    static_assert(sizeof(SupportedFeatures::flags) == sizeof(SupportedFeatures::allFlags), "");

    void querySupportedFeatures();
    uint64_t getAdditionalFlagsForVmBind(bool bindImmediate, bool readOnlyResource);
};

template <typename... XeLogArgs>
void IoctlHelperXe::xeLog(XeLogArgs &&...args) const {
    PRINT_DEBUG_STRING(debugManager.flags.PrintXeLogs.get(), stderr, args...);
}

} // namespace NEO
