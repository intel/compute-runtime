/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {
enum class NewAluOpcodes : uint32_t {
    opcodeLoad = 0x080,
    opcodeLoad0 = 0x081,
    opcodeLoad1 = 0x481,
    opcodeLoadind = 0x082,
    opcodeStoreind = 0x181,
    opcodeShl = 0x105,
    opcodeShr = 0x106,
    opcodeSar = 0x107,
    opcodeFence = 0x001
};

struct MiMath : public AUBFixture, public ::testing::Test {
    void SetUp() override {
        AUBFixture::setUp(defaultHwInfo.get());

        streamAllocation = this->device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::commandBuffer, device->getDeviceBitfield()});
        taskStream = std::make_unique<LinearStream>(streamAllocation);
    }
    void TearDown() override {
        this->device->getMemoryManager()->freeGraphicsMemory(streamAllocation);
        AUBFixture::tearDown();
    }

    void flushStream() {
        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.guardCommandBufferWithPipeControl = true;

        csr->flushTask(*taskStream, 0,
                       &csr->getIndirectHeap(IndirectHeapType::dynamicState, 0u),
                       &csr->getIndirectHeap(IndirectHeapType::indirectObject, 0u),
                       &csr->getIndirectHeap(IndirectHeapType::surfaceState, 0u),
                       0u, dispatchFlags, device->getDevice());

        csr->flushBatchedSubmissions();
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
        auto buffer = taskStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM));
        *static_cast<MI_LOAD_REGISTER_IMM *>(buffer) = cmd;
    }
    template <typename FamilyType>
    void storeValueInRegisterToMemory(int64_t address, int32_t reg) {
        using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
        MI_STORE_REGISTER_MEM cmd2 = FamilyType::cmdInitStoreRegisterMem;
        cmd2.setRegisterAddress(reg);
        cmd2.setMemoryAddress(address);
        cmd2.setMmioRemapEnable(1);
        auto buffer2 = taskStream->getSpace(sizeof(MI_STORE_REGISTER_MEM));
        *static_cast<MI_STORE_REGISTER_MEM *>(buffer2) = cmd2;
    }
    template <typename FamilyType>
    void loadAddressToRegisters(uint32_t registerWithLowPart, uint32_t registerWithHighPart, uint32_t registerWithShift, uint64_t address) {
        loadValueToRegister<FamilyType>(getPartOfGPUAddress(address, true), registerWithLowPart);   // low part to R0
        loadValueToRegister<FamilyType>(getPartOfGPUAddress(address, false), registerWithHighPart); // high part to R1
        loadValueToRegister<FamilyType>(32u, registerWithShift);                                    // value to shift address
    }
    template <typename FamilyType>
    void loadAddressToMiMathAccu(uint32_t lowAddressRegister, uint32_t highAddressRegister, uint32_t shiftReg) {
        using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
        MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(taskStream->getSpace(numberOfOperationToLoadAddressToMiMathAccu * sizeof(MI_MATH_ALU_INST_INLINE)));
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load high part of address from register with older to SRCA
        pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
        pAluParam->DW0.BitField.Operand2 = highAddressRegister;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load 32 - value from shiftReg , to SRCB (to shift high part in register)
        pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
        pAluParam->DW0.BitField.Operand2 = shiftReg;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeShl); // shift high part
        pAluParam->DW0.BitField.Operand1 = 0;
        pAluParam->DW0.BitField.Operand2 = 0;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // move result to highAddressRegister
        pAluParam->DW0.BitField.Operand1 = highAddressRegister;
        pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load highAddressRegister to SRCA
        pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
        pAluParam->DW0.BitField.Operand2 = highAddressRegister;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load low part of address to SRCB
        pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
        pAluParam->DW0.BitField.Operand2 = lowAddressRegister;
        pAluParam++;
        pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeOr); // join parts of address and locate in ACCU
        pAluParam->DW0.BitField.Operand1 = 0;
        pAluParam->DW0.BitField.Operand2 = 0;
    }

    static constexpr size_t bufferSize = MemoryConstants::pageSize;
    const uint32_t numberOfOperationToLoadAddressToMiMathAccu = 7;
    std::unique_ptr<LinearStream> taskStream;
    GraphicsAllocation *streamAllocation = nullptr;
};

