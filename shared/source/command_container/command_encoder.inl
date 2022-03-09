/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "encode_surface_state.inl"

#include <algorithm>

namespace NEO {

template <typename Family>
uint32_t EncodeStates<Family>::copySamplerState(IndirectHeap *dsh,
                                                uint32_t samplerStateOffset,
                                                uint32_t samplerCount,
                                                uint32_t borderColorOffset,
                                                const void *fnDynamicStateHeap,
                                                BindlessHeapsHelper *bindlessHeapHelper,
                                                const HardwareInfo &hwInfo) {
    auto sizeSamplerState = sizeof(SAMPLER_STATE) * samplerCount;
    auto borderColorSize = samplerStateOffset - borderColorOffset;

    SAMPLER_STATE *dstSamplerState = nullptr;
    uint32_t samplerStateOffsetInDsh = 0;

    dsh->align(EncodeStates<Family>::alignIndirectStatePointer);
    uint32_t borderColorOffsetInDsh = 0;
    if (!ApiSpecificConfig::getBindlessConfiguration()) {
        borderColorOffsetInDsh = static_cast<uint32_t>(dsh->getUsed());
        auto borderColor = dsh->getSpace(borderColorSize);

        memcpy_s(borderColor, borderColorSize, ptrOffset(fnDynamicStateHeap, borderColorOffset),
                 borderColorSize);

        dsh->align(INTERFACE_DESCRIPTOR_DATA::SAMPLERSTATEPOINTER_ALIGN_SIZE);
        samplerStateOffsetInDsh = static_cast<uint32_t>(dsh->getUsed());

        dstSamplerState = reinterpret_cast<SAMPLER_STATE *>(dsh->getSpace(sizeSamplerState));
    } else {
        auto borderColor = reinterpret_cast<const SAMPLER_BORDER_COLOR_STATE *>(ptrOffset(fnDynamicStateHeap, borderColorOffset));
        if (borderColor->getBorderColorRed() != 0.0f ||
            borderColor->getBorderColorGreen() != 0.0f ||
            borderColor->getBorderColorBlue() != 0.0f ||
            (borderColor->getBorderColorAlpha() != 0.0f && borderColor->getBorderColorAlpha() != 1.0f)) {
            UNRECOVERABLE_IF(true);
        } else if (borderColor->getBorderColorAlpha() == 0.0f) {
            borderColorOffsetInDsh = bindlessHeapHelper->getDefaultBorderColorOffset();
        } else {
            borderColorOffsetInDsh = bindlessHeapHelper->getAlphaBorderColorOffset();
        }
        dsh->align(INTERFACE_DESCRIPTOR_DATA::SAMPLERSTATEPOINTER_ALIGN_SIZE);
        auto samplerStateInDsh = bindlessHeapHelper->allocateSSInHeap(sizeSamplerState, nullptr, BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH);
        dstSamplerState = reinterpret_cast<SAMPLER_STATE *>(samplerStateInDsh.ssPtr);
        samplerStateOffsetInDsh = static_cast<uint32_t>(samplerStateInDsh.surfaceStateOffset);
    }

    auto srcSamplerState = reinterpret_cast<const SAMPLER_STATE *>(ptrOffset(fnDynamicStateHeap, samplerStateOffset));
    SAMPLER_STATE state = {};
    for (uint32_t i = 0; i < samplerCount; i++) {
        state = srcSamplerState[i];
        state.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));

        HwInfoConfig::get(hwInfo.platform.eProductFamily)->adjustSamplerState(&state, hwInfo);

        dstSamplerState[i] = state;
    }

    return samplerStateOffsetInDsh;
} // namespace NEO

template <typename Family>
void EncodeMathMMIO<Family>::encodeMulRegVal(CommandContainer &container, uint32_t offset, uint32_t val, uint64_t dstAddress) {
    int logLws = 0;
    int i = val;
    while (val >> logLws) {
        logLws++;
    }

    EncodeSetMMIO<Family>::encodeREG(container, CS_GPR_R0, offset);
    EncodeSetMMIO<Family>::encodeIMM(container, CS_GPR_R1, 0, true);

    i = 0;
    while (i < logLws) {
        if (val & (1 << i)) {
            EncodeMath<Family>::addition(container, AluRegisters::R_1,
                                         AluRegisters::R_0, AluRegisters::R_2);
            EncodeSetMMIO<Family>::encodeREG(container, CS_GPR_R1, CS_GPR_R2);
        }
        EncodeMath<Family>::addition(container, AluRegisters::R_0,
                                     AluRegisters::R_0, AluRegisters::R_2);
        EncodeSetMMIO<Family>::encodeREG(container, CS_GPR_R0, CS_GPR_R2);
        i++;
    }
    EncodeStoreMMIO<Family>::encode(*container.getCommandStream(), CS_GPR_R1, dstAddress);
}

/*
 * Compute *firstOperand > secondOperand and store the result in
 * MI_PREDICATE_RESULT where  firstOperand is an device memory address.
 *
 * To calculate the "greater than" operation in the device,
 * (secondOperand - *firstOperand) is used, and if the carry flag register is
 * set, then (*firstOperand) is greater than secondOperand.
 */
