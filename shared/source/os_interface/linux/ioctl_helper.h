/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/utilities/stackvec.h"

#include "neo_igfxfmid.h"

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NEO {
class Drm;
class DrmAllocation;
class DrmMemoryManager;
class OsContextLinux;
class IoctlHelper;
class OSTime;
enum class CacheRegion : uint16_t;
enum class PreferredLocation : int16_t;
enum class AtomicAccessMode : uint32_t;
struct HardwareInfo;
struct HardwareIpVersion;
struct EngineInfo;
struct DrmQueryTopologyData;
struct TimeStampData;

class MemoryInfo;

struct MemoryRegion {
    MemoryClassInstance region;
    uint64_t probedSize;
    uint64_t unallocatedSize;
    uint64_t cpuVisibleSize;
    std::bitset<4> tilesMask;
};

struct EngineCapabilities {
    EngineClassInstance engine;
    struct Flags {
        bool copyClassSaturatePCIE;
        bool copyClassSaturateLink;
    };
    Flags capabilities;
};

struct DistanceInfo {
    MemoryClassInstance region;
    EngineClassInstance engine;
    int32_t distance;
};

struct VmBindParams {
    uint32_t vmId;
    uint32_t handle;
    uint64_t start;
    uint64_t offset;
    uint64_t length;
    uint64_t flags;
    uint64_t extensions;
    uint64_t userFence;
    uint64_t patIndex;
    uint64_t userptr;
    bool sharedSystemUsmEnabled;
    bool sharedSystemUsmBind;
};

struct UuidRegisterResult {
    int32_t retVal;
    uint32_t handle;
};

struct ResetStatsFault {
    uint64_t addr;
    uint16_t type;
    uint16_t level;
    uint16_t access;
    uint16_t flags;
};

using IoctlFunc = std::function<int(void *, int, unsigned long int, void *, bool)>;

struct ExternalCtx {
    void *handle = nullptr;
    IoctlFunc ioctl = nullptr;
};

using MemRegionsVec = StackVec<MemoryClassInstance, 5>;
using VmBindExtSetPatT = uint8_t[40];
using VmBindExtUserFenceT = uint8_t[56];

class IoctlHelper {
  public:
    IoctlHelper(Drm &drmArg) : drm(drmArg){};
    virtual ~IoctlHelper() {}
    static std::unique_ptr<IoctlHelper> getI915Helper(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion, Drm &drm);
    virtual int ioctl(DrmIoctl request, void *arg);
    virtual int ioctl(int fd, DrmIoctl request, void *arg);
    virtual void setExternalContext(ExternalCtx *ctx);

    virtual bool initialize() = 0;
    virtual bool isSetPairAvailable() = 0;
    virtual bool isChunkingAvailable() = 0;
    virtual bool isVmBindAvailable() = 0;
    virtual int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) = 0;
    virtual uint32_t createGem(uint64_t size, uint32_t memoryBanks, std::optional<bool> isCoherent) = 0;
    virtual CacheRegion closAlloc(CacheLevel cacheLevel) = 0;
    virtual uint16_t closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) = 0;
    virtual CacheRegion closFree(CacheRegion closIndex) = 0;
    virtual int waitUserFence(uint32_t ctxId, uint64_t address,
                              uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags,
                              bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) = 0;
    virtual uint32_t getAtomicAdvise(bool isNonAtomic) = 0;
    virtual uint32_t getAtomicAccess(AtomicAccessMode mode) = 0;
    virtual uint32_t getPreferredLocationAdvise() = 0;
    virtual std::optional<MemoryClassInstance> getPreferredLocationRegion(PreferredLocation memoryLocation, uint32_t memoryInstance) = 0;
    virtual bool setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) = 0;
    virtual bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const std::vector<uint32_t> &vmIds) { return true; }
    virtual bool setVmBoAdviseForChunking(int32_t handle, uint64_t start, uint64_t length, uint32_t attribute, void *region) = 0;
    virtual bool setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) = 0;
    virtual bool setGemTiling(void *setTiling) = 0;
    virtual bool getGemTiling(void *setTiling) = 0;
    virtual uint32_t getDirectSubmissionFlag() = 0;
    virtual std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles, uint64_t cookie) = 0;
    virtual uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLockedMemory, bool readOnlyResource) = 0;
    virtual int queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) = 0;
    virtual uint16_t getWaitUserFenceSoftFlag() = 0;
    virtual int execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, TaskCountType counterValue) = 0;
    virtual bool completionFenceExtensionSupported(const bool isVmBindAvailable) = 0;
    virtual bool isPageFaultSupported() = 0;
    virtual std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) = 0;
    virtual uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) = 0;
    virtual uint32_t createContextWithAccessCounters(GemContextCreateExt &gcc) = 0;
    virtual uint32_t createCooperativeContext(GemContextCreateExt &gcc) = 0;
    virtual void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) = 0;
    virtual void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) = 0;
    virtual void setVmBindUserFence(VmBindParams &vmBind, VmBindExtUserFenceT vmBindUserFence) = 0;
    virtual std::optional<uint32_t> getVmAdviseAtomicAttribute() = 0;
    virtual int vmBind(const VmBindParams &vmBindParams) = 0;
    virtual int vmUnbind(const VmBindParams &vmBindParams) = 0;
    virtual int getResetStats(ResetStats &resetStats, uint32_t *status, ResetStatsFault *resetStatsFault) = 0;
    virtual bool isEuStallSupported() = 0;
    virtual uint32_t getEuStallFdParameter() = 0;
    virtual bool perfOpenEuStallStream(uint32_t euStallFdParameter, uint32_t &samplingPeriodNs, uint64_t engineInstance, uint64_t notifyNReports, uint64_t gpuTimeStampfrequency, int32_t *stream) = 0;
    virtual bool perfDisableEuStallStream(int32_t *stream) = 0;
    virtual UuidRegisterResult registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) = 0;
    virtual UuidRegisterResult registerStringClassUuid(const std::string &uuid, uint64_t ptr, uint64_t size) = 0;
    virtual int unregisterUuid(uint32_t handle) = 0;
    virtual bool isContextDebugSupported() = 0;
    virtual int setContextDebugFlag(uint32_t drmContextId) = 0;
    virtual bool isDebugAttachAvailable() = 0;
    virtual unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const = 0;
    virtual int getDrmParamValue(DrmParam drmParam) const = 0;
    virtual std::string getDrmParamString(DrmParam param) const = 0;
    virtual std::string getIoctlString(DrmIoctl ioctlRequest) const = 0;

    virtual bool checkIfIoctlReinvokeRequired(int error, DrmIoctl ioctlRequest) const;
    virtual int createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex, bool allocateInterrupt) = 0;
    virtual bool createMediaContext(uint32_t vmId, void *controlSharedMemoryBuffer, uint32_t controlSharedMemoryBufferSize, void *controlBatchBuffer, uint32_t controlBatchBufferSize, void *&outDoorbell) { return false; }
    virtual bool releaseMediaContext(void *doorbellHandle) { return false; }

    virtual uint32_t getNumMediaDecoders() const { return 0; }
    virtual uint32_t getNumMediaEncoders() const { return 0; }

    virtual void fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) = 0;
    virtual void logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) = 0;
    virtual void fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) = 0;
    virtual void logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) = 0;
    virtual int getDrmParamValueBase(DrmParam drmParam) const = 0;
    unsigned int getIoctlRequestValueBase(DrmIoctl ioctlRequest) const;
    virtual bool setDomainCpu(uint32_t handle, bool writeEnable) = 0;

    std::string getIoctlStringBase(DrmIoctl ioctlRequest) const;
    virtual std::string getFileForMaxGpuFrequency() const = 0;
    virtual std::string getFileForMaxGpuFrequencyOfSubDevice(int tileId) const = 0;
    virtual std::string getFileForMaxMemoryFrequencyOfSubDevice(int tileId) const = 0;
    virtual bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) = 0;
    virtual bool isWaitBeforeBindRequired(bool bind) const = 0;
    virtual void *pciBarrierMmap() { return nullptr; };
    virtual void setupIpVersion();
    virtual bool isImmediateVmBindRequired() const { return false; }

    virtual void configureCcsMode(std::vector<std::string> &files, const std::string expectedFilePrefix, uint32_t ccsMode,
                                  std::vector<std::tuple<std::string, uint32_t>> &deviceCcsModeVec) = 0;

    void writeCcsMode(const std::string &gtFile, uint32_t ccsMode,
                      std::vector<std::tuple<std::string, uint32_t>> &deviceCcsModeVec);
    uint32_t getFlagsForPrimeHandleToFd() const;
    virtual std::unique_ptr<MemoryInfo> createMemoryInfo() = 0;
    virtual size_t getLocalMemoryRegionsSize(const MemoryInfo *memoryInfo, uint32_t subDevicesCount, uint32_t deviceBitfield) const = 0;
    virtual std::unique_ptr<EngineInfo> createEngineInfo(bool isSysmanEnabled) = 0;
    virtual bool getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) = 0;
    virtual bool getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) = 0;

    virtual bool setGpuCpuTimes(TimeStampData *pGpuCpuTime, OSTime *osTime) = 0;
    virtual bool registerResourceClasses() { return false; }
    virtual uint32_t registerResource(DrmResourceClass classType, const void *data, size_t size) { return 0; }
    virtual uint32_t registerIsaCookie(uint32_t isaHandle) { return 0; }
    virtual void unregisterResource(uint32_t handle) { return; }
    virtual bool resourceRegistrationEnabled() { return false; }

    virtual uint32_t notifyFirstCommandQueueCreated(const void *data, size_t size) { return 0; }
    virtual void notifyLastCommandQueueDestroyed(uint32_t handle) { return; }
    virtual int getEuDebugSysFsEnable() { return false; }
    virtual bool isVmBindPatIndexExtSupported() { return false; }

    virtual bool validPageFault(uint16_t flags) { return false; }
    virtual uint32_t getStatusForResetStats(bool banned) { return 0u; }
    virtual void registerBOBindHandle(Drm *drm, DrmAllocation *drmAllocation) { return; }

    virtual void insertEngineToContextParams(ContextParamEngines<> &contextParamEngines, uint32_t engineId, const EngineClassInstance *engineClassInstance, uint32_t tileId, bool hasVirtualEngines) = 0;
    virtual bool isPreemptionSupported() = 0;
    virtual uint32_t getTileIdFromGtId(uint32_t gtId) const = 0;
    virtual uint32_t getGtIdFromTileId(uint32_t tileId, uint16_t engineClass) const = 0;

    virtual bool allocateInterrupt(uint32_t &outHandle) { return false; }
    virtual bool releaseInterrupt(uint32_t handle) { return false; }

    virtual uint64_t *getPagingFenceAddress(uint32_t vmHandleId, OsContextLinux *osContext);
    virtual uint64_t acquireGpuRange(DrmMemoryManager &memoryManager, size_t &size, uint32_t rootDeviceIndex, AllocationType allocType, HeapIndex heapIndex);
    virtual void releaseGpuRange(DrmMemoryManager &memoryManager, void *address, size_t size, uint32_t rootDeviceIndex, AllocationType allocType);
    virtual void *mmapFunction(DrmMemoryManager &memoryManager, void *ptr, size_t size, int prot, int flags, int fd, off_t offset);
    virtual int munmapFunction(DrmMemoryManager &memoryManager, void *ptr, size_t size);
    virtual void registerMemoryToUnmap(DrmAllocation &allocation, void *pointer, size_t size, DrmAllocation::MemoryUnmapFunction unmapFunction);
    virtual BufferObject *allocUserptr(DrmMemoryManager &memoryManager, const AllocationData &allocData, uintptr_t address, size_t size, uint32_t rootDeviceIndex);
    virtual void syncUserptrAlloc(DrmMemoryManager &memoryManager, GraphicsAllocation &allocation) { return; };

    virtual bool queryDeviceParams(uint32_t *moduleId, uint16_t *serverType) { return false; }
    virtual std::unique_ptr<std::vector<uint32_t>> queryDeviceCaps() { return nullptr; }

    virtual bool isTimestampsRefreshEnabled() { return false; }
    virtual uint32_t getNumProcesses() const { return 1; }

    virtual bool makeResidentBeforeLockNeeded() const { return false; }
    virtual bool hasContextFreqHint() { return false; }
    virtual void fillExtSetparamLowLatency(GemContextCreateExtSetParam &extSetparam) { return; }
    virtual bool isSmallBarConfigAllowed() const = 0;

  protected:
    Drm &drm;
    ExternalCtx *externalCtx = nullptr;
};