using MatcherIsDg2OrPvc = IsWithinProducts<IGFX_DG2, IGFX_PVC>;

HWTEST2_F(MiMath, givenLoadIndirectFromMemoryWhenUseMiMathToSimpleOperationThenStoreStateOfRegisterInirectToMemory, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    uint64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = 1u;
    cl_int retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                         CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                         bufferSize, bufferMemory, retVal));
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    csr->makeResident(*allocation);

    uint32_t valueToAdd = 5u;
    uint64_t valueAfterMiMathOperation = bufferMemory[0] + valueToAdd;

    loadAddressToRegisters<FamilyType>(RegisterOffsets::csGprR0, RegisterOffsets::csGprR1, RegisterOffsets::csGprR2, allocation->getGpuAddress()); // prepare registers to mi_math operation
    loadValueToRegister<FamilyType>(valueToAdd, RegisterOffsets::csGprR3);

    auto pCmd = reinterpret_cast<uint32_t *>(taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = numberOfOperationToLoadAddressToMiMathAccu + 13 - 1;
    loadAddressToMiMathAccu<FamilyType>(static_cast<uint32_t>(AluRegisters::gpr0), static_cast<uint32_t>(AluRegisters::gpr1), static_cast<uint32_t>(AluRegisters::gpr2)); // GPU address of buffer load to ACCU register
    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(taskStream->getSpace(13 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeFence); // to be sure that all writes and reads are completed
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeLoadind); // load dword from memory address located in ACCU
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeFence); // to be sure that all writes and reads are completed
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // copy address from ACCU to R2
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // R0 to SRCA
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // R3 to SRCB where is value of 'valueToAdd'
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr3);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeAdd); // do simple add on registers SRCA and SRCB
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // R3 to SRCB where is value of 'valueToAdd'
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load address from R2 where is copy of address to SRCA
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeLoad0);
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeAdd); // move address to ACCU
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeFence); // to be sure that all writes and reads are completed
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeStoreind); // store to memory from ACCU, value from register R1
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr1);

    flushStream();

    expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress()), &valueAfterMiMathOperation, sizeof(valueAfterMiMathOperation));
}
HWTEST2_F(MiMath, givenLoadIndirectFromMemoryWhenUseMiMathThenStoreIndirectToAnotherMemory, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    uint64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = 1u;
    uint64_t bufferBMemory[bufferSize] = {};
    cl_int retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                         CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                         bufferSize, bufferMemory, retVal));
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto bufferB = std::unique_ptr<Buffer>(Buffer::create(context,
                                                          CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                          bufferSize, bufferBMemory, retVal));
    ASSERT_NE(nullptr, bufferB);
    EXPECT_EQ(CL_SUCCESS, retVal);

    csr->makeResident(*buffer->getGraphicsAllocation(rootDeviceIndex));
    csr->makeResident(*bufferB->getGraphicsAllocation(rootDeviceIndex));

    loadAddressToRegisters<FamilyType>(RegisterOffsets::csGprR0, RegisterOffsets::csGprR1, RegisterOffsets::csGprR2, buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress());  // prepare registers to mi_math operation
    loadAddressToRegisters<FamilyType>(RegisterOffsets::csGprR3, RegisterOffsets::csGprR4, RegisterOffsets::csGprR2, bufferB->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress()); // prepare registers to mi_math operation

    auto pCmd = reinterpret_cast<uint32_t *>(taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = numberOfOperationToLoadAddressToMiMathAccu * 2 + 6 - 1;

    loadAddressToMiMathAccu<FamilyType>(static_cast<uint32_t>(AluRegisters::gpr0), static_cast<uint32_t>(AluRegisters::gpr1), static_cast<uint32_t>(AluRegisters::gpr2)); // GPU address of buffer load to ACCU register

    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(taskStream->getSpace(3 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeFence); // to be sure that all writes and reads are completed
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeLoadind); // load dword from memory address located in ACCU to R0
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeFence); // to be sure that all writes and reads are completed
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;

    loadAddressToMiMathAccu<FamilyType>(static_cast<uint32_t>(AluRegisters::gpr3), static_cast<uint32_t>(AluRegisters::gpr4), static_cast<uint32_t>(AluRegisters::gpr2)); // GPU address of bufferB load to ACCU register

    pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(taskStream->getSpace(3 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeFence); // to be sure that all writes and reads are completed
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeStoreind); // store to memory from ACCU, value from register R0
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeFence); // to be sure that all writes and reads are completed
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;

    flushStream();

    expectMemory<FamilyType>(reinterpret_cast<void *>(bufferB->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress()), bufferMemory, sizeof(uint64_t));
}
HWTEST2_F(MiMath, givenValueToMakeLeftLogicalShiftWhenUseMiMathThenShiftIsDoneProperly, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    uint64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = 1u;
    cl_int retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                         CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                         bufferSize, bufferMemory, retVal));
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    csr->makeResident(*buffer->getGraphicsAllocation(rootDeviceIndex));

    uint32_t value = 1u;
    uint32_t shift = 2u;
    uint32_t notPowerOfTwoShift = 5u;
    uint32_t expectedUsedShift = 4u;

    loadValueToRegister<FamilyType>(value, RegisterOffsets::csGprR0);
    loadValueToRegister<FamilyType>(shift, RegisterOffsets::csGprR1);
    loadValueToRegister<FamilyType>(notPowerOfTwoShift, RegisterOffsets::csGprR2);
    auto pCmd = reinterpret_cast<uint32_t *>(taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = 7 - 1;

    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(taskStream->getSpace(7 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value from R0 to SRCA
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeShl); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeShl); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);

    storeValueInRegisterToMemory<FamilyType>(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), RegisterOffsets::csGprR1);
    storeValueInRegisterToMemory<FamilyType>(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress() + 4, RegisterOffsets::csGprR2);
    flushStream();

    uint32_t firstShift = value << shift;
    uint32_t secondShift = value << notPowerOfTwoShift;
    uint32_t executeSecondShift = value << expectedUsedShift;

    expectMemory<FamilyType>(reinterpret_cast<void *>(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress()), &firstShift, sizeof(firstShift));
    expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress() + 4), &secondShift, sizeof(secondShift));
    expectMemory<FamilyType>(reinterpret_cast<void *>(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress() + 4), &executeSecondShift, sizeof(executeSecondShift));
}
HWTEST2_F(MiMath, givenValueToMakeRightLogicalShiftWhenUseMiMathThenShiftIsDoneProperly, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    uint64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = 1u;
    cl_int retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                         CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                         bufferSize, bufferMemory, retVal));
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    csr->makeResident(*allocation);

    uint32_t value = 32u;
    uint32_t shift = 2u;
    uint32_t notPowerOfTwoShift = 5u;
    uint32_t expectedUsedShift = 4u;

    loadValueToRegister<FamilyType>(value, RegisterOffsets::csGprR0);
    loadValueToRegister<FamilyType>(shift, RegisterOffsets::csGprR1);
    loadValueToRegister<FamilyType>(notPowerOfTwoShift, RegisterOffsets::csGprR2);
    auto pCmd = reinterpret_cast<uint32_t *>(taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = 7 - 1;

    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(taskStream->getSpace(7 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value from R0 to SRCA
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr0);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeShr); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr1);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeShr); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr2);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);

    storeValueInRegisterToMemory<FamilyType>(allocation->getGpuAddress(), RegisterOffsets::csGprR1);
    storeValueInRegisterToMemory<FamilyType>(allocation->getGpuAddress() + 4, RegisterOffsets::csGprR2);
    flushStream();

    uint32_t firstShift = value >> shift;
    uint32_t secondShift = value >> notPowerOfTwoShift;
    uint32_t executeSecondShift = value >> expectedUsedShift;

    expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress()), &firstShift, sizeof(firstShift));
    expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress() + 4), &secondShift, sizeof(secondShift));
    expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress() + 4), &executeSecondShift, sizeof(executeSecondShift));
}
HWTEST2_F(MiMath, givenValueToMakeRightAritmeticShiftWhenUseMiMathThenShiftIsDoneProperly, MatcherIsDg2OrPvc) {
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;

    int64_t bufferMemory[bufferSize] = {};
    bufferMemory[0] = -32;
    cl_int retVal = CL_SUCCESS;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                         CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                         bufferSize, bufferMemory, retVal));
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    csr->makeResident(*allocation);

    uint32_t shift = 2u;
    uint32_t notPowerOfTwoShift = 5u;
    uint32_t expectedUsedShift = 4u;

    loadAddressToRegisters<FamilyType>(RegisterOffsets::csGprR0, RegisterOffsets::csGprR1, RegisterOffsets::csGprR2, allocation->getGpuAddress()); // prepare registers to mi_math operation
    loadValueToRegister<FamilyType>(shift, RegisterOffsets::csGprR4);
    loadValueToRegister<FamilyType>(notPowerOfTwoShift, RegisterOffsets::csGprR5);

    auto pCmd = reinterpret_cast<uint32_t *>(taskStream->getSpace(sizeof(MI_MATH)));
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.Value = 0x0;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionType = MI_MATH::COMMAND_TYPE_MI_COMMAND;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.InstructionOpcode = MI_MATH::MI_COMMAND_OPCODE_MI_MATH;
    reinterpret_cast<MI_MATH *>(pCmd)->DW0.BitField.DwordLength = numberOfOperationToLoadAddressToMiMathAccu + 9 - 1;
    loadAddressToMiMathAccu<FamilyType>(static_cast<uint32_t>(AluRegisters::gpr0), static_cast<uint32_t>(AluRegisters::gpr1), static_cast<uint32_t>(AluRegisters::gpr2)); // GPU address of buffer load to ACCU register
    MI_MATH_ALU_INST_INLINE *pAluParam = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(taskStream->getSpace(9 * sizeof(MI_MATH_ALU_INST_INLINE)));
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeLoadind); // load value from R0 to SRCA
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr3);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeFence); // to be sure that all writes and reads are completed
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value from R0 to SRCA
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srca);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr3);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr4);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeSar); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr4);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeLoad); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::srcb);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::gpr5);
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(NewAluOpcodes::opcodeSar); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = 0;
    pAluParam->DW0.BitField.Operand2 = 0;
    pAluParam++;
    pAluParam->DW0.BitField.ALUOpcode = static_cast<uint32_t>(AluRegisters::opcodeStore); // load value to shift to SRCB
    pAluParam->DW0.BitField.Operand1 = static_cast<uint32_t>(AluRegisters::gpr5);
    pAluParam->DW0.BitField.Operand2 = static_cast<uint32_t>(AluRegisters::accu);

    storeValueInRegisterToMemory<FamilyType>(allocation->getGpuAddress(), RegisterOffsets::csGprR4);
    storeValueInRegisterToMemory<FamilyType>(allocation->getGpuAddress() + 4, RegisterOffsets::csGprR5);
    flushStream();

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

    expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress()), &firstShift, sizeof(uint32_t));
    expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress() + 4), &secondShift, sizeof(uint32_t));
    expectMemory<FamilyType>(reinterpret_cast<void *>(allocation->getGpuAddress() + 4), &executeSecondShift, sizeof(uint32_t));
}

