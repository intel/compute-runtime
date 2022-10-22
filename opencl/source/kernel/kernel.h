/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/csr_properties_flags.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/kernel/kernel_execution_type.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/stackvec.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/api/cl_types.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/properties_helper.h"
#include "opencl/source/kernel/kernel_objects_for_aux_translation.h"
#include "opencl/source/program/program.h"

#include <vector>

namespace NEO {
struct CompletionStamp;
class Buffer;
class CommandQueue;
class CommandStreamReceiver;
class GraphicsAllocation;
class ImageTransformer;
class Surface;
class PrintfHandler;
class MultiDeviceKernel;

class Kernel : public ReferenceTrackedObject<Kernel> {
  public:
    static const uint32_t kernelBinaryAlignment = 64;

    enum kernelArgType {
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
        kernelArgType type;
        uint32_t allocId;
        uint32_t allocIdMemoryManagerCounter;
        bool isPatched = false;
        bool isStatelessUncacheable = false;
        bool isSetToNullptr = false;
    };

    enum class TunningStatus {
        STANDARD_TUNNING_IN_PROGRESS,
        SUBDEVICE_TUNNING_IN_PROGRESS,
        TUNNING_DONE
    };

    enum class TunningType {
        DISABLED,
        SIMPLE,
        FULL
    };

    typedef int32_t (Kernel::*KernelArgHandler)(uint32_t argIndex,
                                                size_t argSize,
                                                const void *argVal);

    template <typename kernel_t = Kernel, typename program_t = Program>
    static kernel_t *create(program_t *program, const KernelInfo &kernelInfo, ClDevice &clDevice, cl_int *errcodeRet) {
        cl_int retVal;
        kernel_t *pKernel = nullptr;

        pKernel = new kernel_t(program, kernelInfo, clDevice);
        retVal = pKernel->initialize();

        if (retVal != CL_SUCCESS) {
            delete pKernel;
            pKernel = nullptr;
        }

        auto localMemSize = static_cast<uint32_t>(clDevice.getDevice().getDeviceInfo().localMemSize);
        auto slmInlineSize = kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize;

        if (slmInlineSize > 0 && localMemSize < slmInlineSize) {
            PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n", slmInlineSize, localMemSize);
            retVal = CL_OUT_OF_RESOURCES;
        }

        if (errcodeRet) {
            *errcodeRet = retVal;
        }

        if (fileLoggerInstance().enabled()) {
            std::string source;
            program->getSource(source);
            fileLoggerInstance().dumpKernel(kernelInfo.kernelDescriptor.kernelMetadata.kernelName, source);
        }

        return pKernel;
    }

    Kernel &operator=(const Kernel &) = delete;
    Kernel(const Kernel &) = delete;

    ~Kernel() override;

    static bool isMemObj(kernelArgType kernelArg) {
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

    MOCKABLE_VIRTUAL bool canTransformImages() const;
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

    bool usesBindfulAddressingForBuffers() const {
        return KernelDescriptor::BindfulAndStateless == kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode;
    }

    inline const KernelDescriptor &getDescriptor() const {
        return kernelInfo.kernelDescriptor;
    }
    inline const KernelInfo &getKernelInfo() const {
        return kernelInfo;
    }

    Context &getContext() const {
        return program->getContext();
    }

    Program *getProgram() const { return program; }

    uint32_t getScratchSize() {
        return kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0];
    }

    uint32_t getPrivateScratchSize() {
        return kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[1];
    }

    bool usesSyncBuffer() const;
    void patchSyncBuffer(GraphicsAllocation *gfxAllocation, size_t bufferOffset);
    void *patchBindlessSurfaceState(NEO::GraphicsAllocation *alloc, uint32_t bindless);

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
                        kernelArgType argType,
                        void *argObject,
                        const void *argValue,
                        size_t argSize,
                        GraphicsAllocation *argSvmAlloc = nullptr,
                        cl_mem_flags argSvmFlags = 0);
    void storeKernelArgAllocIdMemoryManagerCounter(uint32_t argIndex, uint32_t allocIdMemoryManagerCounter);
    const void *getKernelArg(uint32_t argIndex) const;
    const SimpleKernelArgInfo &getKernelArgInfo(uint32_t argIndex) const;

