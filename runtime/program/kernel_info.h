/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hw_info.h"
#include "runtime/program/heap_info.h"
#include "runtime/program/kernel_arg_info.h"

#include "CL/cl.h"
#include "ocl_igc_shared/gtpin/gtpin_driver_common.h"
#include "patch_info.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
class BuiltinDispatchInfoBuilder;
class Device;
class Kernel;
struct KernelInfo;
class DispatchInfo;
struct KernelArgumentType;
class GraphicsAllocation;
class MemoryManager;

extern std::unordered_map<std::string, uint32_t> accessQualifierMap;
extern std::unordered_map<std::string, uint32_t> addressQualifierMap;
extern std::map<std::string, size_t> typeSizeMap;

struct WorkloadInfo {
    static const uint32_t undefinedOffset;
    static const uint32_t invalidParentEvent;

    uint32_t globalWorkOffsetOffsets[3] = {undefinedOffset, undefinedOffset, undefinedOffset};
    uint32_t globalWorkSizeOffsets[3] = {undefinedOffset, undefinedOffset, undefinedOffset};
    uint32_t localWorkSizeOffsets[3] = {undefinedOffset, undefinedOffset, undefinedOffset};
    uint32_t localWorkSizeOffsets2[3] = {undefinedOffset, undefinedOffset, undefinedOffset};
    uint32_t enqueuedLocalWorkSizeOffsets[3] = {undefinedOffset, undefinedOffset, undefinedOffset};
    uint32_t numWorkGroupsOffset[3] = {undefinedOffset, undefinedOffset, undefinedOffset};
    uint32_t maxWorkGroupSizeOffset = undefinedOffset;
    uint32_t workDimOffset = undefinedOffset;
    uint32_t slmStaticSize = 0;
    uint32_t simdSizeOffset = undefinedOffset;
    uint32_t parentEventOffset = undefinedOffset;
    uint32_t preferredWkgMultipleOffset = undefinedOffset;
    uint32_t privateMemoryStatelessSizeOffset = undefinedOffset;
    uint32_t localMemoryStatelessWindowSizeOffset = undefinedOffset;
    uint32_t localMemoryStatelessWindowStartAddressOffset = undefinedOffset;
};

static const float YTilingRatioValue = 1.3862943611198906188344642429164f;

struct WorkSizeInfo {

    uint32_t maxWorkGroupSize;
    uint32_t minWorkGroupSize;
    bool hasBarriers;
    uint32_t simdSize;
    uint32_t slmTotalSize;
    GFXCORE_FAMILY coreFamily;
    uint32_t numThreadsPerSubSlice;
    uint32_t localMemSize;
    bool imgUsed = false;
    bool yTiledSurfaces = false;
    bool useRatio = false;
    bool useStrictRatio = false;
    float targetRatio = 0;

    WorkSizeInfo(uint32_t maxWorkGroupSize, bool hasBarriers, uint32_t simdSize, uint32_t slmTotalSize, GFXCORE_FAMILY coreFamily, uint32_t numThreadsPerSubSlice, uint32_t localMemSize, bool imgUsed, bool yTiledSurface);
    WorkSizeInfo(const DispatchInfo &dispatchInfo);
    void setIfUseImg(Kernel *pKernel);
    void setMinWorkGroupSize();
    void checkRatio(const size_t workItems[3]);
};

struct DebugData {
    uint32_t vIsaSize = 0;
    uint32_t genIsaSize = 0;
    const char *vIsa = nullptr;
    const char *genIsa = nullptr;
};

struct KernelInfo {
  public:
    KernelInfo() = default;
    KernelInfo(const KernelInfo &) = delete;
    KernelInfo &operator=(const KernelInfo &) = delete;
    ~KernelInfo();

    void storeArgInfo(const SPatchKernelArgumentInfo *pkernelArgInfo);
    void storeKernelArgument(const SPatchDataParameterBuffer *pDataParameterKernelArg);
    void storeKernelArgument(const SPatchStatelessGlobalMemoryObjectKernelArgument *pStatelessGlobalKernelArg);
    void storeKernelArgument(const SPatchImageMemoryObjectKernelArgument *pImageMemObjKernelArg);
    void storeKernelArgument(const SPatchGlobalMemoryObjectKernelArgument *pGlobalMemObjKernelArg);
    void storeKernelArgument(const SPatchStatelessConstantMemoryObjectKernelArgument *pStatelessConstMemObjKernelArg);
    void storeKernelArgument(const SPatchStatelessDeviceQueueKernelArgument *pStatelessDeviceQueueKernelArg);
    void storeKernelArgument(const SPatchSamplerKernelArgument *pSamplerKernelArg);
    void storePatchToken(const SPatchExecutionEnvironment *execEnv);
    void storePatchToken(const SPatchAllocateStatelessPrivateSurface *pStatelessPrivateSurfaceArg);
    void storePatchToken(const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *pStatelessConstantMemorySurfaceWithInitializationArg);
    void storePatchToken(const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *pStatelessGlobalMemorySurfaceWithInitializationArg);
    void storePatchToken(const SPatchAllocateStatelessPrintfSurface *pStatelessPrintfSurfaceArg);
    void storePatchToken(const SPatchAllocateStatelessEventPoolSurface *pStatelessEventPoolSurfaceArg);
    void storePatchToken(const SPatchAllocateStatelessDefaultDeviceQueueSurface *pStatelessDefaultDeviceQueueSurfaceArg);
    void storePatchToken(const SPatchString *pStringArg);
    void storePatchToken(const SPatchKernelAttributesInfo *pKernelAttributesInfo);
    void storePatchToken(const SPatchAllocateSystemThreadSurface *pSystemThreadSurface);
    void storePatchToken(const SPatchAllocateSyncBuffer *pAllocateSyncBuffer);
    GraphicsAllocation *getGraphicsAllocation() const { return this->kernelAllocation; }
    cl_int resolveKernelInfo();
    void resizeKernelArgInfoAndRegisterParameter(uint32_t argCount) {
        if (kernelArgInfo.size() <= argCount) {
            kernelArgInfo.resize(argCount + 1);
        }
        if (!kernelArgInfo[argCount].needPatch) {
            kernelArgInfo[argCount].needPatch = true;
            argumentsToPatchNum++;
        }
    }