template <typename CompareDataT>
struct ConditionalBbStartTests : public MiMath {
    using TestCompareDataT = CompareDataT;

    void SetUp() override {
        MiMath::SetUp();

        std::vector<CompareDataT> bufferMemory;
        bufferMemory.resize(compareBufferSize);

        if constexpr (isQwordData) {
            baseCompareValue = 0x1'0000'0000;
        }

        std::fill(bufferMemory.begin(), bufferMemory.end(), baseCompareValue);

        // bufferMemory[0]; -- Equal. Dont change
        bufferMemory[1] += 5; // Greater
        bufferMemory[2] -= 5; // Less

        cl_int retVal = CL_SUCCESS;

        buffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                        compareBufferSize * sizeof(CompareDataT), bufferMemory.data(), retVal));

        csr->makeResident(*buffer->getGraphicsAllocation(rootDeviceIndex));

        baseGpuVa = ptrOffset(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), buffer->getOffset());
        baseWriteGpuVa = baseGpuVa + (sizeof(CompareDataT) * numCompareModes);
    }

    template <typename AtomicT>
    typename AtomicT::ATOMIC_OPCODES getAtomicOpcode() const {
        return isQwordData ? AtomicT::ATOMIC_OPCODES::ATOMIC_8B_INCREMENT : AtomicT::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT;
    }

    template <typename AtomicT>
    typename AtomicT::DATA_SIZE geDataSize() const {
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
    std::unique_ptr<Buffer> buffer;
    static constexpr bool isQwordData = std::is_same<uint64_t, TestCompareDataT>::value;
};

