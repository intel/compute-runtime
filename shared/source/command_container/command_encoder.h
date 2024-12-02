/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/encode_alu_helper.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/kernel/kernel_execution_type.h"

#include <list>
#include <optional>

namespace NEO {
enum class SlmPolicy;

class BindlessHeapsHelper;
class Gmm;
class GmmHelper;
class IndirectHeap;
class InOrderExecInfo;
class ProductHelper;

struct DeviceInfo;
struct DispatchKernelEncoderI;
struct EncodeSurfaceStateArgs;
struct HardwareInfo;
struct KernelDescriptor;
struct KernelInfo;
struct MiFlushArgs;
struct EncodeDummyBlitWaArgs;
struct PipeControlArgs;
struct PipelineSelectArgs;
struct RootDeviceEnvironment;
struct StateBaseAddressProperties;
struct StateComputeModeProperties;
struct ImplicitArgs;

struct EncodeDispatchKernelArgs {
    uint64_t eventAddress = 0;
    uint64_t postSyncImmValue = 0;
    uint64_t inOrderCounterValue = 0;
    Device *device = nullptr;
    NEO::InOrderExecInfo *inOrderExecInfo = nullptr;
    DispatchKernelEncoderI *dispatchInterface = nullptr;
    IndirectHeap *surfaceStateHeap = nullptr;
    IndirectHeap *dynamicStateHeap = nullptr;
    const void *threadGroupDimensions = nullptr;
    void *outWalkerPtr = nullptr;
    void *cpuWalkerBuffer = nullptr;
    void *cpuPayloadBuffer = nullptr;
    void *outImplicitArgsPtr = nullptr;
    std::list<void *> *additionalCommands = nullptr;
    PreemptionMode preemptionMode = PreemptionMode::Initial;
    NEO::RequiredPartitionDim requiredPartitionDim = NEO::RequiredPartitionDim::none;
    NEO::RequiredDispatchWalkOrder requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none;
    uint32_t localRegionSize = NEO::localRegionSizeParamNotSet;
    uint32_t partitionCount = 0u;
    uint32_t reserveExtraPayloadSpace = 0;
    uint32_t maxWgCountPerTile = 0;
    int32_t defaultPipelinedThreadArbitrationPolicy = NEO::ThreadArbitrationPolicy::NotPresent;
    bool isIndirect = false;
    bool isPredicate = false;
    bool isTimestampEvent = false;
    bool requiresUncachedMocs = false;
    bool isInternal = false;
    bool isCooperative = false;
    bool isHostScopeSignalEvent = false;
    bool isKernelUsingSystemAllocation = false;
    bool isKernelDispatchedFromImmediateCmdList = false;
    bool isRcs = false;
    bool dcFlushEnable = false;
    bool isHeaplessModeEnabled = false;
    bool isHeaplessStateInitEnabled = false;
    bool interruptEvent = false;
    bool immediateScratchAddressPatching = false;
    bool makeCommandView = false;

    bool requiresSystemMemoryFence() const {
        return (isHostScopeSignalEvent && isKernelUsingSystemAllocation);
    }
};

enum class MiPredicateType : uint32_t {
    disable = 0,
    noopOnResult2Clear = 1,
    noopOnResult2Set = 2
};

enum class CompareOperation : uint32_t {
    equal = 0,
    notEqual = 1,
    greaterOrEqual = 2,
    less = 3,
};

struct EncodeWalkerArgs {
    EncodeWalkerArgs() = delete;

    EncodeWalkerArgs(const KernelDescriptor &kernelDescriptor, KernelExecutionType kernelExecutionType, NEO::RequiredDispatchWalkOrder requiredDispatchWalkOrder,
                     uint32_t localRegionSize, uint32_t maxFrontEndThreads, bool requiredSystemFence)
        : kernelDescriptor(kernelDescriptor),
          kernelExecutionType(kernelExecutionType),
          requiredDispatchWalkOrder(requiredDispatchWalkOrder),
          localRegionSize(localRegionSize),
          maxFrontEndThreads(maxFrontEndThreads),
          requiredSystemFence(requiredSystemFence) {}