    bool getAllowNonUniform() const { return program->getAllowNonUniform(); }
    bool isVmeKernel() const { return kernelInfo.kernelDescriptor.kernelAttributes.flags.usesVme; }
    bool requiresSystolicPipelineSelectMode() const { return systolicPipelineSelectMode; }

    void performKernelTuning(CommandStreamReceiver &commandStreamReceiver, const Vec3<size_t> &lws, const Vec3<size_t> &gws, const Vec3<size_t> &offsets, TimestampPacketContainer *timestampContainer);
    MOCKABLE_VIRTUAL bool isSingleSubdevicePreferred() const;
    void setInlineSamplers();

    // residency for kernel surfaces
    MOCKABLE_VIRTUAL void makeResident(CommandStreamReceiver &commandStreamReceiver);
    MOCKABLE_VIRTUAL void getResidency(std::vector<Surface *> &dst);
    bool requiresCoherency();
    void resetSharedObjectsPatchAddresses();
    bool isUsingSharedObjArgs() const { return usingSharedObjArgs; }
    bool hasUncacheableStatelessArgs() const { return statelessUncacheableArgsCount > 0; }

    bool hasPrintfOutput() const;

    cl_int checkCorrectImageAccessQualifier(cl_uint argIndex,
                                            size_t argSize,
                                            const void *argValue) const;

    static uint32_t dummyPatchLocation;

    uint32_t allBufferArgsStateful = CL_TRUE;

    bool isBuiltIn = false;

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

    MOCKABLE_VIRTUAL bool requiresCacheFlushCommand(const CommandQueue &commandQueue) const;

    using CacheFlushAllocationsVec = StackVec<GraphicsAllocation *, 32>;
    void getAllocationsForCacheFlush(CacheFlushAllocationsVec &out) const;

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
    uint32_t getMaxWorkGroupCount(const cl_uint workDim, const size_t *localWorkSize, const CommandQueue *commandQueue) const;

    uint64_t getKernelStartAddress(const bool localIdsGenerationByRuntime, const bool kernelUsesLocalIds, const bool isCssUsed, const bool returnFullAddress) const;

    bool isKernelDebugEnabled() const { return debugEnabled; }
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

    static bool graphicsAllocationTypeUseSystemMemory(AllocationType type);
    void setDestinationAllocationInSystemMemory(bool value) {
        isDestinationAllocationInSystemMemory = value;
    }
    bool getDestinationAllocationInSystemMemory() const {
        return isDestinationAllocationInSystemMemory;
    }

  protected:
    struct KernelConfig {
        Vec3<size_t> gws;
        Vec3<size_t> lws;
        Vec3<size_t> offsets;
        bool operator==(const KernelConfig &other) const { return this->gws == other.gws && this->lws == other.lws && this->offsets == other.offsets; }
    };
    struct KernelConfigHash {
        size_t operator()(KernelConfig const &config) const {
            auto hash = std::hash<size_t>{};
            size_t gwsHashX = hash(config.gws.x);
            size_t gwsHashY = hash(config.gws.y);
            size_t gwsHashZ = hash(config.gws.z);
            size_t gwsHash = hashCombine(gwsHashX, gwsHashY, gwsHashZ);
            size_t lwsHashX = hash(config.lws.x);
            size_t lwsHashY = hash(config.lws.y);
            size_t lwsHashZ = hash(config.lws.z);
            size_t lwsHash = hashCombine(lwsHashX, lwsHashY, lwsHashZ);
            size_t offsetsHashX = hash(config.offsets.x);
            size_t offsetsHashY = hash(config.offsets.y);
            size_t offsetsHashZ = hash(config.offsets.z);
            size_t offsetsHash = hashCombine(offsetsHashX, offsetsHashY, offsetsHashZ);
            return hashCombine(gwsHash, lwsHash, offsetsHash);
        }