using ConditionalBbStartTests32b = ConditionalBbStartTests<uint32_t>;
using ConditionalBbStartTests64b = ConditionalBbStartTests<uint64_t>;

template <typename T>
template <typename FamilyType>
void ConditionalBbStartTests<T>::whenDispatchingEqualModeThenResultsAreValidImpl() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    // Equal
    {
        uint64_t jumpAddress = taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, jumpAddress, baseGpuVa, baseCompareValue, NEO::CompareOperation::equal, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*taskStream); // should be skipped

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa,
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    // Greater
    {

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, invalidGpuVa, baseGpuVa + sizeof(TestCompareDataT), baseCompareValue, NEO::CompareOperation::equal, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa + sizeof(TestCompareDataT),
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    // Less
    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, invalidGpuVa, baseGpuVa + (sizeof(TestCompareDataT) * 2), baseCompareValue, NEO::CompareOperation::equal, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa + (sizeof(TestCompareDataT) * 2),
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    flushStream();

    TestCompareDataT expectedValue = baseCompareValue + 1;
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa), &expectedValue, sizeof(TestCompareDataT));
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa + sizeof(TestCompareDataT)), &expectedValue, sizeof(TestCompareDataT));
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa + (sizeof(TestCompareDataT) * 2)), &expectedValue, sizeof(TestCompareDataT));
}

