/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/definitions/mi_flush_args.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"

#include "encode_surface_state_args.h"

#include <list>

namespace NEO {

class BindlessHeapsHelper;
class GmmHelper;
class LogicalStateHelper;
class IndirectHeap;
class LogicalStateHelper;
class Gmm;
struct HardwareInfo;
struct StateComputeModeProperties;

struct EncodeDispatchKernelArgs {
    uint64_t eventAddress = 0ull;
    Device *device = nullptr;
    DispatchKernelEncoderI *dispatchInterface = nullptr;
    const void *threadGroupDimensions = nullptr;
    std::list<void *> *additionalCommands = nullptr;
    PreemptionMode preemptionMode = PreemptionMode::Initial;
    uint32_t partitionCount = 0u;
    bool isIndirect = false;
    bool isPredicate = false;
    bool isTimestampEvent = false;
    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = false;
    bool isInternal = false;
    bool isCooperative = false;
    bool isHostScopeSignalEvent = false;
    bool isKernelUsingSystemAllocation = false;
    bool isKernelDispatchedFromImmediateCmdList = false;
    bool isRcs = false;
};

struct EncodeWalkerArgs {
    KernelExecutionType kernelExecutionType = KernelExecutionType::Default;
    bool requiredSystemFence = false;
};

template <typename GfxFamily>
struct EncodeDispatchKernel {
    using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

    static void encode(CommandContainer &container, EncodeDispatchKernelArgs &args, LogicalStateHelper *logicalStateHelper);

    static void encodeAdditionalWalkerFields(const HardwareInfo &hwInfo, WALKER_TYPE &walkerCmd, const EncodeWalkerArgs &walkerArgs);

    static void appendAdditionalIDDFields(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const HardwareInfo &hwInfo,
                                          const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy);

    static void setGrfInfo(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, uint32_t numGrf, const size_t &sizeCrossThreadData,
                           const size_t &sizePerThreadData, const HardwareInfo &hwInfo);

    static void *getInterfaceDescriptor(CommandContainer &container, uint32_t &iddOffset);

    static bool isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                                    const size_t *lws,
                                                    std::array<uint8_t, 3> walkOrder,
                                                    bool requireInputWalkOrder,
                                                    uint32_t &requiredWalkOrder,
                                                    uint32_t simd);

    static bool inlineDataProgrammingRequired(const KernelDescriptor &kernelDesc);

    static void encodeThreadData(WALKER_TYPE &walkerCmd,
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
                                 const HardwareInfo &hwInfo);

    static void programBarrierEnable(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo);

    static void adjustInterfaceDescriptorData(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const HardwareInfo &hwInfo, const uint32_t threadGroupCount, const uint32_t numGrf);

    static void adjustBindingTablePrefetch(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t samplerCount, uint32_t bindingTableEntryCount);

    static void adjustTimestampPacket(WALKER_TYPE &walkerCmd, const HardwareInfo &hwInfo);

    static void setupPostSyncMocs(WALKER_TYPE &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment);

    static void adjustWalkOrder(WALKER_TYPE &walkerCmd, uint32_t requiredWorkGroupOrder, const HardwareInfo &hwInfo);

    static constexpr bool shouldUpdateGlobalAtomics(bool &currentVal, bool refVal, bool updateCurrent);
};

template <typename GfxFamily>
struct EncodeStates {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
    using SAMPLER_BORDER_COLOR_STATE = typename GfxFamily::SAMPLER_BORDER_COLOR_STATE;

    static const uint32_t alignIndirectStatePointer = MemoryConstants::cacheLineSize;
    static const size_t alignInterfaceDescriptorData = MemoryConstants::cacheLineSize;

    static uint32_t copySamplerState(IndirectHeap *dsh,
                                     uint32_t samplerStateOffset,
                                     uint32_t samplerCount,
                                     uint32_t borderColorOffset,
                                     const void *fnDynamicStateHeap,
                                     BindlessHeapsHelper *bindlessHeapHelper,
                                     const HardwareInfo &hwInfo);
};

template <typename GfxFamily>
struct EncodeMath {
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename GfxFamily::MI_MATH;
    constexpr static size_t streamCommandSize = sizeof(MI_MATH) + sizeof(MI_MATH_ALU_INST_INLINE) * NUM_ALU_INST_FOR_READ_MODIFY_WRITE;

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
struct EncodeMathMMIO {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename GfxFamily::MI_MATH;

    static const size_t size = sizeof(MI_STORE_REGISTER_MEM);

    static void encodeMulRegVal(CommandContainer &container, uint32_t offset, uint32_t val, uint64_t dstAddress);

    static void encodeGreaterThanPredicate(CommandContainer &container, uint64_t lhsVal, uint32_t rhsVal);

    static void encodeBitwiseAndVal(CommandContainer &container,
                                    uint32_t regOffset,
                                    uint32_t immVal,
                                    uint64_t dstAddress,
                                    bool workloadPartition);

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

