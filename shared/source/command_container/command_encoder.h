/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"

#include <algorithm>

namespace NEO {

template <typename GfxFamily>
struct EncodeDispatchKernel {
    using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

    static void encode(CommandContainer &container,
                       const void *pThreadGroupDimensions, bool isIndirect, bool isPredicate, DispatchKernelEncoderI *dispatchInterface, uint64_t eventAddress, Device *device, PreemptionMode preemptionMode);

    static void *getInterfaceDescriptor(CommandContainer &container, uint32_t &iddOffset);

    static size_t estimateEncodeDispatchKernelCmdsSize(Device *device);
};

template <typename GfxFamily>
struct EncodeStates {
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;

    static const uint32_t alignIndirectStatePointer = MemoryConstants::cacheLineSize;

    static uint32_t copySamplerState(IndirectHeap *dsh,
                                     uint32_t samplerStateOffset,
                                     uint32_t samplerCount,
                                     uint32_t borderColorOffset,
                                     const void *fnDynamicStateHeap);

    static void adjustStateComputeMode(LinearStream &csr, uint32_t numGrfRequired, void *const stateComputeModePtr, bool isMultiOsContextCapable, bool requiresCoherency);

    static size_t getAdjustStateComputeModeSize();
};

template <typename GfxFamily>
struct EncodeMath {
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_MATH = typename GfxFamily::MI_MATH;

    static uint32_t *commandReserve(CommandContainer &container);
    static void greaterThan(CommandContainer &container,
                            AluRegisters firstOperandRegister,
                            AluRegisters secondOperandRegister,
                            AluRegisters finalResultRegister);
    static void addition(CommandContainer &container,
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

    static void encodeAlu(MI_MATH_ALU_INST_INLINE *pAluParam, AluRegisters srcA, AluRegisters srcB, AluRegisters op, AluRegisters dest, AluRegisters result);

    static void encodeAluSubStoreCarry(MI_MATH_ALU_INST_INLINE *pAluParam, AluRegisters regA, AluRegisters regB, AluRegisters finalResultRegister);

    static void encodeAluAdd(MI_MATH_ALU_INST_INLINE *pAluParam,
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
    static void setGroupCountIndirect(CommandContainer &container, uint32_t offsets[3], void *crossThreadAddress);

    static void setGroupSizeIndirect(CommandContainer &container, uint32_t offsets[3], void *crossThreadAddress, uint32_t lws[3]);

    static size_t getCmdsSizeForIndirectParams();
    static size_t getCmdsSizeForSetGroupSizeIndirect();
    static size_t getCmdsSizeForSetGroupCountIndirect();
};

template <typename GfxFamily>
struct EncodeSetMMIO {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename GfxFamily::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;

    static const size_t sizeIMM = sizeof(MI_LOAD_REGISTER_IMM);
    static const size_t sizeMEM = sizeof(MI_LOAD_REGISTER_MEM);
    static const size_t sizeREG = sizeof(MI_LOAD_REGISTER_REG);

    static void encodeIMM(CommandContainer &container, uint32_t offset, uint32_t data);

    static void encodeMEM(CommandContainer &container, uint32_t offset, uint64_t address);

    static void encodeREG(CommandContainer &container, uint32_t dstOffset, uint32_t srcOffset);
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
struct EncodeStateBaseAddress {
    static void encode(CommandContainer &container);
};

template <typename GfxFamily>
struct EncodeStoreMMIO {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    static const size_t size = sizeof(MI_STORE_REGISTER_MEM);
    static void encode(CommandContainer &container, uint32_t offset, uint64_t address);
};

template <typename GfxFamily>
struct EncodeSurfaceState {
    using R_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename R_SURFACE_STATE::SURFACE_FORMAT;
    using AUXILIARY_SURFACE_MODE = typename R_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    static void encodeBuffer(void *dst, void *address, size_t size, uint32_t mocs,
                             bool cpuCoherent);

    static constexpr uintptr_t getSurfaceBaseAddressAlignmentMask() {
        return ~(getSurfaceBaseAddressAlignment() - 1);
    }

    static constexpr uintptr_t getSurfaceBaseAddressAlignment() { return 4; }

    static void getSshAlignedPointer(uintptr_t &ptr, size_t &offset);
};

template <typename GfxFamily>
struct EncodeComputeMode {
    using STATE_COMPUTE_MODE = typename GfxFamily::STATE_COMPUTE_MODE;
    static void adjustComputeMode(LinearStream &csr, uint32_t numGrfRequired, void *const stateComputeModePtr, bool isMultiOsContextCapable);

    static void adjustPipelineSelect(CommandContainer &container, uint32_t numGrfRequired);
};

template <typename GfxFamily>
struct EncodeWA {
    static void encodeAdditionalPipelineSelect(Device &device, LinearStream &stream, bool is3DPipeline);
    static size_t getAdditionalPipelineSelectSize(Device &device);
};

template <typename GfxFamily>
struct EncodeSempahore {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    static constexpr uint32_t invalidHardwareTag = -2;

    static void programMiSemaphoreWait(MI_SEMAPHORE_WAIT *cmd,
                                       uint64_t compareAddress,
                                       uint32_t compareData,
                                       COMPARE_OPERATION compareMode);

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

    static void programMiAtomic(MI_ATOMIC *atomic, uint64_t writeAddress,
                                ATOMIC_OPCODES opcode,
                                DATA_SIZE dataSize);
};

template <typename GfxFamily>
struct EncodeBatchBufferStartOrEnd {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    static void programBatchBufferStart(LinearStream *commandStream,
                                        uint64_t address,
                                        bool secondLevel);
    static void programBatchBufferEnd(CommandContainer &container);
};

template <typename GfxFamily>
struct EncodeMiFlushDW {
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;
    static void programMiFlushDw(LinearStream &commandStream, uint64_t immediateDataGpuAddress, uint64_t immediateData);
    static void programMiFlushDwWA(LinearStream &commandStream);
    static void appendMiFlushDw(MI_FLUSH_DW *miFlushDwCmd);
    static size_t getMiFlushDwCmdSizeForDataWrite();
    static size_t getMiFlushDwWaSize();
};
} // namespace NEO