class IoctlHelperI915 : public IoctlHelper {
  public:
    using IoctlHelper::IoctlHelper;

    static bool queryDeviceIdAndRevision(Drm &drm);
    void fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) override;
    void logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) override;
    void fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) override;
    void logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) override;
    int getDrmParamValueBase(DrmParam drmParam) const override;
    std::vector<EngineCapabilities> translateToEngineCaps(const std::vector<uint64_t> &data);
    std::unique_ptr<EngineInfo> createEngineInfo(bool isSysmanEnabled) override;
    std::unique_ptr<MemoryInfo> createMemoryInfo() override;
    size_t getLocalMemoryRegionsSize(const MemoryInfo *memoryInfo, uint32_t subDevicesCount, uint32_t deviceBitfield) const override;
    bool setDomainCpu(uint32_t handle, bool writeEnable) override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    std::string getDrmParamString(DrmParam param) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;
    int createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex, bool allocateInterrupt) override;
    std::string getFileForMaxGpuFrequency() const override;
    std::string getFileForMaxGpuFrequencyOfSubDevice(int tileId) const override;
    std::string getFileForMaxMemoryFrequencyOfSubDevice(int tileId) const override;
    void configureCcsMode(std::vector<std::string> &files, const std::string expectedFilePrefix, uint32_t ccsMode,
                          std::vector<std::tuple<std::string, uint32_t>> &deviceCcsModeVec) override;
    bool getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) override;
    bool getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) override;
    uint32_t createGem(uint64_t size, uint32_t memoryBanks, std::optional<bool> isCoherent) override;
    bool setGemTiling(void *setTiling) override;
    bool getGemTiling(void *setTiling) override;
    bool setGpuCpuTimes(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    void insertEngineToContextParams(ContextParamEngines<> &contextParamEngines, uint32_t engineId, const EngineClassInstance *engineClassInstance, uint32_t tileId, bool hasVirtualEngines) override;
    uint32_t getTileIdFromGtId(uint32_t gtId) const override { return gtId; }
    uint32_t getGtIdFromTileId(uint32_t tileId, uint16_t engineClass) const override { return tileId; }
    bool hasContextFreqHint() override;
    void fillExtSetparamLowLatency(GemContextCreateExtSetParam &extSetparam) override;
    bool isSmallBarConfigAllowed() const override { return true; }

  protected:
    virtual std::vector<MemoryRegion> translateToMemoryRegions(const std::vector<uint64_t> &regionInfo);
    bool translateTopologyInfo(const QueryTopologyInfo *queryTopologyInfo, DrmQueryTopologyData &topologyData, TopologyMapping &mapping);
    MOCKABLE_VIRTUAL void initializeGetGpuTimeFunction();
    bool (*getGpuTime)(::NEO::Drm &, uint64_t *) = nullptr;
    bool isPreemptionSupported() override;
    virtual EngineCapabilities::Flags getEngineCapabilitiesFlags(uint64_t capabilities) const;
};