    const KernelDescriptor &kernelDescriptor;
    KernelExecutionType kernelExecutionType = KernelExecutionType::defaultType;
    NEO::RequiredDispatchWalkOrder requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none;
    uint32_t localRegionSize = NEO::localRegionSizeParamNotSet;
    uint32_t maxFrontEndThreads = 0;
    bool requiredSystemFence = false;
};

template <typename GfxFamily>
struct EncodeDispatchKernel {
    static constexpr size_t timestampDestinationAddressAlignment = 16;
    static constexpr size_t immWriteDestinationAddressAlignment = 8;

    using DefaultWalkerType = typename GfxFamily::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

    static void encodeCommon(CommandContainer &container, EncodeDispatchKernelArgs &args);

    template <typename WalkerType>
    static void encode(CommandContainer &container, EncodeDispatchKernelArgs &args);

    template <typename WalkerType>
    static void encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs);

    template <typename InterfaceDescriptorType>
    static void setupPreferredSlmSize(InterfaceDescriptorType *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment,
                                      const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy);

    static uint32_t getThreadCountPerSubslice(const HardwareInfo &hwInfo);
    static uint32_t alignPreferredSlmSize(uint32_t slmSize);

    template <typename InterfaceDescriptorType>
    static void encodeEuSchedulingPolicy(InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy);

    template <typename WalkerType>
    static void encodeThreadData(WalkerType &walkerCmd,
                                 const uint32_t *startWorkGroup,
                                 const uint32_t *numWorkGroups,
                                 const uint32_t *workGroupSizes,
                                 uint32_t simd,
                                 uint32_t localIdDimensions,
                                 uint32_t threadsPerThreadGroup,
                                 uint32_t threadExecutionMask,
                                 bool localIdsGenerationByRuntime,
                                 bool inlineDataProgrammingRequired,
                                 bool isIndirect,
                                 uint32_t requiredWorkGroupOrder,
                                 const RootDeviceEnvironment &rootDeviceEnvironment);

    template <typename InterfaceDescriptorType>
    static void setGrfInfo(InterfaceDescriptorType *pInterfaceDescriptor, uint32_t grfCount, const size_t &sizeCrossThreadData,
                           const size_t &sizePerThreadData, const RootDeviceEnvironment &rootDeviceEnvironment);

    static void *getInterfaceDescriptor(CommandContainer &container, IndirectHeap *childDsh, uint32_t &iddOffset);