template <typename Family>
void EncodeMathMMIO<Family>::encodeGreaterThanPredicate(CommandContainer &container, uint64_t firstOperand, uint32_t secondOperand) {
    EncodeSetMMIO<Family>::encodeMEM(container, CS_GPR_R0, firstOperand);
    EncodeSetMMIO<Family>::encodeIMM(container, CS_GPR_R1, secondOperand, true);

    /* CS_GPR_R* registers map to AluRegisters::R_* registers */
    EncodeMath<Family>::greaterThan(container, AluRegisters::R_0,
                                    AluRegisters::R_1, AluRegisters::R_2);

    EncodeSetMMIO<Family>::encodeREG(container, CS_PREDICATE_RESULT, CS_GPR_R2);
}

/*
 * Compute bitwise AND between a register value from regOffset and immVal
 * and store it into dstAddress.
 */
template <typename Family>
void EncodeMathMMIO<Family>::encodeBitwiseAndVal(CommandContainer &container, uint32_t regOffset, uint32_t immVal, uint64_t dstAddress) {
    EncodeSetMMIO<Family>::encodeREG(container, CS_GPR_R0, regOffset);
    EncodeSetMMIO<Family>::encodeIMM(container, CS_GPR_R1, immVal, true);
    EncodeMath<Family>::bitwiseAnd(container, AluRegisters::R_0,
                                   AluRegisters::R_1,
                                   AluRegisters::R_2);
    EncodeStoreMMIO<Family>::encode(*container.getCommandStream(),
                                    CS_GPR_R2, dstAddress);
}

/*
 * encodeAlu() performs operations that leave a state including the result of
 * an operation such as the carry flag, and the accu flag with subtraction and
 * addition result.
 *
 * Parameter "postOperationStateRegister" is the ALU register with the result
 * from the operation that the function caller is interested in obtaining.
 *
 * Parameter "finalResultRegister" is the final destination register where
 * data from "postOperationStateRegister" will be copied.
 */
template <typename Family>
void EncodeMathMMIO<Family>::encodeAlu(MI_MATH_ALU_INST_INLINE *pAluParam, AluRegisters srcA, AluRegisters srcB, AluRegisters op, AluRegisters finalResultRegister, AluRegisters postOperationStateRegister) {
    MI_MATH_ALU_INST_INLINE aluParam;

    aluParam.DW0.Value = 0x0;
    aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::OPCODE_LOAD);
    aluParam.DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::R_SRCA);
    aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(srcA);
    *pAluParam = aluParam;
    pAluParam++;

    aluParam.DW0.Value = 0x0;
    aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::OPCODE_LOAD);
    aluParam.DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::R_SRCB);
    aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(srcB);
    *pAluParam = aluParam;
    pAluParam++;

    /* Order of operation: Operand1 <ALUOpcode> Operand2 */
    aluParam.DW0.Value = 0x0;
    aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(op);
    aluParam.DW0.BitField.Operand1 = 0;
    aluParam.DW0.BitField.Operand2 = 0;
    *pAluParam = aluParam;
    pAluParam++;

    aluParam.DW0.Value = 0x0;
    aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::OPCODE_STORE);
    aluParam.DW0.BitField.Operand1 = static_cast<uint32_t>(finalResultRegister);
    aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(postOperationStateRegister);
    *pAluParam = aluParam;
    pAluParam++;
}

