/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/command_stream/command_stream_aub_fixture.h"

#include <type_traits>
#include <vector>

using namespace NEO;

namespace L0 {
namespace ult {

enum class NewAluOpcodes : uint32_t {
    opcodeShr = 0x106,
    opcodeSar = 0x107
};

struct MiMath : public CommandStreamAubFixture, public ::testing::Test {
    void SetUp() override {
        CommandStreamAubFixture::setUp();
    }
    void TearDown() override {
        CommandStreamAubFixture::tearDown();
    }

    uint32_t getPartOfGPUAddress(uint64_t address, bool lowPart) {
        constexpr uint32_t shift = 32u;
        constexpr uint32_t mask = 0xffffffff;
        if (lowPart) {
            return static_cast<uint32_t>(address & mask);
        } else {
            return static_cast<uint32_t>(address >> shift);
        }
    }
    template <typename FamilyType>
    void loadValueToRegister(int32_t value, int32_t reg) {
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
        MI_LOAD_REGISTER_IMM cmd = FamilyType::cmdInitLoadRegisterImm;
        cmd.setDataDword(value);
        cmd.setRegisterOffset(reg);
        cmd.setMmioRemapEnable(1);
        auto buffer = this->taskStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM));
        *static_cast<MI_LOAD_REGISTER_IMM *>(buffer) = cmd;
    }
    template <typename FamilyType>
    void storeValueInRegisterToMemory(int64_t address, int32_t reg) {
        using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
        MI_STORE_REGISTER_MEM cmd2 = FamilyType::cmdInitStoreRegisterMem;
        cmd2.setRegisterAddress(reg);
        cmd2.setMemoryAddress(address);
        cmd2.setMmioRemapEnable(1);
        auto buffer2 = this->taskStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
        *static_cast<MI_STORE_REGISTER_MEM *>(buffer2) = cmd2;
    }
    template <typename FamilyType>
    void loadAddressToRegisters(uint32_t registerWithLowPart, uint32_t registerWithHighPart, uint32_t registerWithShift, uint64_t address) {
        this->loadValueToRegister<FamilyType>(this->getPartOfGPUAddress(address, true), registerWithLowPart);
        this->loadValueToRegister<FamilyType>(this->getPartOfGPUAddress(address, false), registerWithHighPart);
        this->loadValueToRegister<FamilyType>(32u, registerWithShift);
    }
    template <typename FamilyType>
    void loadAddressToMiMathAccu(uint32_t lowAddressRegister, uint32_t highAddressRegister, uint32_t shiftReg) {
        using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
        MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(this->taskStream->getSpace(this->numberOfOperationToLoadAddressToMiMathAccu * sizeof(MI_MATH_ALU_INST_INLINE)));
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
        pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
        pAluParam->DW0.BitField.Operand2 = highAddressRegister;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
        pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
        pAluParam->DW0.BitField.Operand2 = shiftReg;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeShl);
        pAluParam->DW0.BitField.Operand1 = 0;
        pAluParam->DW0.BitField.Operand2 = 0;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
        pAluParam->DW0.BitField.Operand1 = highAddressRegister;
        pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
        pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
        pAluParam->DW0.BitField.Operand2 = highAddressRegister;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
        pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
        pAluParam->DW0.BitField.Operand2 = lowAddressRegister;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeOr);
        pAluParam->DW0.BitField.Operand1 = 0;
        pAluParam->DW0.BitField.Operand2 = 0;
    }

    static constexpr size_t bufferSize = MemoryConstants::pageSize;
    const uint32_t numberOfOperationToLoadAddressToMiMathAccu = 7;
};

using MatcherIsDg2OrPvc = IsWithinProducts<IGFX_DG2, IGFX_PVC>;