    static bool isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                                    const size_t *lws,
                                                    std::array<uint8_t, 3> walkOrder,
                                                    bool requireInputWalkOrder,
                                                    uint32_t &requiredWalkOrder,
                                                    uint32_t simd);

    static bool inlineDataProgrammingRequired(const KernelDescriptor &kernelDesc);

    template <typename InterfaceDescriptorType>
    static void programBarrierEnable(InterfaceDescriptorType &interfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo);

    template <typename WalkerType, typename InterfaceDescriptorType>
    static void encodeThreadGroupDispatch(InterfaceDescriptorType &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo,
                                          const uint32_t *threadGroupDimensions, const uint32_t threadGroupCount, const uint32_t grfCount, const uint32_t threadsPerThreadGroup,
                                          WalkerType &walkerCmd);

    static void adjustBindingTablePrefetch(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t samplerCount, uint32_t bindingTableEntryCount);

    template <typename WalkerType>
    static void adjustTimestampPacket(WalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);

    template <typename WalkerType>
    static void setupPostSyncForRegularEvent(WalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);

    template <typename WalkerType>
    static void setWalkerRegionSettings(WalkerType &walkerCmd, const NEO::Device &device, uint32_t partitionCount, uint32_t workgroupSize, uint32_t maxWgCountPerTile, bool requiredDispatchWalkOrder);

    template <typename WalkerType>
    static void setupPostSyncForInOrderExec(WalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);

    template <typename WalkerType>
    static void setupPostSyncMocs(WalkerType &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool dcFlush);

    template <typename WalkerType>
    static void adjustWalkOrder(WalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);

    template <bool heaplessModeEnabled>
    static void programInlineDataHeapless(uint8_t *inlineDataPtr, EncodeDispatchKernelArgs &args, CommandContainer &container, uint64_t offsetThreadData, uint64_t scratchPtr);

    static size_t getSizeRequiredDsh(const KernelDescriptor &kernelDescriptor, uint32_t iddCount);
    static size_t getSizeRequiredSsh(const KernelInfo &kernelInfo);
    inline static size_t additionalSizeRequiredDsh(uint32_t iddCount);
    static bool isDshNeeded(const DeviceInfo &deviceInfo);
    static size_t getDefaultDshAlignment();
    static constexpr size_t getDefaultSshAlignment() {
        return GfxFamily::cacheLineSize;
    }

    static size_t getDefaultIOHAlignment();
    template <bool isHeapless>
    static void setScratchAddress(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &submissionCsr);
    template <bool isHeapless>
    static uint64_t getScratchAddressForImmediatePatching(CommandContainer &container, EncodeDispatchKernelArgs &args);
    template <bool isHeapless>
    static void patchScratchAddressInImplicitArgs(ImplicitArgs &implicitArgs, uint64_t scratchAddress, bool scratchPtrPatchingRequired);

    static size_t getInlineDataOffset(EncodeDispatchKernelArgs &args);
    static size_t getScratchPtrOffsetOfImplicitArgs();

    template <typename WalkerType>
    static void forceComputeWalkerPostSyncFlushWithWrite(WalkerType &walkerCmd);

    static uint32_t alignSlmSize(uint32_t slmSize);
    static uint32_t computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize);

    static bool singleTileExecImplicitScalingRequired(bool cooperativeKernel);

    template <typename WalkerType, typename InterfaceDescriptorType>
    static void overrideDefaultValues(WalkerType &walkerCmd, InterfaceDescriptorType &interfaceDescriptor);
    template <typename WalkerType>
    static void encodeWalkerPostSyncFields(WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs);
    template <typename WalkerType>
    static void encodeComputeDispatchAllWalker(WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs);
};

template <typename GfxFamily>
struct EncodeStates {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
    using SAMPLER_BORDER_COLOR_STATE = typename GfxFamily::SAMPLER_BORDER_COLOR_STATE;

    static constexpr uint32_t alignIndirectStatePointer = MemoryConstants::cacheLineSize;
    static constexpr size_t alignInterfaceDescriptorData = MemoryConstants::cacheLineSize;

    static uint32_t copySamplerState(IndirectHeap *dsh,
                                     uint32_t samplerStateOffset,
                                     uint32_t samplerCount,
                                     uint32_t borderColorOffset,
                                     const void *fnDynamicStateHeap,
                                     BindlessHeapsHelper *bindlessHeapHelper,
                                     const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSshHeapSize();
};

template <typename GfxFamily>
struct EncodeMath {
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename GfxFamily::MI_MATH;
    constexpr static size_t streamCommandSize = sizeof(MI_MATH) + sizeof(MI_MATH_ALU_INST_INLINE) * RegisterConstants::numAluInstForReadModifyWrite;

    static uint32_t *commandReserve(CommandContainer &container);
    static uint32_t *commandReserve(LinearStream &cmdStream);
    static void greaterThan(CommandContainer &container,
                            AluRegisters firstOperandRegister,
                            AluRegisters secondOperandRegister,
                            AluRegisters finalResultRegister);
    static void addition(CommandContainer &container,
                         AluRegisters firstOperandRegister,
                         AluRegisters secondOperandRegister,
                         AluRegisters finalResultRegister);
    static void addition(LinearStream &cmdStream,
                         AluRegisters firstOperandRegister,
                         AluRegisters secondOperandRegister,
                         AluRegisters finalResultRegister);
    static void bitwiseAnd(CommandContainer &container,
                           AluRegisters firstOperandRegister,
                           AluRegisters secondOperandRegister,
                           AluRegisters finalResultRegister);
};

template <typename GfxFamily>
struct EncodeMiPredicate {
    static void encode(LinearStream &cmdStream, MiPredicateType predicateType);