template <typename Family>
uint32_t *EncodeMath<Family>::commandReserve(CommandContainer &container) {
    size_t size = sizeof(MI_MATH) + sizeof(MI_MATH_ALU_INST_INLINE) * NUM_ALU_INST_FOR_READ_MODIFY_WRITE;

    auto cmd = reinterpret_cast<uint32_t *>(container.getCommandStream()->getSpace(size));
    MI_MATH mathBuffer;
    mathBuffer.DW0.Value = 0x0;
    mathBuffer.DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    mathBuffer.DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    mathBuffer.DW0.BitField.DwordLength = NUM_ALU_INST_FOR_READ_MODIFY_WRITE - 1;
    *reinterpret_cast<MI_MATH *>(cmd) = mathBuffer;
    cmd++;

    return cmd;
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeAluAdd(MI_MATH_ALU_INST_INLINE *pAluParam,
                                          AluRegisters firstOperandRegister,
                                          AluRegisters secondOperandRegister,
                                          AluRegisters finalResultRegister) {
    encodeAlu(pAluParam, firstOperandRegister, secondOperandRegister, AluRegisters::OPCODE_ADD, finalResultRegister, AluRegisters::R_ACCU);
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeAluSubStoreCarry(MI_MATH_ALU_INST_INLINE *pAluParam, AluRegisters regA, AluRegisters regB, AluRegisters finalResultRegister) {
    /* regB is subtracted from regA */
    encodeAlu(pAluParam, regA, regB, AluRegisters::OPCODE_SUB, finalResultRegister, AluRegisters::R_CF);
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeAluAnd(MI_MATH_ALU_INST_INLINE *pAluParam,
                                          AluRegisters firstOperandRegister,
                                          AluRegisters secondOperandRegister,
                                          AluRegisters finalResultRegister) {
    encodeAlu(pAluParam, firstOperandRegister, secondOperandRegister, AluRegisters::OPCODE_AND, finalResultRegister, AluRegisters::R_ACCU);
}

/*
 * greaterThan() tests if firstOperandRegister is greater than
 * secondOperandRegister.
 */
template <typename Family>
void EncodeMath<Family>::greaterThan(CommandContainer &container,
                                     AluRegisters firstOperandRegister,
                                     AluRegisters secondOperandRegister,
                                     AluRegisters finalResultRegister) {
    uint32_t *cmd = EncodeMath<Family>::commandReserve(container);

    /* firstOperandRegister will be subtracted from secondOperandRegister */
    EncodeMathMMIO<Family>::encodeAluSubStoreCarry(reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(cmd),
                                                   secondOperandRegister,
                                                   firstOperandRegister,
                                                   finalResultRegister);
}

template <typename Family>
void EncodeMath<Family>::addition(CommandContainer &container,
                                  AluRegisters firstOperandRegister,
                                  AluRegisters secondOperandRegister,
                                  AluRegisters finalResultRegister) {
    uint32_t *cmd = EncodeMath<Family>::commandReserve(container);

    EncodeMathMMIO<Family>::encodeAluAdd(reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(cmd),
                                         firstOperandRegister,
                                         secondOperandRegister,
                                         finalResultRegister);
}

template <typename Family>
void EncodeMath<Family>::bitwiseAnd(CommandContainer &container,
                                    AluRegisters firstOperandRegister,
                                    AluRegisters secondOperandRegister,
                                    AluRegisters finalResultRegister) {
    uint32_t *cmd = EncodeMath<Family>::commandReserve(container);

    EncodeMathMMIO<Family>::encodeAluAnd(reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(cmd),
                                         firstOperandRegister,
                                         secondOperandRegister,
                                         finalResultRegister);
}

template <typename Family>
inline void EncodeSetMMIO<Family>::encodeIMM(CommandContainer &container, uint32_t offset, uint32_t data, bool remap) {
    EncodeSetMMIO<Family>::encodeIMM(*container.getCommandStream(), offset, data, remap);
}

template <typename Family>
inline void EncodeSetMMIO<Family>::encodeMEM(CommandContainer &container, uint32_t offset, uint64_t address) {
    EncodeSetMMIO<Family>::encodeMEM(*container.getCommandStream(), offset, address);
}

template <typename Family>
inline void EncodeSetMMIO<Family>::encodeREG(CommandContainer &container, uint32_t dstOffset, uint32_t srcOffset) {
    EncodeSetMMIO<Family>::encodeREG(*container.getCommandStream(), dstOffset, srcOffset);
}

template <typename Family>
inline void EncodeSetMMIO<Family>::encodeIMM(LinearStream &cmdStream, uint32_t offset, uint32_t data, bool remap) {
    LriHelper<Family>::program(&cmdStream,
                               offset,
                               data,
                               remap);
}

template <typename Family>
void EncodeSetMMIO<Family>::encodeMEM(LinearStream &cmdStream, uint32_t offset, uint64_t address) {
    MI_LOAD_REGISTER_MEM cmd = Family::cmdInitLoadRegisterMem;
    cmd.setRegisterAddress(offset);
    cmd.setMemoryAddress(address);
    remapOffset(&cmd);

    auto buffer = cmdStream.getSpaceForCmd<MI_LOAD_REGISTER_MEM>();
    *buffer = cmd;
}

template <typename Family>
void EncodeSetMMIO<Family>::encodeREG(LinearStream &cmdStream, uint32_t dstOffset, uint32_t srcOffset) {
    MI_LOAD_REGISTER_REG cmd = Family::cmdInitLoadRegisterReg;
    cmd.setSourceRegisterAddress(srcOffset);
    cmd.setDestinationRegisterAddress(dstOffset);
    remapOffset(&cmd);
    auto buffer = cmdStream.getSpaceForCmd<MI_LOAD_REGISTER_REG>();
    *buffer = cmd;
}

template <typename Family>
void EncodeStoreMMIO<Family>::encode(LinearStream &csr, uint32_t offset, uint64_t address) {
    MI_STORE_REGISTER_MEM cmd = Family::cmdInitStoreRegisterMem;
    cmd.setRegisterAddress(offset);
    cmd.setMemoryAddress(address);
    remapOffset(&cmd);
    auto buffer = csr.getSpaceForCmd<MI_STORE_REGISTER_MEM>();
    *buffer = cmd;
}

template <typename Family>
void EncodeSurfaceState<Family>::encodeBuffer(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    UNRECOVERABLE_IF(!isAligned<getSurfaceBaseAddressAlignment()>(args.size));

    SURFACE_STATE_BUFFER_LENGTH Length = {0};
    Length.Length = static_cast<uint32_t>(args.size - 1);

    surfaceState->setWidth(Length.SurfaceState.Width + 1);
    surfaceState->setHeight(Length.SurfaceState.Height + 1);
    surfaceState->setDepth(Length.SurfaceState.Depth + 1);

    surfaceState->setSurfaceType((args.graphicsAddress != 0) ? R_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER
                                                             : R_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL);
    surfaceState->setSurfaceFormat(SURFACE_FORMAT::SURFACE_FORMAT_RAW);
    surfaceState->setSurfaceVerticalAlignment(R_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);
    surfaceState->setSurfaceHorizontalAlignment(R_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT);

    surfaceState->setTileMode(R_SURFACE_STATE::TILE_MODE_LINEAR);
    surfaceState->setVerticalLineStride(0);
    surfaceState->setVerticalLineStrideOffset(0);
    surfaceState->setMemoryObjectControlState(args.mocs);
    surfaceState->setSurfaceBaseAddress(args.graphicsAddress);

    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);

    setCoherencyType(surfaceState, args.cpuCoherent ? R_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT : R_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);

    auto compressionEnabled = args.allocation ? args.allocation->isCompressionEnabled() : false;
    if (compressionEnabled && !args.forceNonAuxMode) {
        // Its expected to not program pitch/qpitch/baseAddress for Aux surface in CCS scenarios
        setCoherencyType(surfaceState, R_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
        setBufferAuxParamsForCCS(surfaceState);
    }

    if (DebugManager.flags.DisableCachingForStatefulBufferAccess.get()) {
        surfaceState->setMemoryObjectControlState(args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    }

    EncodeSurfaceState<Family>::encodeExtraBufferParams(args);

    EncodeSurfaceState<Family>::appendBufferSurfaceState(args);
}

template <typename Family>
void EncodeSurfaceState<Family>::getSshAlignedPointer(uintptr_t &ptr, size_t &offset) {
    auto sshAlignmentMask =
        getSurfaceBaseAddressAlignmentMask();
    uintptr_t alignedPtr = ptr & sshAlignmentMask;

    offset = 0;
    if (ptr != alignedPtr) {
        offset = ptrDiff(ptr, alignedPtr);
        ptr = alignedPtr;
    }
}

// Returned binding table pointer is relative to given heap (which is assumed to be the Surface state base addess)
// as required by the INTERFACE_DESCRIPTOR_DATA.
template <typename Family>
size_t EncodeSurfaceState<Family>::pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, size_t bindingTableCount,
                                                                    const void *srcKernelSsh, size_t srcKernelSshSize,
                                                                    size_t numberOfBindingTableStates, size_t offsetOfBindingTable) {
    using BINDING_TABLE_STATE = typename Family::BINDING_TABLE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename Family::INTERFACE_DESCRIPTOR_DATA;
    using RENDER_SURFACE_STATE = typename Family::RENDER_SURFACE_STATE;

    if (bindingTableCount == 0) {
        // according to compiler, kernel does not reference BTIs to stateful surfaces, so there's nothing to patch
        return 0;
    }
    size_t sshSize = srcKernelSshSize;
    DEBUG_BREAK_IF(srcKernelSsh == nullptr);

    auto srcSurfaceState = srcKernelSsh;
    // Allocate space for new ssh data
    auto dstSurfaceState = dstHeap.getSpace(sshSize);

    // Compiler sends BTI table that is already populated with surface state pointers relative to local SSH.
    // We may need to patch these pointers so that they are relative to surface state base address
    if (dstSurfaceState == dstHeap.getCpuBase()) {
        // nothing to patch, we're at the start of heap (which is assumed to be the surface state base address)
        // we need to simply copy the ssh (including BTIs from compiler)
        memcpy_s(dstSurfaceState, sshSize, srcSurfaceState, sshSize);
        return offsetOfBindingTable;
    }

    // We can copy-over the surface states, but BTIs will need to be patched
    memcpy_s(dstSurfaceState, sshSize, srcSurfaceState, offsetOfBindingTable);

    uint32_t surfaceStatesOffset = static_cast<uint32_t>(ptrDiff(dstSurfaceState, dstHeap.getCpuBase()));

    // march over BTIs and offset the pointers based on surface state base address
    auto *dstBtiTableBase = reinterpret_cast<BINDING_TABLE_STATE *>(ptrOffset(dstSurfaceState, offsetOfBindingTable));
    DEBUG_BREAK_IF(reinterpret_cast<uintptr_t>(dstBtiTableBase) % INTERFACE_DESCRIPTOR_DATA::BINDINGTABLEPOINTER_ALIGN_SIZE != 0);
    auto *srcBtiTableBase = reinterpret_cast<const BINDING_TABLE_STATE *>(ptrOffset(srcSurfaceState, offsetOfBindingTable));
    BINDING_TABLE_STATE bti = Family::cmdInitBindingTableState;
    for (uint32_t i = 0, e = static_cast<uint32_t>(numberOfBindingTableStates); i != e; ++i) {
        uint32_t localSurfaceStateOffset = srcBtiTableBase[i].getSurfaceStatePointer();
        uint32_t offsetedSurfaceStateOffset = localSurfaceStateOffset + surfaceStatesOffset;
        bti.setSurfaceStatePointer(offsetedSurfaceStateOffset); // patch just the SurfaceStatePointer bits
        dstBtiTableBase[i] = bti;
        DEBUG_BREAK_IF(bti.getRawData(0) % sizeof(BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE) != 0);
    }

    return ptrDiff(dstBtiTableBase, dstHeap.getCpuBase());
}

template <typename Family>
inline void EncodeSurfaceState<Family>::encodeExtraCacheSettings(R_SURFACE_STATE *surfaceState, const HardwareInfo &hwInfo) {}

template <typename Family>
void EncodeSurfaceState<Family>::setImageAuxParamsForCCS(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    using AUXILIARY_SURFACE_MODE = typename Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    // Its expected to not program pitch/qpitch/baseAddress for Aux surface in CCS scenarios
    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    setFlagsForMediaCompression(surfaceState, gmm);

    setClearColorParams(surfaceState, gmm);
    setUnifiedAuxBaseAddress<Family>(surfaceState, gmm);
}

template <typename Family>
void EncodeSurfaceState<Family>::setBufferAuxParamsForCCS(R_SURFACE_STATE *surfaceState) {
    using AUXILIARY_SURFACE_MODE = typename R_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

template <typename Family>
bool EncodeSurfaceState<Family>::isAuxModeEnabled(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    using AUXILIARY_SURFACE_MODE = typename R_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    return (surfaceState->getAuxiliarySurfaceMode() == AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

template <typename Family>
void EncodeSurfaceState<Family>::appendParamsForImageFromBuffer(R_SURFACE_STATE *surfaceState) {
}

template <typename Family>
void EncodeSurfaceState<Family>::encodeImplicitScalingParams(const EncodeSurfaceStateArgs &args) {}

template <typename Family>
void *EncodeDispatchKernel<Family>::getInterfaceDescriptor(CommandContainer &container, uint32_t &iddOffset) {

    if (container.nextIddInBlock == container.getNumIddPerBlock()) {
        if (ApiSpecificConfig::getBindlessConfiguration()) {
            container.getDevice()->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH)->align(EncodeStates<Family>::alignInterfaceDescriptorData);
            container.setIddBlock(container.getDevice()->getBindlessHeapsHelper()->getSpaceInHeap(sizeof(INTERFACE_DESCRIPTOR_DATA) * container.getNumIddPerBlock(), BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH));
        } else {
            container.getIndirectHeap(HeapType::DYNAMIC_STATE)->align(EncodeStates<Family>::alignInterfaceDescriptorData);
            container.setIddBlock(container.getHeapSpaceAllowGrow(HeapType::DYNAMIC_STATE,
                                                                  sizeof(INTERFACE_DESCRIPTOR_DATA) * container.getNumIddPerBlock()));
        }
        container.nextIddInBlock = 0;

        EncodeMediaInterfaceDescriptorLoad<Family>::encode(container);
    }

    iddOffset = container.nextIddInBlock;
    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(container.getIddBlock());
    return &interfaceDescriptorData[container.nextIddInBlock++];
}

template <typename Family>
bool EncodeDispatchKernel<Family>::inlineDataProgrammingRequired(const KernelDescriptor &kernelDesc) {
    auto checkKernelForInlineData = true;
    if (DebugManager.flags.EnablePassInlineData.get() != -1) {
        checkKernelForInlineData = !!DebugManager.flags.EnablePassInlineData.get();
    }
    if (checkKernelForInlineData) {
        return kernelDesc.kernelAttributes.flags.passInlineData;
    }
    return false;
}

template <typename Family>
void EncodeDispatchKernel<Family>::adjustTimestampPacket(WALKER_TYPE &walkerCmd, const HardwareInfo &hwInfo) {}

template <typename Family>
void EncodeIndirectParams<Family>::encode(CommandContainer &container, uint64_t crossThreadDataGpuVa, DispatchKernelEncoderI *dispatchInterface, uint64_t implicitArgsGpuPtr) {
    const auto &kernelDescriptor = dispatchInterface->getKernelDescriptor();
    setGroupCountIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups, crossThreadDataGpuVa);
    setGlobalWorkSizeIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize, crossThreadDataGpuVa, dispatchInterface->getGroupSize());
    UNRECOVERABLE_IF(NEO::isValidOffset(kernelDescriptor.payloadMappings.dispatchTraits.workDim) && (kernelDescriptor.payloadMappings.dispatchTraits.workDim & 0b11) != 0u);
    setWorkDimIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.workDim, crossThreadDataGpuVa, dispatchInterface->getGroupSize());
    if (implicitArgsGpuPtr) {
        CrossThreadDataOffset groupCountOffset[] = {offsetof(ImplicitArgs, groupCountX), offsetof(ImplicitArgs, groupCountY), offsetof(ImplicitArgs, groupCountZ)};
        CrossThreadDataOffset globalSizeOffset[] = {offsetof(ImplicitArgs, globalSizeX), offsetof(ImplicitArgs, globalSizeY), offsetof(ImplicitArgs, globalSizeZ)};
        setGroupCountIndirect(container, groupCountOffset, implicitArgsGpuPtr);
        setGlobalWorkSizeIndirect(container, globalSizeOffset, implicitArgsGpuPtr, dispatchInterface->getGroupSize());
        setWorkDimIndirect(container, offsetof(ImplicitArgs, numWorkDim), implicitArgsGpuPtr, dispatchInterface->getGroupSize());
    }
}

template <typename Family>
void EncodeIndirectParams<Family>::setGroupCountIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress) {
    for (int i = 0; i < 3; ++i) {
        if (NEO::isUndefinedOffset(offsets[i])) {
            continue;
        }
        EncodeStoreMMIO<Family>::encode(*container.getCommandStream(), GPUGPU_DISPATCHDIM[i], ptrOffset(crossThreadAddress, offsets[i]));
    }
}

template <typename Family>
void EncodeIndirectParams<Family>::setWorkDimIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset workDimOffset, uint64_t crossThreadAddress, const uint32_t *groupSize) {
    if (NEO::isValidOffset(workDimOffset)) {
        auto dstPtr = ptrOffset(crossThreadAddress, workDimOffset);
        constexpr uint32_t RESULT_REGISTER = CS_GPR_R0;
        constexpr AluRegisters RESULT_ALU_REGISTER = AluRegisters::R_0;
        const uint32_t offset = static_cast<uint32_t>((1ull << 8 * (dstPtr & 0b11)) - 1);
        const uint32_t memoryMask = std::numeric_limits<uint32_t>::max() - static_cast<uint32_t>((1ull << 8 * ((dstPtr & 0b11) + 1)) - 1) + offset;

        /*
         * if ( groupSize[2] > 1 || groupCount[2] > 1 ) { workdim = 3 }
         * else if ( groupSize[1] + groupCount[1] > 2 ) { workdim = 2 }
         * else { workdim = 1 }
         */

        if (groupSize[2] > 1) {
            EncodeSetMMIO<Family>::encodeIMM(container, RESULT_REGISTER, 3 << (8 * (dstPtr & 0b11)), true);
        } else {

            constexpr uint32_t GROUP_COUNT_2_REGISTER = CS_GPR_R1;
            constexpr AluRegisters GROUP_COUNT_2_ALU_REGISTER = AluRegisters::R_1;

            constexpr uint32_t GROUP_SIZE_1_REGISTER = CS_GPR_R0;
            constexpr AluRegisters GROUP_SIZE_1_ALU_REGISTER = AluRegisters::R_0;

            constexpr uint32_t GROUP_COUNT_1_REGISTER = CS_GPR_R1;
            constexpr AluRegisters GROUP_COUNT_1_ALU_REGISTER = AluRegisters::R_1;

            constexpr AluRegisters SUM_ALU_REGISTER = AluRegisters::R_0;

            constexpr AluRegisters WORK_DIM_EQ_3_ALU_REGISTER = AluRegisters::R_3;

            constexpr AluRegisters WORK_DIM_GE_2_ALU_REGISTER = AluRegisters::R_4;

            constexpr uint32_t CONSTANT_ONE_REGISTER = CS_GPR_R5;
            constexpr AluRegisters CONSTANT_ONE_ALU_REGISTER = AluRegisters::R_5;
            constexpr uint32_t CONSTANT_TWO_REGISTER = CS_GPR_R6;
            constexpr AluRegisters CONSTANT_TWO_ALU_REGISTER = AluRegisters::R_6;

            constexpr uint32_t BACKUP_REGISTER = CS_GPR_R7;
            constexpr AluRegisters BACKUP_ALU_REGISTER = AluRegisters::R_7;

            constexpr uint32_t MEMORY_MASK_REGISTER = CS_GPR_R8;
            constexpr AluRegisters MEMORY_MASK_ALU_REGISTER = AluRegisters::R_8;

            constexpr uint32_t OFFSET_REGISTER = CS_GPR_R8;
            constexpr AluRegisters OFFSET_ALU_REGISTER = AluRegisters::R_8;

            if (offset) {
                EncodeSetMMIO<Family>::encodeMEM(container, BACKUP_REGISTER, dstPtr);
                EncodeSetMMIO<Family>::encodeIMM(container, MEMORY_MASK_REGISTER, memoryMask, true);
                EncodeMath<Family>::bitwiseAnd(container, MEMORY_MASK_ALU_REGISTER, BACKUP_ALU_REGISTER, BACKUP_ALU_REGISTER);
                EncodeSetMMIO<Family>::encodeIMM(container, OFFSET_REGISTER, offset, true);
            }

            EncodeSetMMIO<Family>::encodeIMM(container, CONSTANT_ONE_REGISTER, 1, true);
            EncodeSetMMIO<Family>::encodeIMM(container, CONSTANT_TWO_REGISTER, 2, true);

            EncodeSetMMIO<Family>::encodeREG(container, GROUP_COUNT_2_REGISTER, GPUGPU_DISPATCHDIM[2]);

            EncodeMath<Family>::greaterThan(container, GROUP_COUNT_2_ALU_REGISTER, CONSTANT_ONE_ALU_REGISTER, WORK_DIM_EQ_3_ALU_REGISTER);
            EncodeMath<Family>::bitwiseAnd(container, WORK_DIM_EQ_3_ALU_REGISTER, CONSTANT_ONE_ALU_REGISTER, WORK_DIM_EQ_3_ALU_REGISTER);

            EncodeSetMMIO<Family>::encodeIMM(container, GROUP_SIZE_1_REGISTER, groupSize[1], true);
            EncodeSetMMIO<Family>::encodeREG(container, GROUP_COUNT_1_REGISTER, GPUGPU_DISPATCHDIM[1]);

            EncodeMath<Family>::addition(container, GROUP_SIZE_1_ALU_REGISTER, GROUP_COUNT_1_ALU_REGISTER, SUM_ALU_REGISTER);
            EncodeMath<Family>::addition(container, SUM_ALU_REGISTER, WORK_DIM_EQ_3_ALU_REGISTER, SUM_ALU_REGISTER);
            EncodeMath<Family>::greaterThan(container, SUM_ALU_REGISTER, CONSTANT_TWO_ALU_REGISTER, WORK_DIM_GE_2_ALU_REGISTER);
            EncodeMath<Family>::bitwiseAnd(container, WORK_DIM_GE_2_ALU_REGISTER, CONSTANT_ONE_ALU_REGISTER, WORK_DIM_GE_2_ALU_REGISTER);

            if (offset) {
                EncodeMath<Family>::addition(container, CONSTANT_ONE_ALU_REGISTER, OFFSET_ALU_REGISTER, CONSTANT_ONE_ALU_REGISTER);
                EncodeMath<Family>::addition(container, WORK_DIM_EQ_3_ALU_REGISTER, OFFSET_ALU_REGISTER, WORK_DIM_EQ_3_ALU_REGISTER);
                EncodeMath<Family>::bitwiseAnd(container, WORK_DIM_EQ_3_ALU_REGISTER, CONSTANT_ONE_ALU_REGISTER, WORK_DIM_EQ_3_ALU_REGISTER);
                EncodeMath<Family>::addition(container, WORK_DIM_GE_2_ALU_REGISTER, OFFSET_ALU_REGISTER, WORK_DIM_GE_2_ALU_REGISTER);
                EncodeMath<Family>::bitwiseAnd(container, WORK_DIM_GE_2_ALU_REGISTER, CONSTANT_ONE_ALU_REGISTER, WORK_DIM_GE_2_ALU_REGISTER);
            }

            EncodeSetMMIO<Family>::encodeREG(container, RESULT_REGISTER, CONSTANT_ONE_REGISTER);
            EncodeMath<Family>::addition(container, RESULT_ALU_REGISTER, WORK_DIM_GE_2_ALU_REGISTER, RESULT_ALU_REGISTER);
            EncodeMath<Family>::addition(container, RESULT_ALU_REGISTER, WORK_DIM_EQ_3_ALU_REGISTER, RESULT_ALU_REGISTER);

            if (offset) {
                EncodeMath<Family>::addition(container, RESULT_ALU_REGISTER, BACKUP_ALU_REGISTER, RESULT_ALU_REGISTER);
            }
        }
        EncodeStoreMMIO<Family>::encode(*container.getCommandStream(), RESULT_REGISTER, dstPtr);
    }
}