HWTEST2_F(ConditionalBbStartTests32b, whenDispatchingEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    whenDispatchingEqualModeThenResultsAreValidImpl<FamilyType>();
}

HWTEST2_F(ConditionalBbStartTests64b, whenDispatchingEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    whenDispatchingEqualModeThenResultsAreValidImpl<FamilyType>();
}

template <typename T>
template <typename FamilyType>
void ConditionalBbStartTests<T>::whenDispatchingNotEqualModeThenResultsAreValidImpl() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    // Equal
    {

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, invalidGpuVa, baseGpuVa, baseCompareValue, NEO::CompareOperation::notEqual, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa,
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    // Greater
    {

        uint64_t jumpAddress = taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, jumpAddress, baseGpuVa + sizeof(TestCompareDataT), baseCompareValue, NEO::CompareOperation::notEqual, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*taskStream); // should be skipped

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa + sizeof(TestCompareDataT),
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    // Less
    {
        uint64_t jumpAddress = taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, jumpAddress, baseGpuVa + (sizeof(TestCompareDataT) * 2), baseCompareValue, NEO::CompareOperation::notEqual, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*taskStream); // should be skipped

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa + (sizeof(TestCompareDataT) * 2),
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    flushStream();

    TestCompareDataT expectedValue = baseCompareValue + 1;
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa), &expectedValue, sizeof(TestCompareDataT));
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa + sizeof(TestCompareDataT)), &expectedValue, sizeof(TestCompareDataT));
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa + (sizeof(TestCompareDataT) * 2)), &expectedValue, sizeof(TestCompareDataT));
}

HWTEST2_F(ConditionalBbStartTests32b, whenDispatchingNotEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    whenDispatchingNotEqualModeThenResultsAreValidImpl<FamilyType>();
}

HWTEST2_F(ConditionalBbStartTests64b, whenDispatchingNotEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    whenDispatchingNotEqualModeThenResultsAreValidImpl<FamilyType>();
}

