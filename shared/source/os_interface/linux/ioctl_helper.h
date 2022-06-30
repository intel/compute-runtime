/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/utilities/stackvec.h"

#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NEO {
class Drm;
class OsContextLinux;
class IoctlHelper;
enum class CacheRegion : uint16_t;
struct HardwareInfo;

struct MemoryRegion {
    MemoryClassInstance region;
    uint64_t probedSize;
    uint64_t unallocatedSize;
};

struct EngineCapabilities {
    EngineClassInstance engine;
    uint64_t capabilities;
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
};

struct UuidRegisterResult {
    uint32_t retVal;
    uint32_t handle;
};

using MemRegionsVec = StackVec<MemoryClassInstance, 5>;
using VmBindExtSetPatT = uint8_t[40];
using VmBindExtUserFenceT = uint8_t[56];

class IoctlHelper {
  public:
    IoctlHelper(Drm &drmArg) : drm(drmArg){};
    virtual ~IoctlHelper() {}
    static std::unique_ptr<IoctlHelper> get(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion, const std::string &drmVersion, Drm &drm);
    uint32_t ioctl(Drm *drm, DrmIoctl request, void *arg);

    virtual bool isVmBindAvailable(Drm *drm) = 0;
    virtual uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId) = 0;
    virtual CacheRegion closAlloc(Drm *drm) = 0;
    virtual uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) = 0;
    virtual CacheRegion closFree(Drm *drm, CacheRegion closIndex) = 0;
    virtual int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                              uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) = 0;
    virtual uint32_t getAtomicAdvise(bool isNonAtomic) = 0;
    virtual uint32_t getPreferredLocationAdvise() = 0;
    virtual bool setVmBoAdvise(Drm *drm, int32_t handle, uint32_t attribute, void *region) = 0;
    virtual bool setVmPrefetch(Drm *drm, uint64_t start, uint64_t length, uint32_t region) = 0;
    virtual uint32_t getDirectSubmissionFlag() = 0;
    virtual std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) = 0;
    virtual uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) = 0;
    virtual uint32_t queryDistances(Drm *drm, std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) = 0;
    virtual uint16_t getWaitUserFenceSoftFlag() = 0;
    virtual int execBuffer(Drm *drm, ExecBuffer *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) = 0;
    virtual bool completionFenceExtensionSupported(const bool isVmBindAvailable) = 0;
    virtual std::optional<DrmParam> getHasPageFaultParamId() = 0;
    virtual std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) = 0;
    virtual uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) = 0;
    virtual uint32_t createContextWithAccessCounters(Drm *drm, GemContextCreateExt &gcc) = 0;
    virtual uint32_t createCooperativeContext(Drm *drm, GemContextCreateExt &gcc) = 0;
    virtual void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) = 0;
    virtual void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) = 0;
    virtual std::optional<uint64_t> getCopyClassSaturatePCIECapability() = 0;
    virtual std::optional<uint64_t> getCopyClassSaturateLinkCapability() = 0;
    virtual uint32_t getVmAdviseAtomicAttribute() = 0;
    virtual int vmBind(Drm *drm, const VmBindParams &vmBindParams) = 0;
    virtual int vmUnbind(Drm *drm, const VmBindParams &vmBindParams) = 0;
    virtual bool getEuStallProperties(std::array<uint64_t, 12u> &properties, uint64_t dssBufferSize,
                                      uint64_t samplingRate, uint64_t pollPeriod, uint64_t engineInstance, uint64_t notifyNReports) = 0;
    virtual uint32_t getEuStallFdParameter() = 0;
    virtual UuidRegisterResult registerUuid(Drm *drm, const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) = 0;
    virtual UuidRegisterResult registerStringClassUuid(Drm *drm, const std::string &uuid, uint64_t ptr, uint64_t size) = 0;
    virtual int unregisterUuid(Drm *drm, uint32_t handle) = 0;
    virtual bool isContextDebugSupported(Drm *drm) = 0;
    virtual int setContextDebugFlag(Drm *drm, uint32_t drmContextId) = 0;
    virtual bool isDebugAttachAvailable() = 0;
    virtual unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const = 0;
    virtual int getDrmParamValue(DrmParam drmParam) const = 0;

    virtual std::vector<MemoryRegion> translateToMemoryRegions(const std::vector<uint8_t> &regionInfo);

    uint32_t createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex);
    std::vector<EngineCapabilities> translateToEngineCaps(const std::vector<uint8_t> &data);

    void fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture);
    void logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size);

    void fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId);
    void logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger);
    int getDrmParamValueBase(DrmParam drmParam) const;
    unsigned int getIoctlRequestValueBase(DrmIoctl ioctlRequest) const;

  protected:
    Drm &drm;
};

class IoctlHelperUpstream : public IoctlHelper {
  public:
    using IoctlHelper::IoctlHelper;

