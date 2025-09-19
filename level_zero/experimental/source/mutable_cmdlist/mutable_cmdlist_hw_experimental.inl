/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/const_stringref.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/mutable_cmdlist/helper.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_kernel_ext.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/core/source/mutable_cmdlist/usage.h"
#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/experimental/source/mutable_cmdlist/label.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_encoder.h"

#include "encode_surface_state_args.h"

#include <cstddef>
#include <optional>

namespace L0 {
namespace MCL {
constexpr uint32_t regToMMIO(MclAluReg reg) {
    if (reg >= MclAluReg::mclAluRegGpr0 && reg < MclAluReg::mclAluRegGprMax) {
        return 0x2600 + 4 * static_cast<uint32_t>(reg);
    } else if (reg == MclAluReg::mclAluRegPredicate1) {
        return 0x241C;
    } else if (reg == MclAluReg::mclAluRegPredicate2) {
        return 0x23BC;
    } else if (reg == MclAluReg::mclAluRegPredicateResult) {
        return 0x23B8;
    }
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMILoadRegImm(MclAluReg reg, uint32_t value) {
    NEO::LriHelper<GfxFamily>::program(this->commandContainer.getCommandStream(),
                                       regToMMIO(reg),
                                       value,
                                       false,
                                       this->isCopyOnly(false));
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMILoadRegReg(MclAluReg dstReg, MclAluReg srcReg) {
    NEO::EncodeSetMMIO<GfxFamily>::encodeREG(this->commandContainer, regToMMIO(dstReg), regToMMIO(srcReg), this->isCopyOnly(false));
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMILoadRegMem(MclAluReg reg, uint64_t address) {
    NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(this->commandContainer, regToMMIO(reg), address, this->isCopyOnly(false));
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMIStoreRegMem(MclAluReg reg, uint64_t address) {
    NEO::EncodeStoreMMIO<GfxFamily>::encode(*this->commandContainer.getCommandStream(), regToMMIO(reg), address, false, nullptr, this->isCopyOnly(false));
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMIMath(void *aluArray, size_t aluCount) {
    using MI_MATH = typename GfxFamily::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    constexpr int32_t aluRegisterRSrca = 0x20;
    constexpr int32_t aluRegisterRSrcb = 0x21;

    // Add instruction MI_MATH with 4 MI_MATH_ALU_INST_INLINE operands
    auto pCmd3 = reinterpret_cast<uint32_t *>(this->commandContainer.getCommandStream()->getSpace(sizeof(MI_MATH) + aluCount * 4 * sizeof(MI_MATH_ALU_INST_INLINE)));
    MI_MATH miMath;
    miMath.DW0.Value = 0x0;
    miMath.DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    miMath.DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    // 0x3 - 5 Dwords length cmd (-2): 1 for MI_MATH, 4 for MI_MATH_ALU_INST_INLINE
    miMath.DW0.BitField.DwordLength = static_cast<uint32_t>(aluCount * 4 - 1);
    *reinterpret_cast<MI_MATH *>(pCmd3) = miMath;
    pCmd3++;
    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(pCmd3);
    MI_MATH_ALU_INST_INLINE aluParam;

    zex_mcl_alu_operation_t *exAluArray = reinterpret_cast<zex_mcl_alu_operation_t *>(aluArray);
    for (size_t loop = 0; loop < aluCount; loop++, exAluArray++) {
        aluParam.DW0.Value = 0x0;
        // Setup first operand of MI_MATH - load regSource1 into register A
        if (exAluArray->regSource1 != zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_NONE) {
            switch (exAluArray->regSource1) {
            case zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_CONST0:
                aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_LOAD0);
                break;
            case zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_CONST1:
                aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_LOAD1);
                break;
            default:
                aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_LOAD);
                break;
            }

            aluParam.DW0.BitField.Operand1 = aluRegisterRSrca;
            aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(exAluArray->regSource1) / 2;

        } else {
            aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_NOOP);
            aluParam.DW0.BitField.Operand1 = 0;
            aluParam.DW0.BitField.Operand2 = 0;
        }
        *pAluParam = aluParam;
        pAluParam++;

        aluParam.DW0.Value = 0x0;
        // Setup second operand of MI_MATH - load regSource2 into register B
        if (exAluArray->regSource2 != zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_NONE) {
            switch (exAluArray->regSource2) {
            case zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_CONST0:
                aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_LOAD0);
                break;
            case zex_mcl_alu_reg_t::ZE_MCL_ALU_REG_CONST1:
                aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_LOAD1);
                break;
            default:
                aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_LOAD);
                break;
            }
            aluParam.DW0.BitField.Operand1 = aluRegisterRSrcb;
            aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(exAluArray->regSource2) / 2;

        } else {
            aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_NOOP);
            aluParam.DW0.BitField.Operand1 = 0;
            aluParam.DW0.BitField.Operand2 = 0;
        }
        *pAluParam = aluParam;
        pAluParam++;