HWTEST2_F(MiMath, givenLoadIndirectFromMemoryWhenUseMiMathToSimpleOperationThenStoreStateOfRegisterIndirectToMemory, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    uint64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = 1u;

    auto allocation = this->createBufferWithData(bufferMemory, bufferSize);
    ASSERT_NE(nullptr, allocation);

    uint32_t valueToAdd = 5u;
    uint64_t valueAfterMiMathOperation = bufferMemory[0] + valueToAdd;

    this->loadAddressToRegisters<FamilyType>(RegisterOffsets::csGprR0, RegisterOffsets::csGprR1, RegisterOffsets::csGprR2, allocation->getGpuAddress());
    this->loadValueToRegister<FamilyType>(valueToAdd, RegisterOffsets::csGprR3);

    auto pCmd = reinterpret_cast<uint32_t *>(this->taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = this->numberOfOperationToLoadAddressToMiMathAccu + 13 - 1;
    this->loadAddressToMiMathAccu<FamilyType>(static_cast<uint32_t>(AluRegisters::gpr0), static_cast<uint32_t>(AluRegisters::gpr1), static_cast<uint32_t>(AluRegisters::gpr2));
    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(this->taskStream->getSpace(13 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeFenceRd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoadind);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeFenceRd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr3);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeAdd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad0);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeAdd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeFenceRd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStoreind);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr1);

    this->flushStream();

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress()), &valueAfterMiMathOperation, sizeof(valueAfterMiMathOperation));
}
HWTEST2_F(MiMath, givenLoadIndirectFromMemoryWhenUseMiMathThenStoreIndirectToAnotherMemory, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    uint64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = 1u;
    uint64_t bufferBMemory[bufferSize] = {};

    auto buffer = this->createBufferWithData(bufferMemory, bufferSize);
    ASSERT_NE(nullptr, buffer);
    auto bufferB = this->createBufferWithData(bufferBMemory, bufferSize);
    ASSERT_NE(nullptr, bufferB);

    this->loadAddressToRegisters<FamilyType>(RegisterOffsets::csGprR0, RegisterOffsets::csGprR1, RegisterOffsets::csGprR2, buffer->getGpuAddress());
    this->loadAddressToRegisters<FamilyType>(RegisterOffsets::csGprR3, RegisterOffsets::csGprR4, RegisterOffsets::csGprR2, bufferB->getGpuAddress());

    auto pCmd = reinterpret_cast<uint32_t *>(this->taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = this->numberOfOperationToLoadAddressToMiMathAccu * 2 + 6 - 1;

    this->loadAddressToMiMathAccu<FamilyType>(static_cast<uint32_t>(AluRegisters::gpr0), static_cast<uint32_t>(AluRegisters::gpr1), static_cast<uint32_t>(AluRegisters::gpr2));

    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(this->taskStream->getSpace(3 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeFenceRd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoadind);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeFenceRd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;

    this->loadAddressToMiMathAccu<FamilyType>(static_cast<uint32_t>(AluRegisters::gpr3), static_cast<uint32_t>(AluRegisters::gpr4), static_cast<uint32_t>(AluRegisters::gpr2));

    pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(this->taskStream->getSpace(3 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeFenceRd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStoreind);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeFenceRd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;

    this->flushStream();

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(bufferB->getGpuAddress()), bufferMemory, sizeof(uint64_t));
}
HWTEST2_F(MiMath, givenValueToMakeLeftLogicalShiftWhenUseMiMathThenShiftIsDoneProperly, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    uint64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = 1u;

    auto buffer = this->createBufferWithData(bufferMemory, bufferSize);
    ASSERT_NE(nullptr, buffer);

    uint32_t value = 1u;
    uint32_t shift = 2u;
    uint32_t notPowerOfTwoShift = 5u;
    uint32_t expectedUsedShift = 4u;

    this->loadValueToRegister<FamilyType>(value, RegisterOffsets::csGprR0);
    this->loadValueToRegister<FamilyType>(shift, RegisterOffsets::csGprR1);
    this->loadValueToRegister<FamilyType>(notPowerOfTwoShift, RegisterOffsets::csGprR2);
    auto pCmd = reinterpret_cast<uint32_t *>(this->taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = 7 - 1;

    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(this->taskStream->getSpace(7 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeShl);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeShl);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);

    this->storeValueInRegisterToMemory<FamilyType>(buffer->getGpuAddress(), RegisterOffsets::csGprR1);
    this->storeValueInRegisterToMemory<FamilyType>(buffer->getGpuAddress() + 4, RegisterOffsets::csGprR2);
    this->flushStream();

    uint32_t firstShift = value << shift;
    uint32_t secondShift = value << notPowerOfTwoShift;
    uint32_t executeSecondShift = value << expectedUsedShift;

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(buffer->getGpuAddress()), &firstShift, sizeof(firstShift));
    this->expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(buffer->getGpuAddress() + 4), &secondShift, sizeof(secondShift));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(buffer->getGpuAddress() + 4), &executeSecondShift, sizeof(executeSecondShift));
}
HWTEST2_F(MiMath, givenValueToMakeRightLogicalShiftWhenUseMiMathThenShiftIsDoneProperly, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    uint64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = 1u;

    auto allocation = this->createBufferWithData(bufferMemory, bufferSize);
    ASSERT_NE(nullptr, allocation);

    uint32_t value = 32u;
    uint32_t shift = 2u;
    uint32_t notPowerOfTwoShift = 5u;
    uint32_t expectedUsedShift = 4u;

    this->loadValueToRegister<FamilyType>(value, RegisterOffsets::csGprR0);
    this->loadValueToRegister<FamilyType>(shift, RegisterOffsets::csGprR1);
    this->loadValueToRegister<FamilyType>(notPowerOfTwoShift, RegisterOffsets::csGprR2);
    auto pCmd = reinterpret_cast<uint32_t *>(this->taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = 7 - 1;

    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(this->taskStream->getSpace(7 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeShr);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeShr);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);

    this->storeValueInRegisterToMemory<FamilyType>(allocation->getGpuAddress(), RegisterOffsets::csGprR1);
    this->storeValueInRegisterToMemory<FamilyType>(allocation->getGpuAddress() + 4, RegisterOffsets::csGprR2);
    this->flushStream();

    uint32_t firstShift = value >> shift;
    uint32_t secondShift = value >> notPowerOfTwoShift;
    uint32_t executeSecondShift = value >> expectedUsedShift;

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress()), &firstShift, sizeof(firstShift));
    this->expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress() + 4), &secondShift, sizeof(secondShift));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress() + 4), &executeSecondShift, sizeof(executeSecondShift));
}
HWTEST2_F(MiMath, givenValueToMakeRightArithmeticShiftWhenUseMiMathThenShiftIsDoneProperly, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    int64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = -32;

    auto allocation = this->createBufferWithData(bufferMemory, bufferSize);
    ASSERT_NE(nullptr, allocation);

    uint32_t shift = 2u;
    uint32_t notPowerOfTwoShift = 5u;
    uint32_t expectedUsedShift = 4u;

    this->loadAddressToRegisters<FamilyType>(RegisterOffsets::csGprR0, RegisterOffsets::csGprR1, RegisterOffsets::csGprR2, allocation->getGpuAddress());
    this->loadValueToRegister<FamilyType>(shift, RegisterOffsets::csGprR4);
    this->loadValueToRegister<FamilyType>(notPowerOfTwoShift, RegisterOffsets::csGprR5);

    auto pCmd = reinterpret_cast<uint32_t *>(this->taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = this->numberOfOperationToLoadAddressToMiMathAccu + 9 - 1;
    this->loadAddressToMiMathAccu<FamilyType>(static_cast<uint32_t>(AluRegisters::gpr0), static_cast<uint32_t>(AluRegisters::gpr1), static_cast<uint32_t>(AluRegisters::gpr2));
    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(this->taskStream->getSpace(9 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoadind);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr3);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeFenceRd);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr3);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr4);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeSar);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr4);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr5);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeSar);
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr5);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);

    this->storeValueInRegisterToMemory<FamilyType>(allocation->getGpuAddress(), RegisterOffsets::csGprR4);
    this->storeValueInRegisterToMemory<FamilyType>(allocation->getGpuAddress() + 4, RegisterOffsets::csGprR5);
    this->flushStream();

    int64_t firstShift = bufferMemory[0];
    for (uint32_t i = 0; i < shift; i++) {
        firstShift /= 2;
    }
    int64_t secondShift = bufferMemory[0];
    for (uint32_t i = 0; i < notPowerOfTwoShift; i++) {
        secondShift /= 2;
    }
    int64_t executeSecondShift = bufferMemory[0];
    for (uint32_t i = 0; i < expectedUsedShift; i++) {
        executeSecondShift /= 2;
    }

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress()), &firstShift, sizeof(uint32_t));
    this->expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress() + 4), &secondShift, sizeof(uint32_t));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress() + 4), &executeSecondShift, sizeof(uint32_t));
}