template <typename T>
template <typename FamilyType>
void ConditionalBbStartTests<T>::whenDispatchingGreaterOrEqualModeThenResultsAreValidImpl() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    // Equal
    {
        uint64_t jumpAddress = taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, jumpAddress, baseGpuVa, baseCompareValue, NEO::CompareOperation::greaterOrEqual, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*taskStream); // should be skipped

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa,
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    // Greater
    {

        uint64_t jumpAddress = taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, jumpAddress, baseGpuVa + sizeof(TestCompareDataT), baseCompareValue, NEO::CompareOperation::greaterOrEqual, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*taskStream); // should be skipped

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa + sizeof(TestCompareDataT),
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    // Less
    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, invalidGpuVa, baseGpuVa + (sizeof(TestCompareDataT) * 2), baseCompareValue, NEO::CompareOperation::greaterOrEqual, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa + (sizeof(TestCompareDataT) * 2),
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    flushStream();

    TestCompareDataT expectedValue = baseCompareValue + 1;
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa), &expectedValue, sizeof(TestCompareDataT));
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa + sizeof(TestCompareDataT)), &expectedValue, sizeof(TestCompareDataT));
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa + (sizeof(TestCompareDataT) * 2)), &expectedValue, sizeof(TestCompareDataT));
}

HWTEST2_F(ConditionalBbStartTests32b, whenDispatchingGreaterOrEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    whenDispatchingGreaterOrEqualModeThenResultsAreValidImpl<FamilyType>();
}

HWTEST2_F(ConditionalBbStartTests64b, whenDispatchingGreaterOrEqualModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    whenDispatchingGreaterOrEqualModeThenResultsAreValidImpl<FamilyType>();
}

template <typename T>
template <typename FamilyType>
void ConditionalBbStartTests<T>::whenDispatchingLessModeThenResultsAreValidImpl() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;

    // Equal
    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, invalidGpuVa, baseGpuVa, baseCompareValue, NEO::CompareOperation::less, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa,
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    // Greater
    {
        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, invalidGpuVa, baseGpuVa + sizeof(TestCompareDataT), baseCompareValue, NEO::CompareOperation::less, false, isQwordData, false);

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa + sizeof(TestCompareDataT),
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    // Less
    {
        uint64_t jumpAddress = taskStream->getCurrentGpuAddressPosition() + EncodeBatchBufferStartOrEnd<FamilyType>::getCmdSizeConditionalDataMemBatchBufferStart(isQwordData) + EncodeBatchBufferStartOrEnd<FamilyType>::getBatchBufferEndSize();

        EncodeBatchBufferStartOrEnd<FamilyType>::programConditionalDataMemBatchBufferStart(*taskStream, jumpAddress, baseGpuVa + (sizeof(TestCompareDataT) * 2), baseCompareValue, NEO::CompareOperation::less, false, isQwordData, false);

        NEO::EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferEnd(*taskStream); // should be skipped

        EncodeAtomic<FamilyType>::programMiAtomic(*taskStream, baseWriteGpuVa + (sizeof(TestCompareDataT) * 2),
                                                  getAtomicOpcode<MI_ATOMIC>(),
                                                  geDataSize<MI_ATOMIC>(),
                                                  0, 0, 0, 0);
    }

    flushStream();

    TestCompareDataT expectedValue = baseCompareValue + 1;
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa), &expectedValue, sizeof(TestCompareDataT));
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa + sizeof(TestCompareDataT)), &expectedValue, sizeof(TestCompareDataT));
    expectMemory<FamilyType>(reinterpret_cast<void *>(baseWriteGpuVa + (sizeof(TestCompareDataT) * 2)), &expectedValue, sizeof(TestCompareDataT));
}

HWTEST2_F(ConditionalBbStartTests32b, whenDispatchingLessModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    whenDispatchingLessModeThenResultsAreValidImpl<FamilyType>();
}

HWTEST2_F(ConditionalBbStartTests64b, whenDispatchingLessModeThenResultsAreValid, IsAtLeastXeHpcCore) {
    whenDispatchingLessModeThenResultsAreValidImpl<FamilyType>();
}

} // namespace NEO