    static void encodeIMM(CommandContainer &container, uint32_t offset, uint32_t data, bool remap);
    static void encodeMEM(CommandContainer &container, uint32_t offset, uint64_t address);
    static void encodeREG(CommandContainer &container, uint32_t dstOffset, uint32_t srcOffset);

    static void encodeIMM(LinearStream &cmdStream, uint32_t offset, uint32_t data, bool remap);
    static void encodeMEM(LinearStream &cmdStream, uint32_t offset, uint64_t address);
    static void encodeREG(LinearStream &cmdStream, uint32_t dstOffset, uint32_t srcOffset);

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

    static void encode(CommandContainer &container);
};

template <typename GfxFamily>
struct EncodeStateBaseAddressArgs {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    CommandContainer *container = nullptr;
    STATE_BASE_ADDRESS &sbaCmd;

    uint32_t statelessMocsIndex = 0;

    bool useGlobalAtomics = false;
    bool multiOsContextCapable = false;
    bool isRcs = false;
};

template <typename GfxFamily>
struct EncodeStateBaseAddress {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    static void encode(EncodeStateBaseAddressArgs<GfxFamily> &args);
    static size_t getRequiredSizeForStateBaseAddress(Device &device, CommandContainer &container, bool isRcs);
    static void setSbaTrackingForL0DebuggerIfEnabled(bool trackingEnabled,
                                                     Device &device,
                                                     LinearStream &commandStream,
                                                     STATE_BASE_ADDRESS &sbaCmd);

  protected:
    static void setSbaAddressesForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd);
};

template <typename GfxFamily>
struct EncodeStoreMMIO {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    static const size_t size = sizeof(MI_STORE_REGISTER_MEM);
    static void encode(LinearStream &csr, uint32_t offset, uint64_t address, bool workloadPartition);
    static void encode(MI_STORE_REGISTER_MEM *cmdBuffer, uint32_t offset, uint64_t address, bool workloadPartition);
    static void appendFlags(MI_STORE_REGISTER_MEM *storeRegMem, bool workloadPartition);
};

template <typename GfxFamily>
struct EncodeSurfaceState {
    using R_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename R_SURFACE_STATE::SURFACE_FORMAT;
    using AUXILIARY_SURFACE_MODE = typename R_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    using COHERENCY_TYPE = typename R_SURFACE_STATE::COHERENCY_TYPE;

    static void encodeBuffer(EncodeSurfaceStateArgs &args);
    static void encodeExtraBufferParams(EncodeSurfaceStateArgs &args);
    static void encodeImplicitScalingParams(const EncodeSurfaceStateArgs &args);
    static void encodeExtraCacheSettings(R_SURFACE_STATE *surfaceState, const EncodeSurfaceStateArgs &args);
    static void appendBufferSurfaceState(EncodeSurfaceStateArgs &args);

    static constexpr uintptr_t getSurfaceBaseAddressAlignmentMask() {
        return ~(getSurfaceBaseAddressAlignment() - 1);
    }

    static constexpr uintptr_t getSurfaceBaseAddressAlignment() { return 4; }

    static void getSshAlignedPointer(uintptr_t &ptr, size_t &offset);
    static bool doBindingTablePrefetch();
    static bool isBindingTablePrefetchPreferred();

    static size_t pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, size_t bindingTableCount,
                                                   const void *srcKernelSsh, size_t srcKernelSshSize,
                                                   size_t numberOfBindingTableStates, size_t offsetOfBindingTable);

    static void appendImageCompressionParams(R_SURFACE_STATE *surfaceState, GraphicsAllocation *allocation, GmmHelper *gmmHelper,
                                             bool imageFromBuffer, GMM_YUV_PLANE_ENUM plane);
    static void setCoherencyType(R_SURFACE_STATE *surfaceState, COHERENCY_TYPE coherencyType);
    static void setBufferAuxParamsForCCS(R_SURFACE_STATE *surfaceState);
    static void setImageAuxParamsForCCS(R_SURFACE_STATE *surfaceState, Gmm *gmm);
    static bool isAuxModeEnabled(R_SURFACE_STATE *surfaceState, Gmm *gmm);
    static void setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState);
    static void setClearColorParams(R_SURFACE_STATE *surfaceState, Gmm *gmm);
    static void setFlagsForMediaCompression(R_SURFACE_STATE *surfaceState, Gmm *gmm);
    static void disableCompressionFlags(R_SURFACE_STATE *surfaceState);
    static void appendParamsForImageFromBuffer(R_SURFACE_STATE *surfaceState);
};

template <typename GfxFamily>
struct EncodeComputeMode {
    static size_t getCmdSizeForComputeMode(const HardwareInfo &hwInfo, bool hasSharedHandles, bool isRcs);
    static void programComputeModeCommandWithSynchronization(LinearStream &csr, StateComputeModeProperties &properties,
                                                             const PipelineSelectArgs &args, bool hasSharedHandles,
                                                             const HardwareInfo &hwInfo, bool isRcs, LogicalStateHelper *logicalStateHelper);
    static void programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const HardwareInfo &hwInfo, LogicalStateHelper *logicalStateHelper);

    static void adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor);
};