    static constexpr size_t getCmdSize() {
        if constexpr (GfxFamily::isUsingMiSetPredicate) {
            return sizeof(typename GfxFamily::MI_SET_PREDICATE);
        } else {
            return 0;
        }
    }
};

template <typename GfxFamily>
struct EncodeMathMMIO {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename GfxFamily::MI_MATH;

    static const size_t size = sizeof(MI_STORE_REGISTER_MEM);

    static void encodeMulRegVal(CommandContainer &container, uint32_t offset, uint32_t val, uint64_t dstAddress, bool isBcs);

    static void encodeGreaterThanPredicate(CommandContainer &container, uint64_t lhsVal, uint32_t rhsVal, bool isBcs);

    static void encodeBitwiseAndVal(CommandContainer &container,
                                    uint32_t regOffset,
                                    uint32_t immVal,
                                    uint64_t dstAddress,
                                    bool workloadPartition,
                                    void **outCmdBuffer,
                                    bool isBcs);

    static void encodeAlu(MI_MATH_ALU_INST_INLINE *pAluParam, AluRegisters srcA, AluRegisters srcB, AluRegisters op, AluRegisters dest, AluRegisters result);

    static void encodeAluSubStoreCarry(MI_MATH_ALU_INST_INLINE *pAluParam, AluRegisters regA, AluRegisters regB, AluRegisters finalResultRegister);

    static void encodeAluAdd(MI_MATH_ALU_INST_INLINE *pAluParam,
                             AluRegisters firstOperandRegister,
                             AluRegisters secondOperandRegister,
                             AluRegisters finalResultRegister);

    static void encodeAluAnd(MI_MATH_ALU_INST_INLINE *pAluParam,
                             AluRegisters firstOperandRegister,
                             AluRegisters secondOperandRegister,
                             AluRegisters finalResultRegister);

    static void encodeIncrement(LinearStream &cmdStream, AluRegisters operandRegister, bool isBcs);
    static void encodeDecrement(LinearStream &cmdStream, AluRegisters operandRegister, bool isBcs);
    static constexpr size_t getCmdSizeForIncrementOrDecrement() {
        return (EncodeAluHelper<GfxFamily, 4>::getCmdsSize() + (2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM)));
    }

  protected:
    enum class IncrementOrDecrementOperation {
        increment = 0,
        decrement = 1,
    };

    static void encodeIncrementOrDecrement(LinearStream &cmdStream, AluRegisters operandRegister, IncrementOrDecrementOperation operationType, bool isBcs);
};

template <typename GfxFamily>
struct EncodeIndirectParams {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename GfxFamily::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_MATH = typename GfxFamily::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;

    static void encode(CommandContainer &container, uint64_t crossThreadDataGpuVa, DispatchKernelEncoderI *dispatchInterface, uint64_t implicitArgsGpuPtr);
    static void setGroupCountIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress);
    static void setWorkDimIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offset, uint64_t crossThreadAddress, const uint32_t *groupSize);
    static void setGlobalWorkSizeIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress, const uint32_t *lws);

    static size_t getCmdsSizeForSetWorkDimIndirect(const uint32_t *groupSize, bool misalignedPtr);
};

template <typename GfxFamily>
struct EncodeSetMMIO {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename GfxFamily::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;

    static const size_t sizeIMM = sizeof(MI_LOAD_REGISTER_IMM);
    static const size_t sizeMEM = sizeof(MI_LOAD_REGISTER_MEM);
    static const size_t sizeREG = sizeof(MI_LOAD_REGISTER_REG);

    static void encodeIMM(CommandContainer &container, uint32_t offset, uint32_t data, bool remap, bool isBcs);
    static void encodeMEM(CommandContainer &container, uint32_t offset, uint64_t address, bool isBcs);
    static void encodeREG(CommandContainer &container, uint32_t dstOffset, uint32_t srcOffset, bool isBcs);

    static void encodeIMM(LinearStream &cmdStream, uint32_t offset, uint32_t data, bool remap, bool isBcs);
    static void encodeMEM(LinearStream &cmdStream, uint32_t offset, uint64_t address, bool isBcs);
    static void encodeREG(LinearStream &cmdStream, uint32_t dstOffset, uint32_t srcOffset, bool isBcs);