    void storeKernelArgPatchInfo(uint32_t argNum, uint32_t dataSize, uint32_t crossthreadOffset, uint32_t sourceOffset, uint32_t offsetSSH);

    size_t getSamplerStateArrayCount() const;
    size_t getSamplerStateArraySize(const HardwareInfo &hwInfo) const;
    size_t getBorderColorStateSize() const;
    size_t getBorderColorOffset() const;
    unsigned int getMaxSimdSize() const {
        const auto executionEnvironment = patchInfo.executionEnvironment;
        if (executionEnvironment == nullptr || executionEnvironment->LargestCompiledSIMDSize == 1) {
            return 1;
        }

        if (executionEnvironment->CompiledSIMD32) {
            return 32;
        }

        if (executionEnvironment->CompiledSIMD16) {
            return 16;
        }

        return 8;
    }
    bool hasDeviceEnqueue() const {
        return patchInfo.executionEnvironment ? !!patchInfo.executionEnvironment->HasDeviceEnqueue : false;
    }
    bool requiresSubgroupIndependentForwardProgress() const {
        return patchInfo.executionEnvironment ? !!patchInfo.executionEnvironment->SubgroupIndependentForwardProgressRequired : false;
    }
    size_t getMaxRequiredWorkGroupSize(size_t maxWorkGroupSize) const {
        auto requiredWorkGroupSizeX = patchInfo.executionEnvironment->RequiredWorkGroupSizeX;
        auto requiredWorkGroupSizeY = patchInfo.executionEnvironment->RequiredWorkGroupSizeY;
        auto requiredWorkGroupSizeZ = patchInfo.executionEnvironment->RequiredWorkGroupSizeZ;
        size_t maxRequiredWorkGroupSize = requiredWorkGroupSizeX * requiredWorkGroupSizeY * requiredWorkGroupSizeZ;
        if ((maxRequiredWorkGroupSize == 0) || (maxRequiredWorkGroupSize > maxWorkGroupSize)) {
            maxRequiredWorkGroupSize = maxWorkGroupSize;
        }
        return maxRequiredWorkGroupSize;
    }

    uint32_t getConstantBufferSize() const;
    int32_t getArgNumByName(const char *name) const {
        int32_t argNum = 0;
        for (auto &arg : kernelArgInfo) {
            if (arg.name == name) {
                return argNum;
            }
            ++argNum;
        }
        return -1;
    }

    bool createKernelAllocation(uint32_t rootDeviceIndex, MemoryManager *memoryManager);

    std::string name;
    std::string attributes;
    HeapInfo heapInfo = {};
    PatchInfo patchInfo = {};
    std::vector<KernelArgInfo> kernelArgInfo;
    std::vector<KernelArgInfo> kernelNonArgInfo;
    WorkloadInfo workloadInfo = {};
    std::vector<std::pair<uint32_t, uint32_t>> childrenKernelsIdOffset;
    bool usesSsh = false;
    bool requiresSshForBuffers = false;
    bool isValid = false;
    bool isVmeWorkload = false;
    char *crossThreadData = nullptr;
    size_t reqdWorkGroupSize[3] = {WorkloadInfo::undefinedOffset, WorkloadInfo::undefinedOffset, WorkloadInfo::undefinedOffset};
    size_t requiredSubGroupSize = 0;
    std::array<uint8_t, 3> workgroupWalkOrder = {{0, 1, 2}};
    std::array<uint8_t, 3> workgroupDimensionsOrder = {{0, 1, 2}};
    bool requiresWorkGroupOrder = false;
    uint32_t gpuPointerSize = 0;
    const BuiltinDispatchInfoBuilder *builtinDispatchBuilder = nullptr;
    uint32_t argumentsToPatchNum = 0;
    uint32_t systemKernelOffset = 0;
    uint64_t kernelId = 0;
    bool isKernelHeapSubstituted = false;
    GraphicsAllocation *kernelAllocation = nullptr;
    DebugData debugData;
    bool computeMode = false;
    const gtpin::igc_info_t *igcInfoForGtpin = nullptr;
};
} // namespace NEO