    bool isVmBindAvailable(Drm *drm) override;
    uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId) override;
    CacheRegion closAlloc(Drm *drm) override;
    uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(Drm *drm, CacheRegion closIndex) override;
    int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) override;
    uint32_t getAtomicAdvise(bool isNonAtomic) override;
    uint32_t getPreferredLocationAdvise() override;
    bool setVmBoAdvise(Drm *drm, int32_t handle, uint32_t attribute, void *region) override;
    bool setVmPrefetch(Drm *drm, uint64_t start, uint64_t length, uint32_t region) override;
    uint32_t getDirectSubmissionFlag() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) override;
    uint32_t queryDistances(Drm *drm, std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(Drm *drm, ExecBuffer *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) override;
    bool completionFenceExtensionSupported(const bool isVmBindAvailable) override;
    std::optional<DrmParam> getHasPageFaultParamId() override;
    std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) override;
    uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) override;
    uint32_t createContextWithAccessCounters(Drm *drm, GemContextCreateExt &gcc) override;
    uint32_t createCooperativeContext(Drm *drm, GemContextCreateExt &gcc) override;
    void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    std::optional<uint64_t> getCopyClassSaturatePCIECapability() override;
    std::optional<uint64_t> getCopyClassSaturateLinkCapability() override;
    uint32_t getVmAdviseAtomicAttribute() override;
    int vmBind(Drm *drm, const VmBindParams &vmBindParams) override;
    int vmUnbind(Drm *drm, const VmBindParams &vmBindParams) override;
    bool getEuStallProperties(std::array<uint64_t, 12u> &properties, uint64_t dssBufferSize, uint64_t samplingRate,
                              uint64_t pollPeriod, uint64_t engineInstance, uint64_t notifyNReports) override;
    uint32_t getEuStallFdParameter() override;
    UuidRegisterResult registerUuid(Drm *drm, const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) override;
    UuidRegisterResult registerStringClassUuid(Drm *drm, const std::string &uuid, uint64_t ptr, uint64_t size) override;
    int unregisterUuid(Drm *drm, uint32_t handle) override;
    bool isContextDebugSupported(Drm *drm) override;
    int setContextDebugFlag(Drm *drm, uint32_t drmContextId) override;
    bool isDebugAttachAvailable() override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    int getDrmParamValue(DrmParam drmParam) const override;
};

template <PRODUCT_FAMILY gfxProduct>
class IoctlHelperImpl : public IoctlHelperUpstream {
  public:
    using IoctlHelperUpstream::IoctlHelperUpstream;
    static std::unique_ptr<IoctlHelper> get(Drm &drm) {
        return std::make_unique<IoctlHelperImpl<gfxProduct>>(drm);
    }

    uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId) override;
    std::vector<MemoryRegion> translateToMemoryRegions(const std::vector<uint8_t> &regionInfo) override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
};

class IoctlHelperPrelim20 : public IoctlHelper {
  public:
    using IoctlHelper::IoctlHelper;

    bool isVmBindAvailable(Drm *drm) override;
    uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId) override;
    CacheRegion closAlloc(Drm *drm) override;
    uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(Drm *drm, CacheRegion closIndex) override;
    int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) override;
    uint32_t getAtomicAdvise(bool isNonAtomic) override;
    uint32_t getPreferredLocationAdvise() override;
    bool setVmBoAdvise(Drm *drm, int32_t handle, uint32_t attribute, void *region) override;
    bool setVmPrefetch(Drm *drm, uint64_t start, uint64_t length, uint32_t region) override;
    uint32_t getDirectSubmissionFlag() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) override;
    uint32_t queryDistances(Drm *drm, std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(Drm *drm, ExecBuffer *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) override;
    bool completionFenceExtensionSupported(const bool isVmBindAvailable) override;
    std::optional<DrmParam> getHasPageFaultParamId() override;
    std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) override;
    uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) override;
    uint32_t createContextWithAccessCounters(Drm *drm, GemContextCreateExt &gcc) override;
    uint32_t createCooperativeContext(Drm *drm, GemContextCreateExt &gcc) override;
    void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    std::optional<uint64_t> getCopyClassSaturatePCIECapability() override;
    std::optional<uint64_t> getCopyClassSaturateLinkCapability() override;
    uint32_t getVmAdviseAtomicAttribute() override;
    int vmBind(Drm *drm, const VmBindParams &vmBindParams) override;
    int vmUnbind(Drm *drm, const VmBindParams &vmBindParams) override;
    bool getEuStallProperties(std::array<uint64_t, 12u> &properties, uint64_t dssBufferSize, uint64_t samplingRate,
                              uint64_t pollPeriod, uint64_t engineInstance, uint64_t notifyNReports) override;
    uint32_t getEuStallFdParameter() override;
    UuidRegisterResult registerUuid(Drm *drm, const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) override;
    UuidRegisterResult registerStringClassUuid(Drm *drm, const std::string &uuid, uint64_t ptr, uint64_t size) override;
    int unregisterUuid(Drm *drm, uint32_t handle) override;
    bool isContextDebugSupported(Drm *drm) override;
    int setContextDebugFlag(Drm *drm, uint32_t drmContextId) override;
    bool isDebugAttachAvailable() override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    int getDrmParamValue(DrmParam drmParam) const override;
};

} // namespace NEO