class IoctlHelperUpstream : public IoctlHelperI915 {
  public:
    using IoctlHelperI915::IoctlHelperI915;

    bool initialize() override;
    bool isSetPairAvailable() override;
    bool isChunkingAvailable() override;
    bool isVmBindAvailable() override;
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) override;
    CacheRegion closAlloc(CacheLevel cacheLevel) override;
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
    uint32_t getDirectSubmissionFlag() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles, uint64_t cookie) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLockedMemory, bool readOnlyResource) override;
    int queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, TaskCountType counterValue) override;
    bool completionFenceExtensionSupported(const bool isVmBindAvailable) override;
    bool isPageFaultSupported() override;
    std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) override;
    uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) override;
    uint32_t createContextWithAccessCounters(GemContextCreateExt &gcc) override;
    uint32_t createCooperativeContext(GemContextCreateExt &gcc) override;
    void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    void setVmBindUserFence(VmBindParams &vmBind, VmBindExtUserFenceT vmBindUserFence) override;
    std::optional<uint32_t> getVmAdviseAtomicAttribute() override;
    int vmBind(const VmBindParams &vmBindParams) override;
    int vmUnbind(const VmBindParams &vmBindParams) override;
    int getResetStats(ResetStats &resetStats, uint32_t *status, ResetStatsFault *resetStatsFault) override;
    bool isEuStallSupported() override;
    uint32_t getEuStallFdParameter() override;
    bool perfOpenEuStallStream(uint32_t euStallFdParameter, uint32_t &samplingPeriodNs, uint64_t engineInstance, uint64_t notifyNReports, uint64_t gpuTimeStampfrequency, int32_t *stream) override;
    bool perfDisableEuStallStream(int32_t *stream) override;
    UuidRegisterResult registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) override;
    UuidRegisterResult registerStringClassUuid(const std::string &uuid, uint64_t ptr, uint64_t size) override;
    int unregisterUuid(uint32_t handle) override;
    bool isContextDebugSupported() override;
    int setContextDebugFlag(uint32_t drmContextId) override;
    bool isDebugAttachAvailable() override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    int getDrmParamValue(DrmParam drmParam) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override;
    bool isWaitBeforeBindRequired(bool bind) const override;

  protected:
    MOCKABLE_VIRTUAL void detectExtSetPatSupport();
    bool isSetPatSupported = false;
};