    static bool isRemapApplicable(uint32_t offset);
    static void remapOffset(MI_LOAD_REGISTER_MEM *pMiLoadReg);
    static void remapOffset(MI_LOAD_REGISTER_REG *pMiLoadReg);
};

template <typename GfxFamily>
struct EncodeL3State {
    static void encode(CommandContainer &container, bool enableSLM);
};

template <typename GfxFamily>
struct EncodeMediaInterfaceDescriptorLoad {
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;

    static void encode(CommandContainer &container, IndirectHeap *childDsh);
};

template <typename GfxFamily>
struct EncodeStateBaseAddressArgs {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    CommandContainer *container = nullptr;
    STATE_BASE_ADDRESS &sbaCmd;
    StateBaseAddressProperties *sbaProperties = nullptr;

    uint32_t statelessMocsIndex = 0;
    uint32_t l1CachePolicy = 0;
    uint32_t l1CachePolicyDebuggerActive = 0;

    bool multiOsContextCapable = false;
    bool isRcs = false;
    bool doubleSbaWa = false;
    bool heaplessModeEnabled = false;
};

template <typename GfxFamily>
struct EncodeStateBaseAddress {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    static void encode(EncodeStateBaseAddressArgs<GfxFamily> &args);
    static size_t getRequiredSizeForStateBaseAddress(Device &device, CommandContainer &container, bool isRcs);
    static void setSbaTrackingForL0DebuggerIfEnabled(bool trackingEnabled,
                                                     Device &device,
                                                     LinearStream &commandStream,
                                                     STATE_BASE_ADDRESS &sbaCmd, bool useFirstLevelBB);

  protected:
    static void setSbaAddressesForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd);
};

template <typename GfxFamily>
struct EncodeStoreMMIO {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    static const size_t size = sizeof(MI_STORE_REGISTER_MEM);
    static void encode(LinearStream &csr, uint32_t offset, uint64_t address, bool workloadPartition, void **outCmdBuffer, bool isBcs);
    static void encode(MI_STORE_REGISTER_MEM *cmdBuffer, uint32_t offset, uint64_t address, bool workloadPartition, bool isBcs);
    static void appendFlags(MI_STORE_REGISTER_MEM *storeRegMem, bool workloadPartition);
};

template <typename GfxFamily>
struct EncodeComputeMode {
    static size_t getCmdSizeForComputeMode(const RootDeviceEnvironment &rootDeviceEnvironment, bool hasSharedHandles, bool isRcs);
    static void programComputeModeCommandWithSynchronization(LinearStream &csr, StateComputeModeProperties &properties,
                                                             const PipelineSelectArgs &args, bool hasSharedHandles,
                                                             const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlush);
    static void programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment);
    static void adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor);
    static size_t getSizeForComputeMode();
};

template <typename GfxFamily>
struct EncodeSemaphore {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    static constexpr uint32_t invalidHardwareTag = -2;

    static void programMiSemaphoreWait(MI_SEMAPHORE_WAIT *cmd,
                                       uint64_t compareAddress,
                                       uint64_t compareData,
                                       COMPARE_OPERATION compareMode,
                                       bool registerPollMode,
                                       bool waitMode,
                                       bool useQwordData,
                                       bool indirect,
                                       bool switchOnUnsuccessful);

    static void addMiSemaphoreWaitCommand(LinearStream &commandStream,
                                          uint64_t compareAddress,
                                          uint64_t compareData,
                                          COMPARE_OPERATION compareMode,
                                          bool registerPollMode,
                                          bool useQwordData,
                                          bool indirect,
                                          bool switchOnUnsuccessful,
                                          void **outSemWaitCmd);

    static void applyMiSemaphoreWaitCommand(LinearStream &commandStream,
                                            std::list<void *> &commandsList);

    static constexpr size_t getSizeMiSemaphoreWait() { return sizeof(MI_SEMAPHORE_WAIT); }

  protected:
    static void appendSemaphoreCommand(MI_SEMAPHORE_WAIT &cmd, uint64_t compareData, bool indirect, bool useQwordData, bool switchOnUnsuccessful);
};