template <typename CompareDataT>
struct ConditionalBbStartTests : public MiMath {
    using TestCompareDataT = CompareDataT;

    void SetUp() override {
        MiMath::SetUp();

        std::vector<CompareDataT> bufferMemory;
        bufferMemory.resize(this->compareBufferSize);

        if constexpr (isQwordData) {
            this->baseCompareValue = 0x1'0000'0000;
        }

        std::fill(bufferMemory.begin(), bufferMemory.end(), this->baseCompareValue);

        bufferMemory[1] += 5;
        bufferMemory[2] -= 5;

        this->buffer = this->createBufferWithData(bufferMemory.data(), this->compareBufferSize * sizeof(CompareDataT));
        ASSERT_NE(nullptr, this->buffer);

        this->baseGpuVa = this->buffer->getGpuAddress();
        this->baseWriteGpuVa = this->baseGpuVa + (sizeof(CompareDataT) * this->numCompareModes);
    }

    template <typename AtomicT>
    typename AtomicT::ATOMIC_OPCODES getAtomicOpcode() const {
        return isQwordData ? AtomicT::ATOMIC_OPCODES::ATOMIC_8B_INCREMENT : AtomicT::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT;
    }

    template <typename AtomicT>
    typename AtomicT::DATA_SIZE getDataSize() const {
        return isQwordData ? AtomicT::DATA_SIZE::DATA_SIZE_QWORD : AtomicT::DATA_SIZE::DATA_SIZE_DWORD;
    }