template <PRODUCT_FAMILY gfxProduct>
class IoctlHelperImpl : public IoctlHelperUpstream {
  public:
    using IoctlHelperUpstream::IoctlHelperUpstream;
    static std::unique_ptr<IoctlHelper> get(Drm &drm) {
        return std::make_unique<IoctlHelperImpl<gfxProduct>>(drm);
    }

    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) override;
    std::vector<MemoryRegion> translateToMemoryRegions(const std::vector<uint64_t> &regionInfo) override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;
};

class IoctlHelperPrelim20 : public IoctlHelperI915 {
  public:
    IoctlHelperPrelim20(Drm &drmArg);
    bool initialize() override;
    bool isSetPairAvailable() override;
    bool isChunkingAvailable() override;
    bool isVmBindAvailable() override;
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) override;
    CacheRegion closAlloc(CacheLevel cacheLevel) override;
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
    uint32_t getDirectSubmissionFlag() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles, uint64_t cookie) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLockedMemory, bool readOnlyResource) override;
    int queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, TaskCountType counterValue) override;
    bool completionFenceExtensionSupported(const bool isVmBindAvailable) override;
    bool isPageFaultSupported() override;
    std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) override;
    uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) override;
    uint32_t createContextWithAccessCounters(GemContextCreateExt &gcc) override;
    uint32_t createCooperativeContext(GemContextCreateExt &gcc) override;
    void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    void setVmBindUserFence(VmBindParams &vmBind, VmBindExtUserFenceT vmBindUserFence) override;
    std::optional<uint32_t> getVmAdviseAtomicAttribute() override;
    int vmBind(const VmBindParams &vmBindParams) override;
    int vmUnbind(const VmBindParams &vmBindParams) override;
    int getResetStats(ResetStats &resetStats, uint32_t *status, ResetStatsFault *resetStatsFault) override;
    bool perfOpenEuStallStream(uint32_t euStallFdParameter, uint32_t &samplingPeriodNs, uint64_t engineInstance, uint64_t notifyNReports, uint64_t gpuTimeStampfrequency, int32_t *stream) override;
    bool perfDisableEuStallStream(int32_t *stream) override;
    bool isEuStallSupported() override;
    uint32_t getEuStallFdParameter() override;
    UuidRegisterResult registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) override;
    UuidRegisterResult registerStringClassUuid(const std::string &uuid, uint64_t ptr, uint64_t size) override;
    int unregisterUuid(uint32_t handle) override;
    bool isContextDebugSupported() override;
    int setContextDebugFlag(uint32_t drmContextId) override;
    bool isDebugAttachAvailable() override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    int getDrmParamValue(DrmParam drmParam) const override;
    std::string getDrmParamString(DrmParam param) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;
    bool checkIfIoctlReinvokeRequired(int error, DrmIoctl ioctlRequest) const override;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override;
    bool isWaitBeforeBindRequired(bool bind) const override;
    void *pciBarrierMmap() override;
    void setupIpVersion() override;
    bool getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) override;
    uint32_t registerResource(DrmResourceClass classType, const void *data, size_t size) override;
    bool registerResourceClasses() override;
    uint32_t registerIsaCookie(uint32_t isaHandle) override;
    void unregisterResource(uint32_t handle) override;
    bool resourceRegistrationEnabled() override {
        return classHandles.size() > 0;
    }
    uint32_t notifyFirstCommandQueueCreated(const void *data, size_t size) override;
    void notifyLastCommandQueueDestroyed(uint32_t handle) override;
    int getEuDebugSysFsEnable() override;
    bool isVmBindPatIndexExtSupported() override { return true; }

    bool validPageFault(uint16_t flags) override;
    uint32_t getStatusForResetStats(bool banned) override;
    void registerBOBindHandle(Drm *drm, DrmAllocation *drmAllocation) override;
    EngineCapabilities::Flags getEngineCapabilitiesFlags(uint64_t capabilities) const override;

  protected:
    bool queryHwIpVersion(EngineClassInstance &engineInfo, HardwareIpVersion &ipVersion, int &ret);
    StackVec<uint32_t, size_t(DrmResourceClass::maxSize)> classHandles;
    bool handleExecBufferInNonBlockMode = false;
    std::string generateUUID();
    std::string generateElfUUID(const void *data);
    uint64_t uuid = 0;
};

extern std::optional<std::function<std::unique_ptr<IoctlHelper>(Drm &drm)>> ioctlHelperFactory[IGFX_MAX_PRODUCT];
} // namespace NEO