template <typename Family>
void EncodeDispatchKernel<Family>::adjustBindingTablePrefetch(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t samplerCount, uint32_t bindingTableEntryCount) {
    auto enablePrefetch = EncodeSurfaceState<Family>::doBindingTablePrefetch();
    if (DebugManager.flags.ForceBtpPrefetchMode.get() != -1) {
        enablePrefetch = static_cast<bool>(DebugManager.flags.ForceBtpPrefetchMode.get());
    }

    if (enablePrefetch) {
        interfaceDescriptor.setSamplerCount(static_cast<typename INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT>((samplerCount + 3) / 4));
        interfaceDescriptor.setBindingTableEntryCount(std::min(bindingTableEntryCount, 31u));
    } else {
        interfaceDescriptor.setSamplerCount(INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT::SAMPLER_COUNT_NO_SAMPLERS_USED);
        interfaceDescriptor.setBindingTableEntryCount(0u);
    }
}

template <typename Family>
void EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const HardwareInfo &hwInfo) {}

template <typename Family>
void EncodeIndirectParams<Family>::setGlobalWorkSizeIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress, const uint32_t *lws) {
    for (int i = 0; i < 3; ++i) {
        if (NEO::isUndefinedOffset(offsets[i])) {
            continue;
        }
        EncodeMathMMIO<Family>::encodeMulRegVal(container, GPUGPU_DISPATCHDIM[i], lws[i], ptrOffset(crossThreadAddress, offsets[i]));
    }
}