template <typename GfxFamily>
struct EncodeWA {
    static void encodeAdditionalPipelineSelect(LinearStream &stream, const PipelineSelectArgs &args, bool is3DPipeline,
                                               const HardwareInfo &hwInfo, bool isRcs);
    static size_t getAdditionalPipelineSelectSize(Device &device, bool isRcs);

    static void addPipeControlPriorToNonPipelinedStateCommand(LinearStream &commandStream, PipeControlArgs args,
                                                              const HardwareInfo &hwInfo, bool isRcs);
    static void setAdditionalPipeControlFlagsForNonPipelineStateCommand(PipeControlArgs &args);

    static void addPipeControlBeforeStateBaseAddress(LinearStream &commandStream, const HardwareInfo &hwInfo, bool isRcs);

    static void adjustCompressionFormatForPlanarImage(uint32_t &compressionFormat, GMM_YUV_PLANE_ENUM plane);
};

template <typename GfxFamily>
struct EncodeSempahore {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    static constexpr uint32_t invalidHardwareTag = -2;

    static void programMiSemaphoreWait(MI_SEMAPHORE_WAIT *cmd,
                                       uint64_t compareAddress,
                                       uint32_t compareData,
                                       COMPARE_OPERATION compareMode,
                                       bool registerPollMode);

    static void addMiSemaphoreWaitCommand(LinearStream &commandStream,
                                          uint64_t compareAddress,
                                          uint32_t compareData,
                                          COMPARE_OPERATION compareMode,
                                          bool registerPollMode);

    static void addMiSemaphoreWaitCommand(LinearStream &commandStream,
                                          uint64_t compareAddress,
                                          uint32_t compareData,
                                          COMPARE_OPERATION compareMode);

    static size_t getSizeMiSemaphoreWait();
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
                                uint32_t operand1dword0,
                                uint32_t operand1dword1);

    static void programMiAtomic(MI_ATOMIC *atomic,
                                uint64_t writeAddress,
                                ATOMIC_OPCODES opcode,
                                DATA_SIZE dataSize,
                                uint32_t returnDataControl,
                                uint32_t csStall,
                                uint32_t operand1dword0,
                                uint32_t operand1dword1);

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

    static void programBatchBufferStart(LinearStream *commandStream,
                                        uint64_t address,
                                        bool secondLevel);
    static void programBatchBufferEnd(CommandContainer &container);
    static void programBatchBufferEnd(LinearStream &commandStream);
};

template <typename GfxFamily>
struct EncodeMiFlushDW {
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;
    static void programMiFlushDw(LinearStream &commandStream, uint64_t immediateDataGpuAddress, uint64_t immediateData,
                                 MiFlushArgs &args, const HardwareInfo &hwInfo);
    static void programMiFlushDwWA(LinearStream &commandStream);
    static void appendMiFlushDw(MI_FLUSH_DW *miFlushDwCmd, const HardwareInfo &hwInfo);
    static size_t getMiFlushDwCmdSizeForDataWrite();
    static size_t getMiFlushDwWaSize();
};

template <typename GfxFamily>
struct EncodeMemoryPrefetch {
    static void programMemoryPrefetch(LinearStream &commandStream, const GraphicsAllocation &graphicsAllocation, uint32_t size, size_t offset, const HardwareInfo &hwInfo);
    static size_t getSizeForMemoryPrefetch(size_t size, const HardwareInfo &hwInfo);
};

template <typename GfxFamily>
struct EncodeMiArbCheck {
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;

    static void program(LinearStream &commandStream);
    static void adjust(MI_ARB_CHECK &miArbCheck);
    static size_t getCommandSize();
};

template <typename GfxFamily>
struct EncodeEnableRayTracing {
    static void programEnableRayTracing(LinearStream &commandStream, uint64_t backBuffer);
    static void append3dStateBtd(void *ptr3dStateBtd);
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
                                    bool workloadPartitionOffset);

    static void programStoreDataImm(MI_STORE_DATA_IMM *cmdBuffer,
                                    uint64_t gpuAddress,
                                    uint32_t dataDword0,
                                    uint32_t dataDword1,
                                    bool storeQword,
                                    bool workloadPartitionOffset);

    static size_t getStoreDataImmSize() {
        return sizeof(MI_STORE_DATA_IMM);
    }
};

template <typename GfxFamily>
struct EncodeMemoryFence {
    static size_t getSystemMemoryFenceSize();

    static void encodeSystemMemoryFence(LinearStream &commandStream, const GraphicsAllocation *globalFenceAllocation, LogicalStateHelper *logicalStateHelper);
};

template <typename GfxFamily>
struct EncodeKernelArgsBuffer {
    static size_t getKernelArgsBufferCmdsSize(const GraphicsAllocation *kernelArgsBufferAllocation, LogicalStateHelper *logicalStateHelper);

    static void encodeKernelArgsBufferCmds(const GraphicsAllocation *kernelArgsBufferAllocation, LogicalStateHelper *logicalStateHelper);
};

} // namespace NEO