template <typename GfxFamily>
struct EncodeAtomic {
    using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
    using ATOMIC_OPCODES = typename GfxFamily::MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename GfxFamily::MI_ATOMIC::DATA_SIZE;

    static void programMiAtomic(LinearStream &commandStream,
                                uint64_t writeAddress,
                                ATOMIC_OPCODES opcode,
                                DATA_SIZE dataSize,
                                uint32_t returnDataControl,
                                uint32_t csStall,
                                uint64_t operand1Data,
                                uint64_t operand2Data);

    static void programMiAtomic(MI_ATOMIC *atomic,
                                uint64_t writeAddress,
                                ATOMIC_OPCODES opcode,
                                DATA_SIZE dataSize,
                                uint32_t returnDataControl,
                                uint32_t csStall,
                                uint64_t operand1Data,
                                uint64_t operand2Data);

    static void setMiAtomicAddress(MI_ATOMIC &atomic, uint64_t writeAddress);
};

template <typename GfxFamily>
struct EncodeBatchBufferStartOrEnd {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    static constexpr size_t getBatchBufferStartSize() {
        return sizeof(MI_BATCH_BUFFER_START);
    }

    static constexpr size_t getBatchBufferEndSize() {
        return sizeof(MI_BATCH_BUFFER_END);
    }

    static void programBatchBufferStart(MI_BATCH_BUFFER_START *cmdBuffer, uint64_t address, bool secondLevel, bool indirect, bool predicate);
    static void programBatchBufferStart(LinearStream *commandStream, uint64_t address, bool secondLevel, bool indirect, bool predicate);
    static void programBatchBufferEnd(CommandContainer &container);
    static void programBatchBufferEnd(LinearStream &commandStream);

    static void programConditionalDataMemBatchBufferStart(LinearStream &commandStream, uint64_t startAddress, uint64_t compareAddress, uint64_t compareData, CompareOperation compareOperation, bool indirect, bool useQwordData, bool isBcs);
    static void programConditionalDataRegBatchBufferStart(LinearStream &commandStream, uint64_t startAddress, uint32_t compareReg, uint64_t compareData, CompareOperation compareOperation, bool indirect, bool useQwordData, bool isBcs);
    static void programConditionalRegRegBatchBufferStart(LinearStream &commandStream, uint64_t startAddress, AluRegisters compareReg0, AluRegisters compareReg1, CompareOperation compareOperation, bool indirect, bool isBcs);
    static void programConditionalRegMemBatchBufferStart(LinearStream &commandStream, uint64_t startAddress, uint64_t compareAddress, uint32_t compareReg, CompareOperation compareOperation, bool indirect, bool isBcs);

    static size_t constexpr getCmdSizeConditionalDataMemBatchBufferStart(bool useQwordData) {
        size_t size = (getCmdSizeConditionalBufferStartBase() + sizeof(typename GfxFamily::MI_LOAD_REGISTER_MEM) + (2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM)));
        size += useQwordData ? sizeof(typename GfxFamily::MI_LOAD_REGISTER_MEM) : sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);

        return size;
    }

    static size_t constexpr getCmdSizeConditionalDataRegBatchBufferStart(bool useQwordData) {
        size_t size = (getCmdSizeConditionalBufferStartBase() + sizeof(typename GfxFamily::MI_LOAD_REGISTER_REG) + (2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM)));
        size += useQwordData ? sizeof(typename GfxFamily::MI_LOAD_REGISTER_REG) : sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);

        return size;
    }

    static size_t constexpr getCmdSizeConditionalRegMemBatchBufferStart() {
        return (getCmdSizeConditionalBufferStartBase() + +sizeof(typename GfxFamily::MI_LOAD_REGISTER_MEM) + sizeof(typename GfxFamily::MI_LOAD_REGISTER_REG) + (2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM)));
    }

    static size_t constexpr getCmdSizeConditionalRegRegBatchBufferStart() {
        return getCmdSizeConditionalBufferStartBase();
    }

  protected:
    static void appendBatchBufferStart(MI_BATCH_BUFFER_START &cmd, bool indirect, bool predicate);
    static void programConditionalBatchBufferStartBase(LinearStream &commandStream, uint64_t startAddress, AluRegisters regA, AluRegisters regB, CompareOperation compareOperation, bool indirect, bool isBcs);

    static size_t constexpr getCmdSizeConditionalBufferStartBase() {
        return (EncodeAluHelper<GfxFamily, 4>::getCmdsSize() + sizeof(typename GfxFamily::MI_LOAD_REGISTER_REG) +
                (2 * EncodeMiPredicate<GfxFamily>::getCmdSize()) + sizeof(MI_BATCH_BUFFER_START));
    }
};