template <typename Family>
inline size_t EncodeIndirectParams<Family>::getCmdsSizeForSetWorkDimIndirect(const uint32_t *groupSize, bool misaligedPtr) {
    constexpr uint32_t aluCmdSize = sizeof(MI_MATH) + sizeof(MI_MATH_ALU_INST_INLINE) * NUM_ALU_INST_FOR_READ_MODIFY_WRITE;
    auto requiredSize = sizeof(MI_STORE_REGISTER_MEM) + sizeof(MI_LOAD_REGISTER_IMM);
    UNRECOVERABLE_IF(!groupSize);
    if (groupSize[2] < 2) {
        requiredSize += 2 * sizeof(MI_LOAD_REGISTER_IMM) + 3 * sizeof(MI_LOAD_REGISTER_REG) + 8 * aluCmdSize;
        if (misaligedPtr) {
            requiredSize += 2 * sizeof(MI_LOAD_REGISTER_IMM) + sizeof(MI_LOAD_REGISTER_MEM) + 7 * aluCmdSize;
        }
    }
    return requiredSize;
}
template <typename Family>
void EncodeSempahore<Family>::addMiSemaphoreWaitCommand(LinearStream &commandStream,
                                                        uint64_t compareAddress,
                                                        uint32_t compareData,
                                                        COMPARE_OPERATION compareMode) {
    addMiSemaphoreWaitCommand(commandStream, compareAddress, compareData, compareMode, false);
}

