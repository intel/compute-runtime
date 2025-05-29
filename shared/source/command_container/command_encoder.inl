/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/program/kernel_info.h"

#include "encode_surface_state.inl"
#include "encode_surface_state_args.h"

#include <algorithm>

namespace NEO {

template <typename Family>
void EncodeStates<Family>::dshAlign(IndirectHeap *dsh) {
    dsh->align(InterfaceDescriptorTraits<INTERFACE_DESCRIPTOR_DATA>::samplerStatePointerAlignSize);
}

template <typename Family>
uint32_t EncodeStates<Family>::copySamplerState(IndirectHeap *dsh,
                                                uint32_t samplerStateOffset,
                                                uint32_t samplerCount,
                                                uint32_t borderColorOffset,
                                                const void *fnDynamicStateHeap,
                                                BindlessHeapsHelper *bindlessHeapHelper,
                                                const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto sizeSamplerState = sizeof(SAMPLER_STATE) * samplerCount;
    auto borderColorSize = samplerStateOffset - borderColorOffset;

    SAMPLER_STATE *dstSamplerState = nullptr;
    uint32_t samplerStateOffsetInDsh = 0;

    dsh->align(NEO::EncodeDispatchKernel<Family>::getDefaultDshAlignment());
    uint32_t borderColorOffsetInDsh = 0;
    auto borderColor = reinterpret_cast<const SAMPLER_BORDER_COLOR_STATE *>(ptrOffset(fnDynamicStateHeap, borderColorOffset));

    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    bool heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(hwInfo);

    if (!bindlessHeapHelper || (!bindlessHeapHelper->isGlobalDshSupported())) {
        borderColorOffsetInDsh = static_cast<uint32_t>(dsh->getUsed());
        // add offset of graphics allocation base address relative to heap base address
        if (bindlessHeapHelper) {
            borderColorOffsetInDsh += static_cast<uint32_t>(ptrDiff(dsh->getGpuBase(), bindlessHeapHelper->getGlobalHeapsBase()));
        }
        auto borderColorDst = dsh->getSpace(borderColorSize);
        memcpy_s(borderColorDst, borderColorSize, borderColor, borderColorSize);

        dsh->align(InterfaceDescriptorTraits<INTERFACE_DESCRIPTOR_DATA>::samplerStatePointerAlignSize);
        samplerStateOffsetInDsh = static_cast<uint32_t>(dsh->getUsed());

        dstSamplerState = reinterpret_cast<SAMPLER_STATE *>(dsh->getSpace(sizeSamplerState));
    } else {
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
        dshAlign(dsh);
        auto samplerStateInDsh = bindlessHeapHelper->allocateSSInHeap(sizeSamplerState, nullptr, BindlessHeapsHelper::BindlesHeapType::globalDsh);
        dstSamplerState = reinterpret_cast<SAMPLER_STATE *>(samplerStateInDsh.ssPtr);
        samplerStateOffsetInDsh = static_cast<uint32_t>(samplerStateInDsh.surfaceStateOffset);
    }

    auto &helper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto srcSamplerState = reinterpret_cast<const SAMPLER_STATE *>(ptrOffset(fnDynamicStateHeap, samplerStateOffset));
    SAMPLER_STATE state = {};
    for (uint32_t i = 0; i < samplerCount; i++) {
        state = srcSamplerState[i];

        if (heaplessEnabled) {
            EncodeStates<Family>::adjustSamplerStateBorderColor(state, *borderColor);
        } else {
            state.setIndirectStatePointer(static_cast<uint32_t>(borderColorOffsetInDsh));
        }
        helper.adjustSamplerState(&state, hwInfo);
        dstSamplerState[i] = state;
    }

    return samplerStateOffsetInDsh;
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeMulRegVal(CommandContainer &container, uint32_t offset, uint32_t val, uint64_t dstAddress, bool isBcs, EncodeStoreMMIOParams *outStoreMMIOParams) {
    int logLws = 0;
    int i = val;
    while (val >> logLws) {
        logLws++;
    }

    EncodeSetMMIO<Family>::encodeREG(container, RegisterOffsets::csGprR0, offset, isBcs);
    EncodeSetMMIO<Family>::encodeIMM(container, RegisterOffsets::csGprR1, 0, true, isBcs);

    i = 0;
    while (i < logLws) {
        if (val & (1 << i)) {
            EncodeMath<Family>::addition(container, AluRegisters::gpr1,
                                         AluRegisters::gpr0, AluRegisters::gpr2);
            EncodeSetMMIO<Family>::encodeREG(container, RegisterOffsets::csGprR1, RegisterOffsets::csGprR2, isBcs);
        }
        EncodeMath<Family>::addition(container, AluRegisters::gpr0,
                                     AluRegisters::gpr0, AluRegisters::gpr2);
        EncodeSetMMIO<Family>::encodeREG(container, RegisterOffsets::csGprR0, RegisterOffsets::csGprR2, isBcs);
        i++;
    }
    void **outStoreMMIOCmd = nullptr;
    if (outStoreMMIOParams) {
        outStoreMMIOParams->address = dstAddress;
        outStoreMMIOParams->offset = RegisterOffsets::csGprR1;
        outStoreMMIOParams->workloadPartition = false;
        outStoreMMIOParams->isBcs = isBcs;
        outStoreMMIOCmd = &outStoreMMIOParams->command;
    }
    EncodeStoreMMIO<Family>::encode(*container.getCommandStream(), RegisterOffsets::csGprR1, dstAddress, false, outStoreMMIOCmd, isBcs);
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
void EncodeMathMMIO<Family>::encodeGreaterThanPredicate(CommandContainer &container, uint64_t firstOperand, uint32_t secondOperand, bool isBcs) {
    EncodeSetMMIO<Family>::encodeMEM(container, RegisterOffsets::csGprR0, firstOperand, isBcs);
    EncodeSetMMIO<Family>::encodeIMM(container, RegisterOffsets::csGprR1, secondOperand, true, isBcs);

    /* RegisterOffsets::csGprR* registers map to AluRegisters::gpr* registers */
    EncodeMath<Family>::greaterThan(container, AluRegisters::gpr0,
                                    AluRegisters::gpr1, AluRegisters::gpr2);

    EncodeSetMMIO<Family>::encodeREG(container, RegisterOffsets::csPredicateResult, RegisterOffsets::csGprR2, isBcs);
}

/*
 * Compute bitwise AND between a register value from regOffset and immVal
 * and store it into dstAddress.
 */
template <typename Family>
void EncodeMathMMIO<Family>::encodeBitwiseAndVal(CommandContainer &container, uint32_t regOffset, uint32_t immVal, uint64_t dstAddress,
                                                 bool workloadPartition, void **outCmdBuffer, bool isBcs) {
    EncodeSetMMIO<Family>::encodeREG(container, RegisterOffsets::csGprR13, regOffset, isBcs);
    EncodeSetMMIO<Family>::encodeIMM(container, RegisterOffsets::csGprR14, immVal, true, isBcs);
    EncodeMath<Family>::bitwiseAnd(container, AluRegisters::gpr13,
                                   AluRegisters::gpr14,
                                   AluRegisters::gpr12);
    EncodeStoreMMIO<Family>::encode(*container.getCommandStream(),
                                    RegisterOffsets::csGprR12, dstAddress, workloadPartition, outCmdBuffer, isBcs);
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
    aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    aluParam.DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(srcA);
    *pAluParam = aluParam;
    pAluParam++;

    aluParam.DW0.Value = 0x0;
    aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    aluParam.DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
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
    aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    aluParam.DW0.BitField.Operand1 = static_cast<uint32_t>(finalResultRegister);
    aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(postOperationStateRegister);
    *pAluParam = aluParam;
    pAluParam++;
}

template <typename Family>
uint32_t *EncodeMath<Family>::commandReserve(CommandContainer &container) {
    return commandReserve(*container.getCommandStream());
}

template <typename Family>
uint32_t *EncodeMath<Family>::commandReserve(LinearStream &cmdStream) {
    size_t size = sizeof(MI_MATH) + sizeof(MI_MATH_ALU_INST_INLINE) * RegisterConstants::numAluInstForReadModifyWrite;

    auto cmd = reinterpret_cast<uint32_t *>(cmdStream.getSpace(size));
    MI_MATH mathBuffer;
    mathBuffer.DW0.Value = 0x0;
    mathBuffer.DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    mathBuffer.DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    mathBuffer.DW0.BitField.DwordLength = RegisterConstants::numAluInstForReadModifyWrite - 1;
    *reinterpret_cast<MI_MATH *>(cmd) = mathBuffer;
    cmd++;

    return cmd;
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeAluAdd(MI_MATH_ALU_INST_INLINE *pAluParam,
                                          AluRegisters firstOperandRegister,
                                          AluRegisters secondOperandRegister,
                                          AluRegisters finalResultRegister) {
    encodeAlu(pAluParam, firstOperandRegister, secondOperandRegister, AluRegisters::opcodeAdd, finalResultRegister, AluRegisters::accu);
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeAluSubStoreCarry(MI_MATH_ALU_INST_INLINE *pAluParam, AluRegisters regA, AluRegisters regB, AluRegisters finalResultRegister) {
    /* regB is subtracted from regA */
    encodeAlu(pAluParam, regA, regB, AluRegisters::opcodeSub, finalResultRegister, AluRegisters::cf);
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeAluAnd(MI_MATH_ALU_INST_INLINE *pAluParam,
                                          AluRegisters firstOperandRegister,
                                          AluRegisters secondOperandRegister,
                                          AluRegisters finalResultRegister) {
    encodeAlu(pAluParam, firstOperandRegister, secondOperandRegister, AluRegisters::opcodeAnd, finalResultRegister, AluRegisters::accu);
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeIncrementOrDecrement(LinearStream &cmdStream, AluRegisters operandRegister, IncrementOrDecrementOperation operationType, bool isBcs) {
    LriHelper<Family>::program(&cmdStream, RegisterOffsets::csGprR7, 1, true, isBcs);
    LriHelper<Family>::program(&cmdStream, RegisterOffsets::csGprR7 + 4, 0, true, isBcs);

    EncodeAluHelper<Family, 4> aluHelper;
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, operandRegister);
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr7);
    aluHelper.setNextAlu((operationType == IncrementOrDecrementOperation::increment) ? AluRegisters::opcodeAdd
                                                                                     : AluRegisters::opcodeSub);
    aluHelper.setNextAlu(AluRegisters::opcodeStore, operandRegister, AluRegisters::accu);

    aluHelper.copyToCmdStream(cmdStream);
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeIncrement(LinearStream &cmdStream, AluRegisters operandRegister, bool isBcs) {
    encodeIncrementOrDecrement(cmdStream, operandRegister, IncrementOrDecrementOperation::increment, isBcs);
}

template <typename Family>
void EncodeMathMMIO<Family>::encodeDecrement(LinearStream &cmdStream, AluRegisters operandRegister, bool isBcs) {
    encodeIncrementOrDecrement(cmdStream, operandRegister, IncrementOrDecrementOperation::decrement, isBcs);
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
void EncodeMath<Family>::addition(LinearStream &cmdStream,
                                  AluRegisters firstOperandRegister,
                                  AluRegisters secondOperandRegister,
                                  AluRegisters finalResultRegister) {
    uint32_t *cmd = EncodeMath<Family>::commandReserve(cmdStream);

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
inline void EncodeSetMMIO<Family>::encodeIMM(CommandContainer &container, uint32_t offset, uint32_t data, bool remap, bool isBcs) {
    EncodeSetMMIO<Family>::encodeIMM(*container.getCommandStream(), offset, data, remap, isBcs);
}

template <typename Family>
inline void EncodeSetMMIO<Family>::encodeMEM(CommandContainer &container, uint32_t offset, uint64_t address, bool isBcs) {
    EncodeSetMMIO<Family>::encodeMEM(*container.getCommandStream(), offset, address, isBcs);
}

template <typename Family>
inline void EncodeSetMMIO<Family>::encodeREG(CommandContainer &container, uint32_t dstOffset, uint32_t srcOffset, bool isBcs) {
    EncodeSetMMIO<Family>::encodeREG(*container.getCommandStream(), dstOffset, srcOffset, isBcs);
}

template <typename Family>
inline void EncodeSetMMIO<Family>::encodeIMM(LinearStream &cmdStream, uint32_t offset, uint32_t data, bool remap, bool isBcs) {
    LriHelper<Family>::program(&cmdStream,
                               offset,
                               data,
                               remap,
                               isBcs);
}

template <typename Family>
inline void EncodeStateBaseAddress<Family>::setSbaTrackingForL0DebuggerIfEnabled(bool trackingEnabled,
                                                                                 Device &device,
                                                                                 LinearStream &commandStream,
                                                                                 STATE_BASE_ADDRESS &sbaCmd, bool useFirstLevelBB) {
    if (!trackingEnabled) {
        return;
    }
    NEO::Debugger::SbaAddresses sbaAddresses = {};
    NEO::EncodeStateBaseAddress<Family>::setSbaAddressesForDebugger(sbaAddresses, sbaCmd);
    device.getL0Debugger()->captureStateBaseAddress(commandStream, sbaAddresses, useFirstLevelBB);
}

template <typename Family>
void EncodeSetMMIO<Family>::encodeMEM(LinearStream &cmdStream, uint32_t offset, uint64_t address, bool isBcs) {
    MI_LOAD_REGISTER_MEM cmd = Family::cmdInitLoadRegisterMem;
    cmd.setRegisterAddress(offset);
    cmd.setMemoryAddress(address);
    remapOffset(&cmd);
    if (isBcs) {
        cmd.setRegisterAddress(offset + RegisterOffsets::bcs0Base);
    }

    auto buffer = cmdStream.getSpaceForCmd<MI_LOAD_REGISTER_MEM>();
    *buffer = cmd;
}

template <typename Family>
void EncodeSetMMIO<Family>::encodeREG(LinearStream &cmdStream, uint32_t dstOffset, uint32_t srcOffset, bool isBcs) {
    MI_LOAD_REGISTER_REG cmd = Family::cmdInitLoadRegisterReg;
    cmd.setSourceRegisterAddress(srcOffset);
    cmd.setDestinationRegisterAddress(dstOffset);
    remapOffset(&cmd);
    if (isBcs) {
        cmd.setSourceRegisterAddress(srcOffset + RegisterOffsets::bcs0Base);
        cmd.setDestinationRegisterAddress(dstOffset + RegisterOffsets::bcs0Base);
    }
    auto buffer = cmdStream.getSpaceForCmd<MI_LOAD_REGISTER_REG>();
    *buffer = cmd;
}

template <typename Family>
void EncodeStoreMMIO<Family>::encode(LinearStream &csr, uint32_t offset, uint64_t address, bool workloadPartition, void **outCmdBuffer, bool isBcs) {
    auto buffer = csr.getSpaceForCmd<MI_STORE_REGISTER_MEM>();
    if (outCmdBuffer != nullptr) {
        *outCmdBuffer = buffer;
    }
    EncodeStoreMMIO<Family>::encode(buffer, offset, address, workloadPartition, isBcs);
}

template <typename Family>
inline void EncodeStoreMMIO<Family>::encode(MI_STORE_REGISTER_MEM *cmdBuffer, uint32_t offset, uint64_t address, bool workloadPartition, bool isBcs) {
    MI_STORE_REGISTER_MEM cmd = Family::cmdInitStoreRegisterMem;
    cmd.setRegisterAddress(offset);
    cmd.setMemoryAddress(address);
    appendFlags(&cmd, workloadPartition);
    if (isBcs) {
        cmd.setRegisterAddress(offset + RegisterOffsets::bcs0Base);
    }
    *cmdBuffer = cmd;
}

template <typename Family>
void EncodeSurfaceState<Family>::encodeBuffer(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    uint64_t bufferSize = alignUp(args.size, getSurfaceBaseAddressAlignment());
    bufferSize = std::min(bufferSize, static_cast<uint64_t>(MemoryConstants::fullStatefulRegion - 1));
    SurfaceStateBufferLength length = {0};
    length.length = static_cast<uint32_t>(bufferSize - 1);

    surfaceState->setWidth(length.surfaceState.width + 1);
    surfaceState->setHeight(length.surfaceState.height + 1);
    surfaceState->setDepth(length.surfaceState.depth + 1);

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

    if (debugManager.flags.DisableCachingForStatefulBufferAccess.get()) {
        surfaceState->setMemoryObjectControlState(args.gmmHelper->getUncachedMOCS());
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
size_t EncodeSurfaceState<Family>::pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap,
                                                                    const void *srcKernelSsh, size_t srcKernelSshSize,
                                                                    size_t numberOfBindingTableStates, size_t offsetOfBindingTable) {
    if constexpr (Family::isHeaplessRequired()) {
        UNRECOVERABLE_IF(true);
        return 0;
    } else {
        using BINDING_TABLE_STATE = typename Family::BINDING_TABLE_STATE;

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
        DEBUG_BREAK_IF(reinterpret_cast<uintptr_t>(dstBtiTableBase) % Family::INTERFACE_DESCRIPTOR_DATA::BINDINGTABLEPOINTER_ALIGN_SIZE != 0);
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
}

template <typename Family>
void EncodeSurfaceState<Family>::appendParamsForImageFromBuffer(R_SURFACE_STATE *surfaceState) {
}

template <typename Family>
inline void EncodeDispatchKernel<Family>::encodeCommon(CommandContainer &container, EncodeDispatchKernelArgs &args) {
    using DefaultWalkerType = typename Family::DefaultWalkerType;
    EncodeDispatchKernel<Family>::template encode<DefaultWalkerType>(container, args);
}

template <typename Family>
void *EncodeDispatchKernel<Family>::getInterfaceDescriptor(CommandContainer &container, IndirectHeap *childDsh, uint32_t &iddOffset) {
    if (container.nextIddInBlockRef() == container.getNumIddPerBlock()) {
        void *heapPointer = nullptr;
        size_t heapSize = sizeof(INTERFACE_DESCRIPTOR_DATA) * container.getNumIddPerBlock();
        if (childDsh != nullptr) {
            childDsh->align(NEO::EncodeDispatchKernel<Family>::getDefaultDshAlignment());
            heapPointer = childDsh->getSpace(heapSize);
        } else {
            container.getIndirectHeap(HeapType::dynamicState)->align(NEO::EncodeDispatchKernel<Family>::getDefaultDshAlignment());
            heapPointer = container.getHeapSpaceAllowGrow(HeapType::dynamicState, heapSize);
        }
        container.setIddBlock(heapPointer);
        container.nextIddInBlockRef() = 0;
    }

    iddOffset = container.nextIddInBlockRef();
    auto interfaceDescriptorData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(container.getIddBlock());
    container.nextIddInBlockRef()++;
    return &interfaceDescriptorData[iddOffset];
}

template <typename Family>
bool EncodeDispatchKernel<Family>::inlineDataProgrammingRequired(const KernelDescriptor &kernelDesc) {
    auto checkKernelForInlineData = true;
    if (debugManager.flags.EnablePassInlineData.get() != -1) {
        checkKernelForInlineData = !!debugManager.flags.EnablePassInlineData.get();
    }
    if (checkKernelForInlineData) {
        return kernelDesc.kernelAttributes.flags.passInlineData;
    }
    return false;
}

template <typename Family>
void EncodeIndirectParams<Family>::encode(CommandContainer &container, uint64_t crossThreadDataGpuVa, DispatchKernelEncoderI *dispatchInterface, uint64_t implicitArgsGpuPtr, IndirectParamsInInlineDataArgs *outArgs) {
    const auto &kernelDescriptor = dispatchInterface->getKernelDescriptor();
    if (outArgs) {
        for (int i = 0; i < 3; i++) {
            if (!NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[i]) && kernelDescriptor.kernelAttributes.inlineDataPayloadSize > kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[i]) {
                outArgs->storeGroupCountInInlineData[i] = true;
            }
            if (!NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[i]) && kernelDescriptor.kernelAttributes.inlineDataPayloadSize > kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[i]) {
                outArgs->storeGlobalWorkSizeInInlineData[i] = true;
            }
        }
        if (!NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.dispatchTraits.workDim) && kernelDescriptor.kernelAttributes.inlineDataPayloadSize > kernelDescriptor.payloadMappings.dispatchTraits.workDim) {
            outArgs->storeWorkDimInInlineData = true;
        }
    }
    setGroupCountIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups, crossThreadDataGpuVa, outArgs);
    setGlobalWorkSizeIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize, crossThreadDataGpuVa, dispatchInterface->getGroupSize(), outArgs);
    UNRECOVERABLE_IF(NEO::isValidOffset(kernelDescriptor.payloadMappings.dispatchTraits.workDim) && (kernelDescriptor.payloadMappings.dispatchTraits.workDim & 0b11) != 0u);
    setWorkDimIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.workDim, crossThreadDataGpuVa, dispatchInterface->getGroupSize(), outArgs);
    if (implicitArgsGpuPtr) {
        const auto version = container.getDevice()->getGfxCoreHelper().getImplicitArgsVersion();
        if (version == 0) {
            constexpr CrossThreadDataOffset groupCountOffset[] = {offsetof(ImplicitArgsV0, groupCountX), offsetof(ImplicitArgsV0, groupCountY), offsetof(ImplicitArgsV0, groupCountZ)};
            constexpr CrossThreadDataOffset globalSizeOffset[] = {offsetof(ImplicitArgsV0, globalSizeX), offsetof(ImplicitArgsV0, globalSizeY), offsetof(ImplicitArgsV0, globalSizeZ)};
            constexpr auto numWorkDimOffset = offsetof(ImplicitArgsV0, numWorkDim);
            setGroupCountIndirect(container, groupCountOffset, implicitArgsGpuPtr, nullptr);
            setGlobalWorkSizeIndirect(container, globalSizeOffset, implicitArgsGpuPtr, dispatchInterface->getGroupSize(), nullptr);
            setWorkDimIndirect(container, numWorkDimOffset, implicitArgsGpuPtr, dispatchInterface->getGroupSize(), nullptr);
        } else if (version == 1) {
            constexpr CrossThreadDataOffset groupCountOffsetV1[] = {offsetof(ImplicitArgsV1, groupCountX), offsetof(ImplicitArgsV1, groupCountY), offsetof(ImplicitArgsV1, groupCountZ)};
            constexpr CrossThreadDataOffset globalSizeOffsetV1[] = {offsetof(ImplicitArgsV1, globalSizeX), offsetof(ImplicitArgsV1, globalSizeY), offsetof(ImplicitArgsV1, globalSizeZ)};
            constexpr auto numWorkDimOffsetV1 = offsetof(ImplicitArgsV1, numWorkDim);
            setGroupCountIndirect(container, groupCountOffsetV1, implicitArgsGpuPtr, nullptr);
            setGlobalWorkSizeIndirect(container, globalSizeOffsetV1, implicitArgsGpuPtr, dispatchInterface->getGroupSize(), nullptr);
            setWorkDimIndirect(container, numWorkDimOffsetV1, implicitArgsGpuPtr, dispatchInterface->getGroupSize(), nullptr);
        }
    }
    if (outArgs && !outArgs->commandsToPatch.empty()) {
        auto &commandStream = *container.getCommandStream();
        EncodeMiArbCheck<Family>::program(commandStream, true);
        auto gpuVa = commandStream.getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<Family>::getBatchBufferStartSize();
        EncodeBatchBufferStartOrEnd<Family>::programBatchBufferStart(&commandStream, gpuVa, !(container.getFlushTaskUsedForImmediate() || container.isUsingPrimaryBuffer()), false, false);
        EncodeMiArbCheck<Family>::program(commandStream, false);
    }
}

template <typename Family>
void EncodeIndirectParams<Family>::setGroupCountIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress, IndirectParamsInInlineDataArgs *outArgs) {
    for (int i = 0; i < 3; ++i) {
        if (NEO::isUndefinedOffset(offsets[i])) {
            continue;
        }
        void **storeCmd = nullptr;
        if (outArgs && outArgs->storeGroupCountInInlineData[i]) {
            outArgs->commandsToPatch.push_back({});
            auto &commandArgs = outArgs->commandsToPatch.back();
            storeCmd = &commandArgs.command;
            commandArgs.address = offsets[i];
            commandArgs.offset = RegisterOffsets::gpgpuDispatchDim[i];
            commandArgs.isBcs = false;
            commandArgs.workloadPartition = false;
        }
        EncodeStoreMMIO<Family>::encode(*container.getCommandStream(), RegisterOffsets::gpgpuDispatchDim[i], ptrOffset(crossThreadAddress, offsets[i]), false, storeCmd, false);
    }
}

template <typename Family>
void EncodeIndirectParams<Family>::applyInlineDataGpuVA(IndirectParamsInInlineDataArgs &args, uint64_t inlineDataGpuVa) {
    for (auto &commandArgs : args.commandsToPatch) {
        auto commandToPatch = reinterpret_cast<MI_STORE_REGISTER_MEM *>(commandArgs.command);
        EncodeStoreMMIO<Family>::encode(commandToPatch, commandArgs.offset, commandArgs.address + inlineDataGpuVa, commandArgs.workloadPartition, commandArgs.isBcs);
    }
}

template <typename Family>
void EncodeIndirectParams<Family>::setWorkDimIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset workDimOffset, uint64_t crossThreadAddress, const uint32_t *groupSize, IndirectParamsInInlineDataArgs *outArgs) {
    if (NEO::isValidOffset(workDimOffset)) {
        auto dstPtr = ptrOffset(crossThreadAddress, workDimOffset);
        constexpr uint32_t resultRegister = RegisterOffsets::csGprR0;
        constexpr AluRegisters resultAluRegister = AluRegisters::gpr0;
        const uint32_t offset = static_cast<uint32_t>((1ull << 8 * (dstPtr & 0b11)) - 1);
        const uint32_t memoryMask = std::numeric_limits<uint32_t>::max() - static_cast<uint32_t>((1ull << 8 * ((dstPtr & 0b11) + 1)) - 1) + offset;

        /*
         * if ( groupSize[2] > 1 || groupCount[2] > 1 ) { workdim = 3 }
         * else if ( groupSize[1] + groupCount[1] > 2 ) { workdim = 2 }
         * else { workdim = 1 }
         */

        if (groupSize[2] > 1) {
            EncodeSetMMIO<Family>::encodeIMM(container, resultRegister, 3 << (8 * (dstPtr & 0b11)), true, false);
        } else {

            constexpr uint32_t groupCount2Register = RegisterOffsets::csGprR1;
            constexpr AluRegisters groupCount2AluRegister = AluRegisters::gpr1;

            constexpr uint32_t groupSize1Register = RegisterOffsets::csGprR0;
            constexpr AluRegisters groupSize1AluRegister = AluRegisters::gpr0;

            constexpr uint32_t groupCount1Register = RegisterOffsets::csGprR1;
            constexpr AluRegisters groupCount1AluRegister = AluRegisters::gpr1;

            constexpr AluRegisters sumAluRegister = AluRegisters::gpr0;

            constexpr AluRegisters workDimEq3AluRegister = AluRegisters::gpr3;

            constexpr AluRegisters workDimGe2AluRegister = AluRegisters::gpr4;

            constexpr uint32_t constantOneRegister = RegisterOffsets::csGprR5;
            constexpr AluRegisters constantOneAluRegister = AluRegisters::gpr5;
            constexpr uint32_t constantTwoRegister = RegisterOffsets::csGprR6;
            constexpr AluRegisters constantTwoAluRegister = AluRegisters::gpr6;

            constexpr uint32_t backupRegister = RegisterOffsets::csGprR7;
            constexpr AluRegisters backupAluRegister = AluRegisters::gpr7;

            constexpr uint32_t memoryMaskRegister = RegisterOffsets::csGprR8;
            constexpr AluRegisters memoryMaskAluRegister = AluRegisters::gpr8;

            constexpr uint32_t offsetRegister = RegisterOffsets::csGprR8;
            constexpr AluRegisters offsetAluRegister = AluRegisters::gpr8;

            if (offset) {
                EncodeSetMMIO<Family>::encodeMEM(container, backupRegister, dstPtr, false);
                EncodeSetMMIO<Family>::encodeIMM(container, memoryMaskRegister, memoryMask, true, false);
                EncodeMath<Family>::bitwiseAnd(container, memoryMaskAluRegister, backupAluRegister, backupAluRegister);
                EncodeSetMMIO<Family>::encodeIMM(container, offsetRegister, offset, true, false);
            }

            EncodeSetMMIO<Family>::encodeIMM(container, constantOneRegister, 1, true, false);
            EncodeSetMMIO<Family>::encodeIMM(container, constantTwoRegister, 2, true, false);

            EncodeSetMMIO<Family>::encodeREG(container, groupCount2Register, RegisterOffsets::gpgpuDispatchDim[2], false);

            EncodeMath<Family>::greaterThan(container, groupCount2AluRegister, constantOneAluRegister, workDimEq3AluRegister);
            EncodeMath<Family>::bitwiseAnd(container, workDimEq3AluRegister, constantOneAluRegister, workDimEq3AluRegister);

            EncodeSetMMIO<Family>::encodeIMM(container, groupSize1Register, groupSize[1], true, false);
            EncodeSetMMIO<Family>::encodeREG(container, groupCount1Register, RegisterOffsets::gpgpuDispatchDim[1], false);

            EncodeMath<Family>::addition(container, groupSize1AluRegister, groupCount1AluRegister, sumAluRegister);
            EncodeMath<Family>::addition(container, sumAluRegister, workDimEq3AluRegister, sumAluRegister);
            EncodeMath<Family>::greaterThan(container, sumAluRegister, constantTwoAluRegister, workDimGe2AluRegister);
            EncodeMath<Family>::bitwiseAnd(container, workDimGe2AluRegister, constantOneAluRegister, workDimGe2AluRegister);

            if (offset) {
                EncodeMath<Family>::addition(container, constantOneAluRegister, offsetAluRegister, constantOneAluRegister);
                EncodeMath<Family>::addition(container, workDimEq3AluRegister, offsetAluRegister, workDimEq3AluRegister);
                EncodeMath<Family>::bitwiseAnd(container, workDimEq3AluRegister, constantOneAluRegister, workDimEq3AluRegister);
                EncodeMath<Family>::addition(container, workDimGe2AluRegister, offsetAluRegister, workDimGe2AluRegister);
                EncodeMath<Family>::bitwiseAnd(container, workDimGe2AluRegister, constantOneAluRegister, workDimGe2AluRegister);
            }

            EncodeSetMMIO<Family>::encodeREG(container, resultRegister, constantOneRegister, false);
            EncodeMath<Family>::addition(container, resultAluRegister, workDimGe2AluRegister, resultAluRegister);
            EncodeMath<Family>::addition(container, resultAluRegister, workDimEq3AluRegister, resultAluRegister);

            if (offset) {
                EncodeMath<Family>::addition(container, resultAluRegister, backupAluRegister, resultAluRegister);
            }
        }
        void **storeCmd = nullptr;
        if (outArgs && outArgs->storeWorkDimInInlineData) {
            outArgs->commandsToPatch.push_back({});
            auto &commandArgs = outArgs->commandsToPatch.back();
            storeCmd = &commandArgs.command;
            commandArgs.address = workDimOffset;
            commandArgs.offset = resultRegister;
            commandArgs.isBcs = false;
            commandArgs.workloadPartition = false;
        }
        EncodeStoreMMIO<Family>::encode(*container.getCommandStream(), resultRegister, dstPtr, false, storeCmd, false);
    }
}

template <typename Family>
bool EncodeSurfaceState<Family>::doBindingTablePrefetch() {
    auto enableBindingTablePrefetech = isBindingTablePrefetchPreferred();
    if (debugManager.flags.ForceBtpPrefetchMode.get() != -1) {
        enableBindingTablePrefetech = static_cast<bool>(debugManager.flags.ForceBtpPrefetchMode.get());
    }
    return enableBindingTablePrefetech;
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernelWithHeap<Family>::adjustBindingTablePrefetch(InterfaceDescriptorType &interfaceDescriptor, uint32_t samplerCount, uint32_t bindingTableEntryCount) {
    auto enablePrefetch = EncodeSurfaceState<Family>::doBindingTablePrefetch();

    if (enablePrefetch) {
        interfaceDescriptor.setSamplerCount(static_cast<typename InterfaceDescriptorType::SAMPLER_COUNT>((samplerCount + 3) / 4));
        interfaceDescriptor.setBindingTableEntryCount(std::min(bindingTableEntryCount, 31u));
    } else {
        interfaceDescriptor.setSamplerCount(InterfaceDescriptorType::SAMPLER_COUNT::SAMPLER_COUNT_NO_SAMPLERS_USED);
        interfaceDescriptor.setBindingTableEntryCount(0u);
    }
}

template <typename Family>
size_t EncodeDispatchKernel<Family>::getSizeRequiredDsh(const KernelDescriptor &kernelDescriptor, uint32_t iddCount) {
    constexpr auto samplerStateSize = sizeof(typename Family::SAMPLER_STATE);
    const auto numSamplers = kernelDescriptor.payloadMappings.samplerTable.numSamplers;
    const auto additionalDshSize = additionalSizeRequiredDsh(iddCount);
    if (numSamplers == 0U) {
        return alignUp(additionalDshSize, EncodeDispatchKernel<Family>::getDefaultDshAlignment());
    }

    size_t size = kernelDescriptor.payloadMappings.samplerTable.tableOffset -
                  kernelDescriptor.payloadMappings.samplerTable.borderColor;
    size = alignUp(size, EncodeDispatchKernel<Family>::getDefaultDshAlignment());

    size += numSamplers * samplerStateSize;
    size = alignUp(size, InterfaceDescriptorTraits<INTERFACE_DESCRIPTOR_DATA>::samplerStatePointerAlignSize);

    if (additionalDshSize > 0) {
        size = alignUp(size, EncodeStates<Family>::alignInterfaceDescriptorData);
        size += additionalDshSize;
        size = alignUp(size, EncodeDispatchKernel<Family>::getDefaultDshAlignment());
    }

    return size;
}

template <typename Family>
size_t EncodeDispatchKernel<Family>::getSizeRequiredSsh(const KernelInfo &kernelInfo) {
    size_t requiredSshSize = kernelInfo.heapInfo.surfaceStateHeapSize;
    bool isBindlessKernel = NEO::KernelDescriptor ::isBindlessAddressingKernel(kernelInfo.kernelDescriptor);
    if (isBindlessKernel) {
        requiredSshSize = std::max(requiredSshSize, kernelInfo.kernelDescriptor.kernelAttributes.numArgsStateful * sizeof(typename Family::RENDER_SURFACE_STATE));
    }
    requiredSshSize = alignUp(requiredSshSize, EncodeDispatchKernel<Family>::getDefaultSshAlignment());
    return requiredSshSize;
}

template <typename Family>
size_t EncodeDispatchKernel<Family>::getDefaultDshAlignment() {
    return Family::cacheLineSize;
}

template <typename Family>
void EncodeIndirectParams<Family>::setGlobalWorkSizeIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress, const uint32_t *lws, IndirectParamsInInlineDataArgs *outArgs) {
    for (int i = 0; i < 3; ++i) {
        if (NEO::isUndefinedOffset(offsets[i])) {
            continue;
        }
        EncodeStoreMMIOParams *storeParams = nullptr;

        auto patchLocation = ptrOffset(crossThreadAddress, offsets[i]);
        if (outArgs && outArgs->storeGlobalWorkSizeInInlineData[i]) {
            outArgs->commandsToPatch.push_back({});
            storeParams = &outArgs->commandsToPatch.back();
            patchLocation = offsets[i];
        }
        EncodeMathMMIO<Family>::encodeMulRegVal(container, RegisterOffsets::gpgpuDispatchDim[i], lws[i], patchLocation, false, storeParams);
    }
}

template <typename Family>
inline size_t EncodeIndirectParams<Family>::getCmdsSizeForSetWorkDimIndirect(const uint32_t *groupSize, bool misaligedPtr) {
    constexpr uint32_t aluCmdSize = sizeof(MI_MATH) + sizeof(MI_MATH_ALU_INST_INLINE) * RegisterConstants::numAluInstForReadModifyWrite;
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
void EncodeSemaphore<Family>::addMiSemaphoreWaitCommand(LinearStream &commandStream,
                                                        uint64_t compareAddress,
                                                        uint64_t compareData,
                                                        COMPARE_OPERATION compareMode,
                                                        bool registerPollMode,
                                                        bool useQwordData,
                                                        bool indirect,
                                                        bool switchOnUnsuccessful,
                                                        void **outSemWaitCmd) {
    auto semaphoreCommand = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
    if (outSemWaitCmd != nullptr) {
        *outSemWaitCmd = semaphoreCommand;
    }
    programMiSemaphoreWait(semaphoreCommand, compareAddress, compareData, compareMode, registerPollMode, true, useQwordData, indirect, switchOnUnsuccessful);
}
template <typename Family>
void EncodeSemaphore<Family>::applyMiSemaphoreWaitCommand(LinearStream &commandStream, std::list<void *> &commandsList) {
    MI_SEMAPHORE_WAIT *semaphoreCommand = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
    commandsList.push_back(semaphoreCommand);
}

template <typename Family>
void EncodeAtomic<Family>::programMiAtomic(MI_ATOMIC *atomic,
                                           uint64_t writeAddress,
                                           ATOMIC_OPCODES opcode,
                                           DATA_SIZE dataSize,
                                           uint32_t returnDataControl,
                                           uint32_t csStall,
                                           uint64_t operand1Data,
                                           uint64_t operand2Data) {
    MI_ATOMIC cmd = Family::cmdInitAtomic;
    cmd.setAtomicOpcode(opcode);
    cmd.setDataSize(dataSize);
    EncodeAtomic<Family>::setMiAtomicAddress(cmd, writeAddress);
    cmd.setReturnDataControl(returnDataControl);
    cmd.setCsStall(csStall);

    if (opcode == ATOMIC_OPCODES::ATOMIC_4B_MOVE || opcode == ATOMIC_OPCODES::ATOMIC_8B_MOVE || opcode == ATOMIC_OPCODES::ATOMIC_8B_CMP_WR || opcode == ATOMIC_OPCODES::ATOMIC_8B_ADD) {
        cmd.setDwordLength(MI_ATOMIC::DWORD_LENGTH::DWORD_LENGTH_INLINE_DATA_1);
        cmd.setInlineData(0x1);

        cmd.setOperand1DataDword0(getLowPart(operand1Data));
        cmd.setOperand1DataDword1(getHighPart(operand1Data));

        cmd.setOperand2DataDword0(getLowPart(operand2Data));
        cmd.setOperand2DataDword1(getHighPart(operand2Data));
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
                                           uint64_t operand1Data,
                                           uint64_t operand2Data) {
    auto miAtomic = commandStream.getSpaceForCmd<MI_ATOMIC>();
    EncodeAtomic<Family>::programMiAtomic(miAtomic, writeAddress, opcode, dataSize, returnDataControl, csStall, operand1Data, operand2Data);
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programConditionalDataMemBatchBufferStart(LinearStream &commandStream, uint64_t startAddress, uint64_t compareAddress,
                                                                                    uint64_t compareData, CompareOperation compareOperation, bool indirect, bool useQwordData, bool isBcs) {
    EncodeSetMMIO<Family>::encodeMEM(commandStream, RegisterOffsets::csGprR7, compareAddress, isBcs);

    if (useQwordData) {
        EncodeSetMMIO<Family>::encodeMEM(commandStream, RegisterOffsets::csGprR7 + 4, compareAddress + 4, isBcs);
    } else {
        LriHelper<Family>::program(&commandStream, RegisterOffsets::csGprR7 + 4, 0, true, isBcs);
    }

    uint32_t compareDataLow = static_cast<uint32_t>(compareData & std::numeric_limits<uint32_t>::max());
    uint32_t compareDataHigh = useQwordData ? static_cast<uint32_t>(compareData >> 32) : 0;

    LriHelper<Family>::program(&commandStream, RegisterOffsets::csGprR8, compareDataLow, true, isBcs);
    LriHelper<Family>::program(&commandStream, RegisterOffsets::csGprR8 + 4, compareDataHigh, true, isBcs);

    programConditionalBatchBufferStartBase(commandStream, startAddress, AluRegisters::gpr7, AluRegisters::gpr8, compareOperation, indirect, isBcs);
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programConditionalDataRegBatchBufferStart(LinearStream &commandStream, uint64_t startAddress, uint32_t compareReg,
                                                                                    uint64_t compareData, CompareOperation compareOperation, bool indirect, bool useQwordData, bool isBcs) {
    EncodeSetMMIO<Family>::encodeREG(commandStream, RegisterOffsets::csGprR7, compareReg, isBcs);
    if (useQwordData) {
        EncodeSetMMIO<Family>::encodeREG(commandStream, RegisterOffsets::csGprR7 + 4, compareReg + 4, isBcs);
    } else {
        LriHelper<Family>::program(&commandStream, RegisterOffsets::csGprR7 + 4, 0, true, isBcs);
    }

    uint32_t compareDataLow = static_cast<uint32_t>(compareData & std::numeric_limits<uint32_t>::max());
    uint32_t compareDataHigh = useQwordData ? static_cast<uint32_t>(compareData >> 32) : 0;

    LriHelper<Family>::program(&commandStream, RegisterOffsets::csGprR8, compareDataLow, true, isBcs);
    LriHelper<Family>::program(&commandStream, RegisterOffsets::csGprR8 + 4, compareDataHigh, true, isBcs);

    programConditionalBatchBufferStartBase(commandStream, startAddress, AluRegisters::gpr7, AluRegisters::gpr8, compareOperation, indirect, isBcs);
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programConditionalRegRegBatchBufferStart(LinearStream &commandStream, uint64_t startAddress, AluRegisters compareReg0,
                                                                                   AluRegisters compareReg1, CompareOperation compareOperation, bool indirect, bool isBcs) {

    programConditionalBatchBufferStartBase(commandStream, startAddress, compareReg0, compareReg1, compareOperation, indirect, isBcs);
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programConditionalRegMemBatchBufferStart(LinearStream &commandStream, uint64_t startAddress, uint64_t compareAddress, uint32_t compareReg,
                                                                                   CompareOperation compareOperation, bool indirect, bool isBcs) {
    EncodeSetMMIO<Family>::encodeMEM(commandStream, RegisterOffsets::csGprR7, compareAddress, isBcs);
    LriHelper<Family>::program(&commandStream, RegisterOffsets::csGprR7 + 4, 0, true, isBcs);

    EncodeSetMMIO<Family>::encodeREG(commandStream, RegisterOffsets::csGprR8, compareReg, isBcs);
    LriHelper<Family>::program(&commandStream, RegisterOffsets::csGprR8 + 4, 0, true, isBcs);

    programConditionalBatchBufferStartBase(commandStream, startAddress, AluRegisters::gpr7, AluRegisters::gpr8, compareOperation, indirect, isBcs);
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programConditionalBatchBufferStartBase(LinearStream &commandStream, uint64_t startAddress, AluRegisters regA, AluRegisters regB,
                                                                                 CompareOperation compareOperation, bool indirect, bool isBcs) {
    EncodeAluHelper<Family, 4> aluHelper;
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, regA);
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, regB);
    aluHelper.setNextAlu(AluRegisters::opcodeSub);

    if ((compareOperation == CompareOperation::equal) || (compareOperation == CompareOperation::notEqual)) {
        aluHelper.setNextAlu(AluRegisters::opcodeStore, AluRegisters::gpr7, AluRegisters::zf);
    } else if ((compareOperation == CompareOperation::greaterOrEqual) || (compareOperation == CompareOperation::less)) {
        aluHelper.setNextAlu(AluRegisters::opcodeStore, AluRegisters::gpr7, AluRegisters::cf);
    } else {
        UNRECOVERABLE_IF(true);
    }

    aluHelper.copyToCmdStream(commandStream);

    EncodeSetMMIO<Family>::encodeREG(commandStream, RegisterOffsets::csPredicateResult2, RegisterOffsets::csGprR7, isBcs);

    MiPredicateType predicateType = MiPredicateType::noopOnResult2Clear; // Equal or Less
    if ((compareOperation == CompareOperation::notEqual) || (compareOperation == CompareOperation::greaterOrEqual)) {
        predicateType = MiPredicateType::noopOnResult2Set;
    }

    EncodeMiPredicate<Family>::encode(commandStream, predicateType);

    programBatchBufferStart(&commandStream, startAddress, false, indirect, true);

    EncodeMiPredicate<Family>::encode(commandStream, MiPredicateType::disable);
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programBatchBufferStart(MI_BATCH_BUFFER_START *cmdBuffer, uint64_t address, bool secondLevel, bool indirect, bool predicate) {
    MI_BATCH_BUFFER_START cmd = Family::cmdInitBatchBufferStart;
    if (secondLevel) {
        cmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
    }
    cmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
    cmd.setBatchBufferStartAddress(address);

    appendBatchBufferStart(cmd, indirect, predicate);

    *cmdBuffer = cmd;
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programBatchBufferStart(LinearStream *commandStream, uint64_t address, bool secondLevel, bool indirect, bool predicate) {
    programBatchBufferStart(commandStream->getSpaceForCmd<MI_BATCH_BUFFER_START>(), address, secondLevel, indirect, predicate);
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programBatchBufferEnd(LinearStream &commandStream) {
    MI_BATCH_BUFFER_END cmd = Family::cmdInitBatchBufferEnd;
    auto buffer = commandStream.getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *buffer = cmd;
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::programBatchBufferEnd(CommandContainer &container) {
    programBatchBufferEnd(*container.getCommandStream());
}

template <typename GfxFamily>
void EncodeMiFlushDW<GfxFamily>::appendWa(LinearStream &commandStream, MiFlushArgs &args) {
    BlitCommandsHelper<GfxFamily>::dispatchDummyBlit(commandStream, args.waArgs);
}

template <typename Family>
void EncodeMiFlushDW<Family>::programWithWa(LinearStream &commandStream, uint64_t immediateDataGpuAddress, uint64_t immediateData,
                                            MiFlushArgs &args) {
    UNRECOVERABLE_IF(args.waArgs.isWaRequired && !args.commandWithPostSync);
    appendWa(commandStream, args);
    args.waArgs.isWaRequired = false;

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
    adjust(&miFlush, args.waArgs.rootDeviceEnvironment->getProductHelper());
    *miFlushDwCmd = miFlush;
}

template <typename Family>
size_t EncodeMiFlushDW<Family>::getWaSize(const EncodeDummyBlitWaArgs &waArgs) {
    return BlitCommandsHelper<Family>::getDummyBlitSize(waArgs);
}

template <typename Family>
size_t EncodeMiFlushDW<Family>::getCommandSizeWithWa(const EncodeDummyBlitWaArgs &waArgs) {
    return sizeof(typename Family::MI_FLUSH_DW) + EncodeMiFlushDW<Family>::getWaSize(waArgs);
}

template <typename Family>
void EncodeMiArbCheck<Family>::program(LinearStream &commandStream, std::optional<bool> preParserDisable) {
    MI_ARB_CHECK cmd = Family::cmdInitArbCheck;

    EncodeMiArbCheck<Family>::adjust(cmd, preParserDisable);
    auto miArbCheckStream = commandStream.getSpaceForCmd<MI_ARB_CHECK>();
    *miArbCheckStream = cmd;
}

template <typename Family>
size_t EncodeMiArbCheck<Family>::getCommandSize() { return sizeof(MI_ARB_CHECK); }

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
                                                           bool workloadPartitionOffset,
                                                           void **outCmdPtr) {
    auto miStoreDataImmBuffer = commandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    if (outCmdPtr != nullptr) {
        *outCmdPtr = miStoreDataImmBuffer;
    }
    EncodeStoreMemory<Family>::programStoreDataImm(miStoreDataImmBuffer,
                                                   gpuAddress,
                                                   dataDword0,
                                                   dataDword1,
                                                   storeQword,
                                                   workloadPartitionOffset);
}

template <typename Family>
void EncodeMiPredicate<Family>::encode(LinearStream &cmdStream, [[maybe_unused]] MiPredicateType predicateType) {
    if constexpr (Family::isUsingMiSetPredicate) {
        using MI_SET_PREDICATE = typename Family::MI_SET_PREDICATE;
        using PREDICATE_ENABLE = typename MI_SET_PREDICATE::PREDICATE_ENABLE;

        auto miSetPredicate = Family::cmdInitSetPredicate;
        miSetPredicate.setPredicateEnable(static_cast<PREDICATE_ENABLE>(predicateType));

        *cmdStream.getSpaceForCmd<MI_SET_PREDICATE>() = miSetPredicate;
    }
}

template <typename Family>
void EnodeUserInterrupt<Family>::encode(LinearStream &commandStream) {
    *commandStream.getSpaceForCmd<typename Family::MI_USER_INTERRUPT>() = Family::cmdInitUserInterrupt;
}

template <typename Family>
bool EncodeSurfaceState<Family>::isBindingTablePrefetchPreferred() {
    return false;
}

template <typename Family>
void EncodeComputeMode<Family>::adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor) {

    PipelineSelectArgs pipelineSelectArgs;
    pipelineSelectArgs.systolicPipelineSelectMode = kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode;
    pipelineSelectArgs.systolicPipelineSelectSupport = container.systolicModeSupportRef();

    PreambleHelper<Family>::programPipelineSelect(container.getCommandStream(),
                                                  pipelineSelectArgs,
                                                  container.getDevice()->getRootDeviceEnvironment());
}

} // namespace NEO
