/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/csr_properties_flags.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/kernel/kernel_execution_type.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/logger.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/kernel/kernel_objects_for_aux_translation.h"

#include <map>
#include <vector>

namespace NEO {
class MemObj;
class TimestampPacketContainer;
class Context;
class Program;
struct ImplicitArgs;
enum class AllocationType;
struct PatchInfoData;
struct CompletionStamp;
class Buffer;
class CommandQueue;
class CommandStreamReceiver;
class GraphicsAllocation;
class Surface;
class PrintfHandler;
class MultiDeviceKernel;
class LocalIdsCache;

class Kernel : public ReferenceTrackedObject<Kernel>, NEO::NonCopyableAndNonMovableClass {
  public:
    static const uint32_t kernelBinaryAlignment = 64;

    enum KernelArgType {
        NONE_OBJ,
        IMAGE_OBJ,
        BUFFER_OBJ,
        PIPE_OBJ,
        SVM_OBJ,
        SVM_ALLOC_OBJ,
        SAMPLER_OBJ,
        ACCELERATOR_OBJ,
        DEVICE_QUEUE_OBJ,
        SLM_OBJ
    };

    struct SimpleKernelArgInfo {
        cl_mem_flags svmFlags;
        void *object;
        const void *value;
        size_t size;
        GraphicsAllocation *svmAllocation;
        KernelArgType type;
        uint32_t allocId;
        uint32_t allocIdMemoryManagerCounter;
        bool isPatched = false;
        bool isStatelessUncacheable = false;
        bool isSetToNullptr = false;
    };

    typedef int32_t (Kernel::*KernelArgHandler)(uint32_t argIndex,
                                                size_t argSize,
                                                const void *argVal);

    template <typename KernelType = Kernel, typename ProgramType = Program>
    static KernelType *create(ProgramType *program, const KernelInfo &kernelInfo, ClDevice &clDevice, cl_int &errcodeRet) {
        cl_int retVal;
        KernelType *pKernel = nullptr;

        pKernel = new KernelType(program, kernelInfo, clDevice);
        retVal = pKernel->initialize();

        if (retVal != CL_SUCCESS) {
            delete pKernel;
            pKernel = nullptr;
        }
        errcodeRet = retVal;

        if (fileLoggerInstance().enabled()) {
            std::string source;
            program->getSource(source);
            fileLoggerInstance().dumpKernel(kernelInfo.kernelDescriptor.kernelMetadata.kernelName, source);
        }

        return pKernel;
    }

    ~Kernel() override;

    static bool isMemObj(KernelArgType kernelArg) {
        return kernelArg == BUFFER_OBJ || kernelArg == IMAGE_OBJ || kernelArg == PIPE_OBJ;
    }

    bool isAuxTranslationRequired() const { return auxTranslationRequired; }
    void setAuxTranslationRequired(bool onOff) { auxTranslationRequired = onOff; }
    void updateAuxTranslationRequired();

    ArrayRef<uint8_t> getCrossThreadDataRef() {
        return ArrayRef<uint8_t>(reinterpret_cast<uint8_t *>(crossThreadData), crossThreadDataSize);
    }

    char *getCrossThreadData() const {
        return crossThreadData;
    }

    uint32_t getCrossThreadDataSize() const {
        return crossThreadDataSize;
    }

    cl_int initialize();

    MOCKABLE_VIRTUAL cl_int cloneKernel(Kernel *pSourceKernel);

    MOCKABLE_VIRTUAL bool isPatched() const;

    // API entry points
    cl_int setArgument(uint32_t argIndex, size_t argSize, const void *argVal) { return setArg(argIndex, argSize, argVal); }
    cl_int setArgSvm(uint32_t argIndex, size_t svmAllocSize, void *svmPtr, GraphicsAllocation *svmAlloc, cl_mem_flags svmFlags);
    MOCKABLE_VIRTUAL cl_int setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc, uint32_t allocId);

    void setSvmKernelExecInfo(GraphicsAllocation *argValue);
    void clearSvmKernelExecInfo();

    cl_int getInfo(cl_kernel_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet) const;