template <typename Family>
void EncodeSempahore<Family>::addMiSemaphoreWaitCommand(LinearStream &commandStream,
                                                        uint64_t compareAddress,
                                                        uint32_t compareData,
                                                        COMPARE_OPERATION compareMode,
                                                        bool registerPollMode) {
    auto semaphoreCommand = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
    programMiSemaphoreWait(semaphoreCommand,
                           compareAddress,
                           compareData,
                           compareMode,
                           registerPollMode);
}

template <typename Family>
inline size_t EncodeSempahore<Family>::getSizeMiSemaphoreWait() {
    return sizeof(MI_SEMAPHORE_WAIT);
}

template <typename Family>
inline void EncodeAtomic<Family>::setMiAtomicAddress(MI_ATOMIC &atomic, uint64_t writeAddress) {
    atomic.setMemoryAddress(static_cast<uint32_t>(writeAddress & 0x0000FFFFFFFFULL));
    atomic.setMemoryAddressHigh(static_cast<uint32_t>(writeAddress >> 32));
}

template <typename Family>
void EncodeAtomic<Family>::programMiAtomic(MI_ATOMIC *atomic,
                                           uint64_t writeAddress,
                                           ATOMIC_OPCODES opcode,
                                           DATA_SIZE dataSize,
                                           uint32_t returnDataControl,
                                           uint32_t csStall,
                                           uint32_t operand1dword0,
                                           uint32_t operand1dword1) {
    MI_ATOMIC cmd = Family::cmdInitAtomic;
    cmd.setAtomicOpcode(opcode);
    cmd.setDataSize(dataSize);
    EncodeAtomic<Family>::setMiAtomicAddress(cmd, writeAddress);
    cmd.setReturnDataControl(returnDataControl);
    cmd.setCsStall(csStall);
    if (opcode == ATOMIC_OPCODES::ATOMIC_4B_MOVE ||
        opcode == ATOMIC_OPCODES::ATOMIC_8B_MOVE) {
        cmd.setDwordLength(MI_ATOMIC::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_1);
        cmd.setInlineData(0x1);
        cmd.setOperand1DataDword0(operand1dword0);
        cmd.setOperand1DataDword1(operand1dword1);
    }

    *atomic = cmd;
}