        size_t hashCombine(size_t hash1, size_t hash2, size_t hash3) const {
            return (hash1 ^ (hash2 << 1u)) ^ (hash3 << 2u);
        }
    };
    struct KernelSubmissionData {
        std::unique_ptr<TimestampPacketContainer> kernelStandardTimestamps;
        std::unique_ptr<TimestampPacketContainer> kernelSubdeviceTimestamps;
        TunningStatus status;
        bool singleSubdevicePreferred = false;
    };

    Kernel(Program *programArg, const KernelInfo &kernelInfo, ClDevice &clDevice);

    void makeArgsResident(CommandStreamReceiver &commandStreamReceiver);

    void *patchBufferOffset(const ArgDescPointer &argAsPtr, void *svmPtr, GraphicsAllocation *svmAlloc);

    void patchWithImplicitSurface(void *ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const ArgDescPointer &arg);

    void provideInitializationHints();

    void markArgPatchedAndResolveArgs(uint32_t argIndex);
    void resolveArgs();

    void reconfigureKernel();
    bool hasDirectStatelessAccessToSharedBuffer() const;
    bool hasDirectStatelessAccessToHostMemory() const;
    bool hasIndirectStatelessAccessToHostMemory() const;

    void addAllocationToCacheFlushVector(uint32_t argIndex, GraphicsAllocation *argAllocation);
    bool allocationForCacheFlush(GraphicsAllocation *argAllocation) const;

    const ClDevice &getDevice() const {
        return clDevice;
    }
    cl_int patchPrivateSurface();

    bool hasTunningFinished(KernelSubmissionData &submissionData);
    bool hasRunFinished(TimestampPacketContainer *timestampContainer);

    UnifiedMemoryControls unifiedMemoryControls{};

    std::map<uint32_t, MemObj *> migratableArgsMap{};

    std::unordered_map<KernelConfig, KernelSubmissionData, KernelConfigHash> kernelSubmissionMap;

    std::vector<SimpleKernelArgInfo> kernelArguments;
    std::vector<KernelArgHandler> kernelArgHandlers;
    std::vector<GraphicsAllocation *> kernelSvmGfxAllocations;
    std::vector<GraphicsAllocation *> kernelUnifiedMemoryGfxAllocations;
    std::vector<PatchInfoData> patchInfoDataList;
    std::vector<GraphicsAllocation *> kernelArgRequiresCacheFlush;
    std::vector<size_t> slmSizes;

    std::unique_ptr<ImageTransformer> imageTransformer;
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

    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::None;
    KernelExecutionType executionType = KernelExecutionType::Default;

    uint32_t patchedArgumentsNum = 0;
    uint32_t startOffset = 0;
    uint32_t statelessUncacheableArgsCount = 0;
    uint32_t additionalKernelExecInfo = AdditionalKernelExecInfo::DisableOverdispatch;
    uint32_t maxKernelWorkGroupSize = 0;
    uint32_t slmTotalSize = 0u;
    uint32_t sshLocalSize = 0u;
    uint32_t crossThreadDataSize = 0u;

    bool containsStatelessWrites = true;
    bool usingSharedObjArgs = false;
    bool usingImages = false;
    bool usingImagesOnly = false;
    bool auxTranslationRequired = false;
    bool systolicPipelineSelectMode = false;
    bool svmAllocationsRequireCacheFlush = false;
    bool isUnifiedMemorySyncRequired = true;
    bool debugEnabled = false;
    bool singleSubdevicePreferredInCurrentEnqueue = false;
    bool kernelHasIndirectAccess = true;
    bool anyKernelArgumentUsingSystemMemory = false;
    bool isDestinationAllocationInSystemMemory = false;
};

} // namespace NEO