    template <typename FamilyType>
    void whenDispatchingEqualModeThenResultsAreValidImpl();

    template <typename FamilyType>
    void whenDispatchingNotEqualModeThenResultsAreValidImpl();

    template <typename FamilyType>
    void whenDispatchingGreaterOrEqualModeThenResultsAreValidImpl();

    template <typename FamilyType>
    void whenDispatchingLessModeThenResultsAreValidImpl();

    uint64_t baseGpuVa = 0;
    uint64_t baseWriteGpuVa = 0;
    uint64_t invalidGpuVa = 0x1230000;
    uint32_t numCompareModes = 3;
    const size_t compareBufferSize = numCompareModes * 3;
    CompareDataT baseCompareValue = 10;
    NEO::GraphicsAllocation *buffer = nullptr;
    static constexpr bool isQwordData = std::is_same<uint64_t, TestCompareDataT>::value;
};

using ConditionalBbStartTests32b = ConditionalBbStartTests<uint32_t>;
using ConditionalBbStartTests64b = ConditionalBbStartTests<uint64_t>;

template <typename T>
template <typename FamilyType>
void ConditionalBbStartTests<T>::whenDispatchingEqualModeThenResultsAreValidImpl() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    {
        uint64_t jumpAddress = this->taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, jumpAddress, this->baseGpuVa, this->baseCompareValue, NEO::CompareOperation::equal, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*this->taskStream);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa,
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, this->invalidGpuVa, this->baseGpuVa + sizeof(TestCompareDataT), this->baseCompareValue, NEO::CompareOperation::equal, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa + sizeof(TestCompareDataT),
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, this->invalidGpuVa, this->baseGpuVa + (sizeof(TestCompareDataT) * 2), this->baseCompareValue, NEO::CompareOperation::equal, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa + (sizeof(TestCompareDataT) * 2),
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    this->flushStream();

    TestCompareDataT expectedValue = this->baseCompareValue + 1;
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa), &expectedValue, sizeof(TestCompareDataT));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa + sizeof(TestCompareDataT)), &expectedValue, sizeof(TestCompareDataT));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa + (sizeof(TestCompareDataT) * 2)), &expectedValue, sizeof(TestCompareDataT));
}

HWTEST2_F(ConditionalBbStartTests32b, whenDispatchingEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    this->whenDispatchingEqualModeThenResultsAreValidImpl<FamilyType>();
}

HWTEST2_F(ConditionalBbStartTests64b, whenDispatchingEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    this->whenDispatchingEqualModeThenResultsAreValidImpl<FamilyType>();
}

template <typename T>
template <typename FamilyType>
void ConditionalBbStartTests<T>::whenDispatchingNotEqualModeThenResultsAreValidImpl() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, this->invalidGpuVa, this->baseGpuVa, this->baseCompareValue, NEO::CompareOperation::notEqual, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa,
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    {
        uint64_t jumpAddress = this->taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, jumpAddress, this->baseGpuVa + sizeof(TestCompareDataT), this->baseCompareValue, NEO::CompareOperation::notEqual, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*this->taskStream);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa + sizeof(TestCompareDataT),
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    {
        uint64_t jumpAddress = this->taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, jumpAddress, this->baseGpuVa + (sizeof(TestCompareDataT) * 2), this->baseCompareValue, NEO::CompareOperation::notEqual, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*this->taskStream);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa + (sizeof(TestCompareDataT) * 2),
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    this->flushStream();

    TestCompareDataT expectedValue = this->baseCompareValue + 1;
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa), &expectedValue, sizeof(TestCompareDataT));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa + sizeof(TestCompareDataT)), &expectedValue, sizeof(TestCompareDataT));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa + (sizeof(TestCompareDataT) * 2)), &expectedValue, sizeof(TestCompareDataT));
}