template <typename Family>
void EncodeAtomic<Family>::programMiAtomic(LinearStream &commandStream,
                                           uint64_t writeAddress,
                                           ATOMIC_OPCODES opcode,
                                           DATA_SIZE dataSize,
                                           uint32_t returnDataControl,
                                           uint32_t csStall,
                                           uint32_t operand1dword0,
                                           uint32_t operand1dword1) {
    auto miAtomic = commandStream.getSpaceForCmd<MI_ATOMIC>();
    EncodeAtomic<Family>::programMiAtomic(miAtomic, writeAddress, opcode, dataSize, returnDataControl, csStall, operand1dword0, operand1dword1);
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programBatchBufferStart(LinearStream *commandStream,
                                                                  uint64_t address,
                                                                  bool secondLevel) {
    MI_BATCH_BUFFER_START cmd = Family::cmdInitBatchBufferStart;
    if (secondLevel) {
        cmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
    }
    cmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
    cmd.setBatchBufferStartAddress(address);
    auto buffer = commandStream->getSpaceForCmd<MI_BATCH_BUFFER_START>();
    *buffer = cmd;
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programBatchBufferEnd(CommandContainer &container) {
    MI_BATCH_BUFFER_END cmd = Family::cmdInitBatchBufferEnd;
    auto buffer = container.getCommandStream()->getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *buffer = cmd;
}

template <typename Family>
void EncodeMiFlushDW<Family>::programMiFlushDw(LinearStream &commandStream, uint64_t immediateDataGpuAddress, uint64_t immediateData,
                                               MiFlushArgs &args, const HardwareInfo &hwInfo) {
    programMiFlushDwWA(commandStream);

    auto miFlushDwCmd = commandStream.getSpaceForCmd<MI_FLUSH_DW>();
    MI_FLUSH_DW miFlush = Family::cmdInitMiFlushDw;
    if (args.commandWithPostSync) {
        auto postSyncType = args.timeStampOperation ? MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER : MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD;
        miFlush.setPostSyncOperation(postSyncType);
        miFlush.setDestinationAddress(immediateDataGpuAddress);
        miFlush.setImmediateData(immediateData);
    }
    miFlush.setNotifyEnable(args.notifyEnable);
    miFlush.setTlbInvalidate(args.tlbFlush);
    appendMiFlushDw(&miFlush, hwInfo);
    *miFlushDwCmd = miFlush;
}

template <typename Family>
size_t EncodeMiFlushDW<Family>::getMiFlushDwCmdSizeForDataWrite() {
    return sizeof(typename Family::MI_FLUSH_DW) + EncodeMiFlushDW<Family>::getMiFlushDwWaSize();
}

template <typename Family>
inline void EncodeMemoryPrefetch<Family>::programMemoryPrefetch(LinearStream &commandStream, const GraphicsAllocation &graphicsAllocation, uint32_t size, size_t offset, const HardwareInfo &hwInfo) {}

template <typename Family>
inline size_t EncodeMemoryPrefetch<Family>::getSizeForMemoryPrefetch(size_t size) { return 0u; }

template <typename Family>
void EncodeMiArbCheck<Family>::program(LinearStream &commandStream) {
    MI_ARB_CHECK cmd = Family::cmdInitArbCheck;

    EncodeMiArbCheck<Family>::adjust(cmd);

    auto miArbCheckStream = commandStream.getSpaceForCmd<MI_ARB_CHECK>();
    *miArbCheckStream = cmd;
}

template <typename Family>
inline size_t EncodeMiArbCheck<Family>::getCommandSize() { return sizeof(MI_ARB_CHECK); }

template <typename Family>
inline void EncodeNoop<Family>::alignToCacheLine(LinearStream &commandStream) {
    auto used = commandStream.getUsed();
    auto alignment = MemoryConstants::cacheLineSize;
    auto partialCacheline = used & (alignment - 1);
    if (partialCacheline) {
        auto amountToPad = alignment - partialCacheline;
        auto pCmd = commandStream.getSpace(amountToPad);
        memset(pCmd, 0, amountToPad);
    }
}

template <typename Family>
inline void EncodeNoop<Family>::emitNoop(LinearStream &commandStream, size_t bytesToUpdate) {
    if (bytesToUpdate) {
        auto ptr = commandStream.getSpace(bytesToUpdate);
        memset(ptr, 0, bytesToUpdate);
    }
}

template <typename Family>
inline void EncodeStoreMemory<Family>::programStoreDataImm(LinearStream &commandStream,
                                                           uint64_t gpuAddress,
                                                           uint32_t dataDword0,
                                                           uint32_t dataDword1,
                                                           bool storeQword,
                                                           bool workloadPartitionOffset) {
    auto miStoreDataImmBuffer = commandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    EncodeStoreMemory<Family>::programStoreDataImm(miStoreDataImmBuffer,
                                                   gpuAddress,
                                                   dataDword0,
                                                   dataDword1,
                                                   storeQword,
                                                   workloadPartitionOffset);
}

template <typename GfxFamily>
void EncodeEnableRayTracing<GfxFamily>::append3dStateBtd(void *ptr3dStateBtd) {}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::setAdditionalPipeControlFlagsForNonPipelineStateCommand(PipeControlArgs &args) {}

} // namespace NEO