    cl_int getArgInfo(cl_uint argIndx, cl_kernel_arg_info paramName,
                      size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const;

    cl_int getWorkGroupInfo(cl_kernel_work_group_info paramName,
                            size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const;

    cl_int getSubGroupInfo(cl_kernel_sub_group_info paramName,
                           size_t inputValueSize, const void *inputValue,
                           size_t paramValueSize, void *paramValue,
                           size_t *paramValueSizeRet) const;

    const void *getKernelHeap() const;
    void *getSurfaceStateHeap() const;
    const void *getDynamicStateHeap() const;

    size_t getKernelHeapSize() const;
    size_t getSurfaceStateHeapSize() const;
    size_t getDynamicStateHeapSize() const;
    size_t getNumberOfBindingTableStates() const;
    size_t getBindingTableOffset() const {
        return localBindingTableOffset;
    }

    void resizeSurfaceStateHeap(void *pNewSsh, size_t newSshSize, size_t newBindingTableCount, size_t newBindingTableOffset);

    void substituteKernelHeap(void *newKernelHeap, size_t newKernelHeapSize);
    bool isKernelHeapSubstituted() const;
    uint64_t getKernelId() const;
    void setKernelId(uint64_t newKernelId);
    uint32_t getStartOffset() const;
    void setStartOffset(uint32_t offset);

    const std::vector<SimpleKernelArgInfo> &getKernelArguments() const {
        return kernelArguments;
    }

    size_t getKernelArgsNumber() const {
        return kernelArguments.size();
    }

    inline const KernelDescriptor &getDescriptor() const {
        return kernelInfo.kernelDescriptor;
    }
    inline const KernelInfo &getKernelInfo() const {
        return kernelInfo;
    }

    Context &getContext() const;

    Program *getProgram() const { return program; }

    uint32_t getScratchSize(uint32_t slotId) {
        return kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[slotId];
    }

    bool usesSyncBuffer() const;
    void patchSyncBuffer(GraphicsAllocation *gfxAllocation, size_t bufferOffset);
    void *patchBindlessSurfaceState(NEO::GraphicsAllocation *alloc, uint32_t bindless);
    uint32_t getSurfaceStateIndexForBindlessOffset(NEO::CrossThreadDataOffset bindlessOffset) const;

    template <bool heaplessEnabled>
    void patchBindlessSurfaceStatesForImplicitArgs(uint64_t bindlessSurfaceStatesBaseAddress) const;

    template <bool heaplessEnabled>
    void patchBindlessSurfaceStatesInCrossThreadData(uint64_t bindlessSurfaceStatesBaseAddress) const;

    void patchBindlessSamplerStatesInCrossThreadData(uint64_t bindlessSamplerStatesBaseAddress) const;

    // Helpers
    cl_int setArg(uint32_t argIndex, uint32_t argValue);
    cl_int setArg(uint32_t argIndex, uint64_t argValue);
    cl_int setArg(uint32_t argIndex, cl_mem argValue);
    cl_int setArg(uint32_t argIndex, cl_mem argValue, uint32_t mipLevel);
    cl_int setArg(uint32_t argIndex, size_t argSize, const void *argVal);

    // Handlers
    void setKernelArgHandler(uint32_t argIndex, KernelArgHandler handler);

    void unsetArg(uint32_t argIndex);

    cl_int setArgImmediate(uint32_t argIndex,
                           size_t argSize,
                           const void *argVal);

    cl_int setArgBuffer(uint32_t argIndex,
                        size_t argSize,
                        const void *argVal);

    cl_int setArgPipe(uint32_t argIndex,
                      size_t argSize,
                      const void *argVal);

    cl_int setArgImage(uint32_t argIndex,
                       size_t argSize,
                       const void *argVal);

    cl_int setArgImageWithMipLevel(uint32_t argIndex,
                                   size_t argSize,
                                   const void *argVal, uint32_t mipLevel);

    cl_int setArgLocal(uint32_t argIndex,
                       size_t argSize,
                       const void *argVal);

    cl_int setArgSampler(uint32_t argIndex,
                         size_t argSize,
                         const void *argVal);

    cl_int setArgAccelerator(uint32_t argIndex,
                             size_t argSize,
                             const void *argVal);

    void storeKernelArg(uint32_t argIndex,
                        KernelArgType argType,
                        void *argObject,
                        const void *argValue,
                        size_t argSize,
                        GraphicsAllocation *argSvmAlloc = nullptr,
                        cl_mem_flags argSvmFlags = 0);
    void storeKernelArgAllocIdMemoryManagerCounter(uint32_t argIndex, uint32_t allocIdMemoryManagerCounter);
    const void *getKernelArg(uint32_t argIndex) const;
    const SimpleKernelArgInfo &getKernelArgInfo(uint32_t argIndex) const;

    bool getAllowNonUniform() const;
    bool isVmeKernel() const { return kernelInfo.kernelDescriptor.kernelAttributes.flags.usesVme; }
    bool requiresSystolicPipelineSelectMode() const { return systolicPipelineSelectMode; }

    MOCKABLE_VIRTUAL bool isSingleSubdevicePreferred() const;
    void setInlineSamplers();

    // residency for kernel surfaces
    MOCKABLE_VIRTUAL void makeResident(CommandStreamReceiver &commandStreamReceiver);
    MOCKABLE_VIRTUAL void getResidency(std::vector<Surface *> &dst);
    void resetSharedObjectsPatchAddresses();
    bool isUsingSharedObjArgs() const { return usingSharedObjArgs; }
    bool hasUncacheableStatelessArgs() const { return statelessUncacheableArgsCount > 0; }

    bool hasPrintfOutput() const;

    cl_int checkCorrectImageAccessQualifier(cl_uint argIndex,
                                            size_t argSize,
                                            const void *argValue) const;

    static uint32_t dummyPatchLocation;

    uint32_t allBufferArgsStateful = CL_TRUE;

    KernelExecutionType getExecutionType() const {
        return executionType;
    }

    bool is32Bit() const {
        return kernelInfo.kernelDescriptor.kernelAttributes.gpuPointerSize == 4;
    }

    size_t getPerThreadSystemThreadSurfaceSize() const {
        return kernelInfo.kernelDescriptor.kernelAttributes.perThreadSystemThreadSurfaceSize;
    }

    std::vector<PatchInfoData> &getPatchInfoDataList() { return patchInfoDataList; };
    bool usesImages() const {
        return usingImages;
    }
    bool usesOnlyImages() const {
        return usingImagesOnly;
    }

    std::unique_ptr<KernelObjsForAuxTranslation> fillWithKernelObjsForAuxTranslation();

    void setAuxTranslationDirection(AuxTranslationDirection auxTranslationDirection) {
        this->auxTranslationDirection = auxTranslationDirection;
    }
    void setUnifiedMemorySyncRequirement(bool isUnifiedMemorySyncRequired) {
        this->isUnifiedMemorySyncRequired = isUnifiedMemorySyncRequired;
    }
    void setUnifiedMemoryProperty(cl_kernel_exec_info infoType, bool infoValue);
    void setUnifiedMemoryExecInfo(GraphicsAllocation *argValue);
    void clearUnifiedMemoryExecInfo();

    bool areStatelessWritesUsed() { return containsStatelessWrites; }
    int setKernelThreadArbitrationPolicy(uint32_t propertyValue);
    cl_int setKernelExecutionType(cl_execution_info_kernel_type_intel executionType);
    void getSuggestedLocalWorkSize(const cl_uint workDim, const size_t *globalWorkSize, const size_t *globalWorkOffset,
                                   size_t *localWorkSize);
    uint32_t getMaxWorkGroupCount(const cl_uint workDim, const size_t *localWorkSize, const CommandQueue *commandQueue, bool forceSingleTileQuery) const;

    uint64_t getKernelStartAddress(const bool localIdsGenerationByRuntime, const bool kernelUsesLocalIds, const bool isCssUsed, const bool returnFullAddress) const;

    void setAdditionalKernelExecInfo(uint32_t additionalKernelExecInfo);
    uint32_t getAdditionalKernelExecInfo() const;
    MOCKABLE_VIRTUAL bool requiresWaDisableRccRhwoOptimization() const;

    // dispatch traits
    void setGlobalWorkOffsetValues(uint32_t globalWorkOffsetX, uint32_t globalWorkOffsetY, uint32_t globalWorkOffsetZ);
    void setGlobalWorkSizeValues(uint32_t globalWorkSizeX, uint32_t globalWorkSizeY, uint32_t globalWorkSizeZ);
    void setLocalWorkSizeValues(uint32_t localWorkSizeX, uint32_t localWorkSizeY, uint32_t localWorkSizeZ);
    void setLocalWorkSize2Values(uint32_t localWorkSizeX, uint32_t localWorkSizeY, uint32_t localWorkSizeZ);
    void setEnqueuedLocalWorkSizeValues(uint32_t localWorkSizeX, uint32_t localWorkSizeY, uint32_t localWorkSizeZ);
    void setNumWorkGroupsValues(uint32_t numWorkGroupsX, uint32_t numWorkGroupsY, uint32_t numWorkGroupsZ);
    void setWorkDim(uint32_t workDim);

    const uint32_t *getDispatchTrait(const CrossThreadDataOffset offset) const {
        return isValidOffset(offset) ? reinterpret_cast<uint32_t *>(getCrossThreadData() + offset)
                                     : &Kernel::dummyPatchLocation;
    }
    const uint32_t *getWorkDim() const { return getDispatchTrait(getDescriptor().payloadMappings.dispatchTraits.workDim); }
    std::array<const uint32_t *, 3> getDispatchTraitArray(const CrossThreadDataOffset dispatchTrait[3]) const { return {getDispatchTrait(dispatchTrait[0]), getDispatchTrait(dispatchTrait[1]), getDispatchTrait(dispatchTrait[2])}; }
    std::array<const uint32_t *, 3> getGlobalWorkOffsetValues() const { return getDispatchTraitArray(getDescriptor().payloadMappings.dispatchTraits.globalWorkOffset); }
    std::array<const uint32_t *, 3> getLocalWorkSizeValues() const { return getDispatchTraitArray(getDescriptor().payloadMappings.dispatchTraits.localWorkSize); }
    std::array<const uint32_t *, 3> getLocalWorkSize2Values() const { return getDispatchTraitArray(getDescriptor().payloadMappings.dispatchTraits.localWorkSize2); }
    std::array<const uint32_t *, 3> getEnqueuedLocalWorkSizeValues() const { return getDispatchTraitArray(getDescriptor().payloadMappings.dispatchTraits.enqueuedLocalWorkSize); }
    std::array<const uint32_t *, 3> getNumWorkGroupsValues() const { return getDispatchTraitArray(getDescriptor().payloadMappings.dispatchTraits.numWorkGroups); }

    bool isLocalWorkSize2Patchable();

    uint32_t getMaxKernelWorkGroupSize() const;
    uint32_t getSlmTotalSize() const;
    bool getHasIndirectAccess() const {
        return this->kernelHasIndirectAccess;
    }

    MultiDeviceKernel *getMultiDeviceKernel() const { return pMultiDeviceKernel; }
    void setMultiDeviceKernel(MultiDeviceKernel *pMultiDeviceKernelToSet) { pMultiDeviceKernel = pMultiDeviceKernelToSet; }

    bool areMultipleSubDevicesInContext() const;
    bool requiresMemoryMigration() const { return migratableArgsMap.size() > 0; }
    const std::map<uint32_t, MemObj *> &getMemObjectsToMigrate() const { return migratableArgsMap; }
    ImplicitArgs *getImplicitArgs() const { return pImplicitArgs.get(); }
    const HardwareInfo &getHardwareInfo() const;
    bool isAnyKernelArgumentUsingSystemMemory() const {
        return anyKernelArgumentUsingSystemMemory;
    }

    bool isAnyKernelArgumentUsingZeroCopyMemory() const {
        return anyKernelArgumentUsingZeroCopyMemory;
    }

    static bool graphicsAllocationTypeUseSystemMemory(AllocationType type);
    void setDestinationAllocationInSystemMemory(bool value) {
        isDestinationAllocationInSystemMemory = value;
    }
    bool getDestinationAllocationInSystemMemory() const {
        return isDestinationAllocationInSystemMemory;
    }

    void setLocalIdsForGroup(const Vec3<uint16_t> &groupSize, void *destination) const;
    size_t getLocalIdsSizeForGroup(const Vec3<uint16_t> &groupSize) const;
    size_t getLocalIdsSizePerThread() const;

    const GfxCoreHelper &getGfxCoreHelper() const {
        return getDevice().getGfxCoreHelper();
    }
    bool isBuiltInKernel() const {
        return isBuiltIn;
    }

  protected:
    Kernel(Program *programArg, const KernelInfo &kernelInfo, ClDevice &clDevice);

    void makeArgsResident(CommandStreamReceiver &commandStreamReceiver);

    void *patchBufferOffset(const ArgDescPointer &argAsPtr, void *svmPtr, GraphicsAllocation *svmAlloc);

    void patchWithImplicitSurface(uint64_t ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const ArgDescPointer &arg);

    void provideInitializationHints();

    void markArgPatchedAndResolveArgs(uint32_t argIndex);

    void reconfigureKernel();
    bool hasDirectStatelessAccessToSharedBuffer() const;
    bool hasDirectStatelessAccessToHostMemory() const;
    bool hasIndirectStatelessAccessToHostMemory() const;

    const ClDevice &getDevice() const {
        return clDevice;
    }
    cl_int patchPrivateSurface();

    void initializeLocalIdsCache();
    std::unique_ptr<LocalIdsCache> localIdsCache;

    UnifiedMemoryControls unifiedMemoryControls{};

    std::map<uint32_t, MemObj *> migratableArgsMap{};

    std::vector<SimpleKernelArgInfo> kernelArguments;
    std::vector<KernelArgHandler> kernelArgHandlers;
    std::vector<GraphicsAllocation *> kernelSvmGfxAllocations;
    std::vector<GraphicsAllocation *> kernelUnifiedMemoryGfxAllocations;
    std::vector<PatchInfoData> patchInfoDataList;
    std::vector<size_t> slmSizes;

    std::unique_ptr<char[]> pSshLocal;
    std::unique_ptr<ImplicitArgs> pImplicitArgs = nullptr;

    uint64_t privateSurfaceSize = 0u;

    size_t numberOfBindingTableStates = 0u;
    size_t localBindingTableOffset = 0u;

    const ExecutionEnvironment &executionEnvironment;
    Program *program;
    ClDevice &clDevice;
    const KernelInfo &kernelInfo;
    GraphicsAllocation *privateSurface = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;

    uint32_t *maxWorkGroupSizeForCrossThreadData = &Kernel::dummyPatchLocation;
    uint32_t *dataParameterSimdSize = &Kernel::dummyPatchLocation;
    uint32_t *parentEventOffset = &Kernel::dummyPatchLocation;
    uint32_t *preferredWkgMultipleOffset = &Kernel::dummyPatchLocation;
    char *crossThreadData = nullptr;

    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::none;
    KernelExecutionType executionType = KernelExecutionType::defaultType;

    uint32_t patchedArgumentsNum = 0;
    uint32_t startOffset = 0;
    uint32_t statelessUncacheableArgsCount = 0;
    uint32_t additionalKernelExecInfo = AdditionalKernelExecInfo::disableOverdispatch;
    uint32_t maxKernelWorkGroupSize = 0;
    uint32_t slmTotalSize = 0u;
    uint32_t sshLocalSize = 0u;
    uint32_t crossThreadDataSize = 0u;
    uint32_t implicitArgsVersion = 0;

    bool containsStatelessWrites = true;
    bool usingSharedObjArgs = false;
    bool usingImages = false;
    bool usingImagesOnly = false;
    bool auxTranslationRequired = false;
    bool systolicPipelineSelectMode = false;
    bool isUnifiedMemorySyncRequired = true;
    bool kernelHasIndirectAccess = true;
    bool anyKernelArgumentUsingSystemMemory = false;
    bool anyKernelArgumentUsingZeroCopyMemory = false;
    bool isDestinationAllocationInSystemMemory = false;
    bool isBuiltIn = false;
};

static_assert(NEO::NonCopyableAndNonMovable<Kernel>);

} // namespace NEO