HWTEST2_F(ConditionalBbStartTests32b, whenDispatchingNotEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    this->whenDispatchingNotEqualModeThenResultsAreValidImpl<FamilyType>();
}

HWTEST2_F(ConditionalBbStartTests64b, whenDispatchingNotEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    this->whenDispatchingNotEqualModeThenResultsAreValidImpl<FamilyType>();
}

template <typename T>
template <typename FamilyType>
void ConditionalBbStartTests<T>::whenDispatchingGreaterOrEqualModeThenResultsAreValidImpl() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    {
        uint64_t jumpAddress = this->taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, jumpAddress, this->baseGpuVa, this->baseCompareValue, NEO::CompareOperation::greaterOrEqual, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*this->taskStream);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa,
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    {
        uint64_t jumpAddress = this->taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, jumpAddress, this->baseGpuVa + sizeof(TestCompareDataT), this->baseCompareValue, NEO::CompareOperation::greaterOrEqual, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*this->taskStream);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa + sizeof(TestCompareDataT),
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, this->invalidGpuVa, this->baseGpuVa + (sizeof(TestCompareDataT) * 2), this->baseCompareValue, NEO::CompareOperation::greaterOrEqual, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa + (sizeof(TestCompareDataT) * 2),
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    this->flushStream();

    TestCompareDataT expectedValue = this->baseCompareValue + 1;
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa), &expectedValue, sizeof(TestCompareDataT));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa + sizeof(TestCompareDataT)), &expectedValue, sizeof(TestCompareDataT));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa + (sizeof(TestCompareDataT) * 2)), &expectedValue, sizeof(TestCompareDataT));
}

HWTEST2_F(ConditionalBbStartTests32b, whenDispatchingGreaterOrEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    this->whenDispatchingGreaterOrEqualModeThenResultsAreValidImpl<FamilyType>();
}

HWTEST2_F(ConditionalBbStartTests64b, whenDispatchingGreaterOrEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    this->whenDispatchingGreaterOrEqualModeThenResultsAreValidImpl<FamilyType>();
}

template <typename T>
template <typename FamilyType>
void ConditionalBbStartTests<T>::whenDispatchingLessModeThenResultsAreValidImpl() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, this->invalidGpuVa, this->baseGpuVa, this->baseCompareValue, NEO::CompareOperation::less, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa,
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, this->invalidGpuVa, this->baseGpuVa + sizeof(TestCompareDataT), this->baseCompareValue, NEO::CompareOperation::less, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa + sizeof(TestCompareDataT),
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    {
        uint64_t jumpAddress = this->taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*this->taskStream, jumpAddress, this->baseGpuVa + (sizeof(TestCompareDataT) * 2), this->baseCompareValue, NEO::CompareOperation::less, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*this->taskStream);

        EncodeAtomic<FamilyType>::programMiAtomic(*this->taskStream, this->baseWriteGpuVa + (sizeof(TestCompareDataT) * 2),
                                                  this->template getAtomicOpcode<MI_ATOMIC>(),
                                                  this->template getDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    this->flushStream();

    TestCompareDataT expectedValue = this->baseCompareValue + 1;
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa), &expectedValue, sizeof(TestCompareDataT));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa + sizeof(TestCompareDataT)), &expectedValue, sizeof(TestCompareDataT));
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(this->baseWriteGpuVa + (sizeof(TestCompareDataT) * 2)), &expectedValue, sizeof(TestCompareDataT));
}

HWTEST2_F(ConditionalBbStartTests32b, whenDispatchingLessModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    this->whenDispatchingLessModeThenResultsAreValidImpl<FamilyType>();
}

HWTEST2_F(ConditionalBbStartTests64b, whenDispatchingLessModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    this->whenDispatchingLessModeThenResultsAreValidImpl<FamilyType>();
}

} // namespace ult
} // namespace L0