template <typename GfxFamily>
struct EncodeMiFlushDW {
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;
    static void programWithWa(LinearStream &commandStream, uint64_t immediateDataGpuAddress, uint64_t immediateData,
                              MiFlushArgs &args);

    static size_t getCommandSizeWithWa(const EncodeDummyBlitWaArgs &waArgs);

  protected:
    static size_t getWaSize(const EncodeDummyBlitWaArgs &waArgs);
    static void appendWa(LinearStream &commandStream, MiFlushArgs &args);
    static void adjust(MI_FLUSH_DW *miFlushDwCmd, const ProductHelper &productHelper);
};

template <typename GfxFamily>
struct EncodeMemoryPrefetch {
    static void programMemoryPrefetch(LinearStream &commandStream, const GraphicsAllocation &graphicsAllocation, uint32_t size, size_t offset, const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeForMemoryPrefetch(size_t size, const RootDeviceEnvironment &rootDeviceEnvironment);
};

template <typename GfxFamily>
struct EncodeMiArbCheck {
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;

    static void program(LinearStream &commandStream, std::optional<bool> preParserDisable);
    static size_t getCommandSize();

  protected:
    static void adjust(MI_ARB_CHECK &miArbCheck, std::optional<bool> preParserDisable);
};

template <typename GfxFamily>
struct EncodeWA {
    static void encodeAdditionalPipelineSelect(LinearStream &stream, const PipelineSelectArgs &args, bool is3DPipeline,
                                               const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs);
    static size_t getAdditionalPipelineSelectSize(Device &device, bool isRcs);

    static void addPipeControlPriorToNonPipelinedStateCommand(LinearStream &commandStream, PipeControlArgs args,
                                                              const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs);
    static void setAdditionalPipeControlFlagsForNonPipelineStateCommand(PipeControlArgs &args);

    static void addPipeControlBeforeStateBaseAddress(LinearStream &commandStream, const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlushRequired);

    static void adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, int plane);
};

template <typename GfxFamily>
struct EncodeEnableRayTracing {
    static void programEnableRayTracing(LinearStream &commandStream, uint64_t backBuffer);
    static void append3dStateBtd(void *ptr3dStateBtd);
    static bool is48bResourceNeededForRayTracing();
};

template <typename GfxFamily>
struct EncodeNoop {
    static void alignToCacheLine(LinearStream &commandStream);
    static void emitNoop(LinearStream &commandStream, size_t bytesToUpdate);
};

template <typename GfxFamily>
struct EncodeStoreMemory {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    static void programStoreDataImm(LinearStream &commandStream,
                                    uint64_t gpuAddress,
                                    uint32_t dataDword0,
                                    uint32_t dataDword1,
                                    bool storeQword,
                                    bool workloadPartitionOffset,
                                    void **outCmdPtr);

    static void programStoreDataImm(MI_STORE_DATA_IMM *cmdBuffer,
                                    uint64_t gpuAddress,
                                    uint32_t dataDword0,
                                    uint32_t dataDword1,
                                    bool storeQword,
                                    bool workloadPartitionOffset);

    static size_t getStoreDataImmSize() {
        return sizeof(MI_STORE_DATA_IMM);
    }

  protected:
    static void encodeForceCompletionCheck(MI_STORE_DATA_IMM &storeDataImmCmd);
};

template <typename GfxFamily>
struct EncodeMemoryFence {
    static size_t getSystemMemoryFenceSize();

    static void encodeSystemMemoryFence(LinearStream &commandStream, const GraphicsAllocation *globalFenceAllocation);
};

template <typename GfxFamily>
struct EnodeUserInterrupt {
    static void encode(LinearStream &commandStream);
};

} // namespace NEO