        // Setup third operand of MI_MATH - "Operation" on registers A and B
        aluParam.DW0.Value = 0x0;
        aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(exAluArray->opType);
        aluParam.DW0.BitField.Operand1 = 0;
        aluParam.DW0.BitField.Operand2 = 0;
        *pAluParam = aluParam;
        pAluParam++;

        aluParam.DW0.Value = 0x0;
        // Setup fourth operand of MI_MATH - store result into regDest
        if (exAluArray->flag >= zex_mcl_alu_flag_t::ZE_MCL_ALU_FLAG_NACC) {
            aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_STOREINV);
            aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(exAluArray->flag) - 0x100;
        } else {
            aluParam.DW0.BitField.ALUOpcode = static_cast<uint32_t>(zex_mcl_alu_op_type_t::ZE_MCL_ALU_OP_STORE);
            aluParam.DW0.BitField.Operand2 = static_cast<uint32_t>(exAluArray->flag);
        }
        aluParam.DW0.BitField.Operand1 = static_cast<uint32_t>(exAluArray->regDest) / 2;
        *pAluParam = aluParam;
        pAluParam++;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendJump(Label *label, const InterfaceOperandDescriptor *condition) {
    auto cs = this->commandContainer.getCommandStream();

    const auto hasCondition = (condition != nullptr);
    if (hasCondition) {
        if (condition->memory != nullptr) {
            if (condition->flags & InterfaceOperandDescriptor::Flags::usesVariable) {
                auto variable = reinterpret_cast<Variable *>(condition->memory);
                if (false == variable->isType(VariableType::buffer)) {
                    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                }

                appendMILoadRegVariable(MclAluReg::mclAluRegPredicate2, variable);
            } else {
                auto memAddr = reinterpret_cast<GpuAddress>(condition->memory) + condition->offset;
                NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(*cs, regToMMIO(MclAluReg::mclAluRegPredicate2), memAddr, getBase()->isCopyOnly(false));
            }
        } else {
            auto regMMIO = regToMMIO(static_cast<MclAluReg>(condition->offset));
            NEO::EncodeSetMMIO<GfxFamily>::encodeREG(*cs, regToMMIO(MclAluReg::mclAluRegPredicate2), regMMIO, getBase()->isCopyOnly(false));
        }

        if (condition->flags & InterfaceOperandDescriptor::Flags::jumpOnClear) {
            this->appendSetPredicate(NEO::MiPredicateType::noopOnResult2Set);
        } else {
            this->appendSetPredicate(NEO::MiPredicateType::noopOnResult2Clear);
        }
    }

    GpuAddress jumpAddress = dirtyAddress;
    if (label->isSet()) {
        jumpAddress = label->getAddress();
    }

    CommandBufferOffset jumpAddressOffset = cs->getUsed() + sizeof(uint32_t);

    NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(cs, jumpAddress, !this->base->getCmdListBatchBufferFlag(), false, hasCondition);

    label->addUsage(jumpAddressOffset);
    if (hasCondition) {
        this->appendSetPredicate(NEO::MiPredicateType::disable);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendSetPredicate(NEO::MiPredicateType predicateType) {
    NEO::EncodeMiPredicate<GfxFamily>::encode(*this->commandContainer.getCommandStream(), predicateType);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendVariableLaunchKernel(Kernel *kernel, Variable *groupCount, Event *signalEvent, uint32_t numWaitEvents, ze_event_handle_t *waitEvents) {
    bool relaxedOrderingDispatch = false;
    ze_result_t ret = CommandListCoreFamily<gfxCoreFamily>::addEventsToCmdList(numWaitEvents, waitEvents, nullptr, relaxedOrderingDispatch, true, true, false, false);
    if (ret) {
        return ret;
    }

    CmdListKernelLaunchParams launchParams = {};
    launchParams.skipInOrderNonWalkerSignaling = base->skipInOrderNonWalkerSignalingAllowed(signalEvent->toHandle());
    launchParams.omitAddingKernelArgumentResidency = true;
    launchParams.relaxedOrderingDispatch = relaxedOrderingDispatch;

    auto &residencyContainer = kernel->getArgumentsResidencyContainer();
    for (auto resource : residencyContainer) {
        addToResidencyContainer(resource);
    }

    ze_group_count_t threadGroupDimensions = {1U, 1U, 1U};
    ret = appendLaunchKernelWithParams(kernel, threadGroupDimensions,
                                       signalEvent, launchParams);
    if (ret) {
        return ret;
    }

    auto &kernelExt = MclKernelExt::get(kernel);
    Variable *groupSize = kernelExt.getGroupSizeVariable();

    auto dispatch = (*dispatches.rbegin()).get();
    uint32_t initialGroupCount[3] = {threadGroupDimensions.groupCountX, threadGroupDimensions.groupCountY, threadGroupDimensions.groupCountZ};
    MutableKernelDispatchParameters dispatchParams = {
        initialGroupCount,                                             // groupCount
        static_cast<L0::KernelImp *>(kernel)->getGroupSize(),          // groupSize
        static_cast<L0::KernelImp *>(kernel)->getGlobalOffsets(),      // globalOffset
        kernel->getPerThreadDataSizeForWholeThreadGroup(),             // perThreadSize
        kernel->getRequiredWorkgroupOrder(),                           // walkOrder
        kernel->getNumThreadsPerThreadGroup(),                         // numThreadsPerThreadGroup
        kernel->getThreadExecutionMask(),                              // threadExecutionMask
        kernel->getMaxWgCountPerTile(getBase()->getEngineGroupType()), // maxWorkGroupCountPerTile
        0,                                                             // maxCooperativeGroupCount
        NEO::localRegionSizeParamNotSet,                               // localRegionSize
        NEO::RequiredPartitionDim::none,                               // requiredPartitionDim
        NEO::RequiredDispatchWalkOrder::none,                          // requiredDispatchWalkOrder
        kernel->requiresGenerationOfLocalIdsByRuntime(),               // generationOfLocalIdsByRuntime
        false};                                                        // cooperativeDispatch

    auto mutableCommandWalker = (*mutableWalkerCmds.rbegin()).get();
    ret = addVariableDispatch(kernel->getKernelDescriptor(), *dispatch,
                              groupSize, groupCount, nullptr, nullptr,
                              mutableCommandWalker, dispatchParams);

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMILoadRegVariable(MclAluReg reg, Variable *variable) {
    auto cs = this->commandContainer.getCommandStream();
    CommandBufferOffset offset = cs->getUsed() + 2 * sizeof(uint32_t);
    CommandBufferOffset fullOffset = reinterpret_cast<CommandBufferOffset>(ptrOffset(cs->getCpuBase(), offset));
    NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(*cs, regToMMIO(reg), dirtyAddress, getBase()->isCopyOnly(false));

    auto retVal = variable->addCsUsage(offset, fullOffset);
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t MutableCommandListCoreFamily<gfxCoreFamily>::appendMIStoreRegVariable(MclAluReg reg, Variable *variable) {
    auto cs = this->commandContainer.getCommandStream();
    CommandBufferOffset offset = cs->getUsed() + 2 * sizeof(uint32_t);
    CommandBufferOffset fullOffset = reinterpret_cast<CommandBufferOffset>(ptrOffset(cs->getCpuBase(), offset));
    NEO::EncodeStoreMMIO<GfxFamily>::encode(*cs, regToMMIO(reg), dirtyAddress, false, nullptr, getBase()->isCopyOnly(false));

    auto retVal = variable->addCsUsage(offset, fullOffset);
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void MutableCommandListCoreFamily<gfxCoreFamily>::programStateBaseAddressHook(size_t cmdBufferOffset, bool surfaceBaseAddressModify) {
    StateBaseAddressOffsets sbaOffsets{};
    sbaOffsets.gsba = cmdBufferOffset + 1 * sizeof(uint32_t);
    sbaOffsets.isba = cmdBufferOffset + 10 * sizeof(uint32_t);
    if (surfaceBaseAddressModify) {
        sbaOffsets.ssba = cmdBufferOffset + 4 * sizeof(uint32_t);
    }
    sbaVec.push_back(sbaOffsets);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void MutableCommandListCoreFamily<gfxCoreFamily>::setBufferSurfaceState(void *address, NEO::GraphicsAllocation *alloc, Variable *variable) {
    constexpr auto sshAlignmentMask = NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignmentMask();
    constexpr auto sshAlignment = NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment();

    if (this->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState) == nullptr || alloc == nullptr) {
        return;
    }

    const auto &bufferUsages = variable->getBufferUsages();
    if (bufferUsages.bufferOffset.size() == 0 &&
        bufferUsages.bindful.size() == 0 &&
        bufferUsages.bindfulWithoutOffset.size() == 0) {
        return;
    }

    const CpuAddress iohCpuBase = reinterpret_cast<CpuAddress>(this->commandContainer.getIndirectHeap(NEO::HeapType::indirectObject)->getCpuBase());
    const CpuAddress sshCpuBase = reinterpret_cast<CpuAddress>(this->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState)->getCpuBase());

    GpuAddress baseAddress = alloc->getGpuAddressToPatch() & sshAlignmentMask;
    auto misalignedSize = ptrDiff(alloc->getGpuAddressToPatch(), baseAddress);
    uint32_t bufferOffset = static_cast<uint32_t>(ptrDiff(address, reinterpret_cast<void *>(baseAddress)));
    size_t bufferSize = alloc->getUnderlyingBufferSize();

    NEO::Device *neoDevice = this->device->getNEODevice();

    bool l3Enabled = true;
    auto allocData = this->device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(alloc->getGpuAddress()));
    if (allocData && allocData->allocationFlagsProperty.flags.locallyUncachedResource) {
        l3Enabled = false;
    }
    const auto mocs = this->device->getMOCS(l3Enabled, false);
    const auto numAvailableDevices = neoDevice->getNumGenericSubDevices();
    auto gmmHelper = neoDevice->getGmmHelper();

    for (auto offsetInIOH : bufferUsages.bufferOffset) {
        auto patchLocation = ptrOffset(iohCpuBase, offsetInIOH);
        *reinterpret_cast<uint32_t *>(patchLocation) = bufferOffset;
    }

    auto setSurfaceState = [&mocs, &numAvailableDevices, &alloc, &gmmHelper, &neoDevice](CpuAddress surfaceStateAddress, GpuAddress bufferAddressForSsh, size_t bufferSizeForSsh) {
        auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;
        auto isDebuggerActive = neoDevice->getDebugger() != nullptr;
        NEO::EncodeSurfaceStateArgs args;
        args.outMemory = &surfaceState;
        args.graphicsAddress = bufferAddressForSsh;
        args.size = bufferSizeForSsh;
        args.mocs = mocs;
        args.numAvailableDevices = numAvailableDevices;
        args.allocation = alloc;
        args.gmmHelper = gmmHelper;
        args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;
        args.isDebuggerActive = isDebuggerActive;
        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);

        *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateAddress) = surfaceState;
    };
    for (auto bindfulOffset : bufferUsages.bindful) {
        CpuAddress surfaceStateAddress = ptrOffset(sshCpuBase, bindfulOffset);
        GpuAddress bufferAddressForSsh = baseAddress;
        size_t bufferSizeForSsh = alignUp(bufferSize + misalignedSize, sshAlignment);
        setSurfaceState(surfaceStateAddress, bufferAddressForSsh, bufferSizeForSsh);
    }

    for (auto bindfulOffset : bufferUsages.bindfulWithoutOffset) {
        CpuAddress surfaceStateAddress = ptrOffset(sshCpuBase, bindfulOffset);
        GpuAddress bufferAddressForSsh = reinterpret_cast<GpuAddress>(address);
        DEBUG_BREAK_IF(bufferAddressForSsh != (bufferAddressForSsh & sshAlignmentMask));
        size_t bufferSizeForSsh = alignUp(bufferSize - bufferOffset + misalignedSize, sshAlignment);
        setSurfaceState(surfaceStateAddress, bufferAddressForSsh, bufferSizeForSsh);
    }

    DEBUG_BREAK_IF(bufferUsages.bindless.size() > 0U);
    DEBUG_BREAK_IF(bufferUsages.bindlessWithoutOffset.size() > 0U);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline MutableComputeWalker *MutableCommandListCoreFamily<gfxCoreFamily>::getCommandWalker(CommandBufferOffset offsetToWalkerCommand, uint8_t indirectOffset, uint8_t scratchOffset) {
    void *walkerCpuBuffer = MutableComputeWalkerHw<GfxFamily>::createCommandBuffer();
    void *walkerCmd = ptrOffset(this->base->getCmdContainer().getCommandStream()->getCpuBase(), offsetToWalkerCommand);

    std::memcpy(walkerCpuBuffer, walkerCmd, MutableComputeWalkerHw<GfxFamily>::getCommandSize());
    return new MutableComputeWalkerHw<GfxFamily>(walkerCmd, indirectOffset, scratchOffset, walkerCpuBuffer,
                                                 this->hasStageCommitVariables);
}

} // namespace MCL
} // namespace L0
