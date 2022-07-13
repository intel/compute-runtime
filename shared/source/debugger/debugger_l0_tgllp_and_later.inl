/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <typename GfxFamily>
size_t DebuggerL0Hw<GfxFamily>::getSbaTrackingCommandsSize(size_t trackedAddressCount) {
    if (singleAddressSpaceSbaTracking) {
        if (NEO::DebugManager.flags.DebuggerDisableSingleAddressSbaTracking.get()) {
            return 0;
        } else {
            constexpr uint32_t aluCmdSize = sizeof(typename GfxFamily::MI_MATH) + sizeof(typename GfxFamily::MI_MATH_ALU_INST_INLINE) * NUM_ALU_INST_FOR_READ_MODIFY_WRITE;
            return 2 * (sizeof(typename GfxFamily::MI_ARB_CHECK) + sizeof(typename GfxFamily::MI_BATCH_BUFFER_START)) +
                   trackedAddressCount * (sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM) + aluCmdSize + 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM) +
                                          3 * sizeof(typename GfxFamily::MI_STORE_DATA_IMM) +
                                          sizeof(typename GfxFamily::MI_ARB_CHECK) +
                                          sizeof(typename GfxFamily::MI_BATCH_BUFFER_START));
        }
    }
    return trackedAddressCount * NEO::EncodeStoreMemory<GfxFamily>::getStoreDataImmSize();
}

template <typename GfxFamily>
void DebuggerL0Hw<GfxFamily>::programSbaTrackingCommandsSingleAddressSpace(NEO::LinearStream &cmdStream, const SbaAddresses &sba) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;
    using MI_NOOP = typename GfxFamily::MI_NOOP;

    const auto offsetToAddress = offsetof(MI_STORE_DATA_IMM, TheStructure.RawData[1]);
    const auto offsetToData = offsetof(MI_STORE_DATA_IMM, TheStructure.Common.DataDword0);

    UNRECOVERABLE_IF(!singleAddressSpaceSbaTracking);

    if (NEO::DebugManager.flags.DebuggerDisableSingleAddressSbaTracking.get()) {
        return;
    }

    std::vector<std::pair<size_t, uint64_t>> fieldOffsetAndValue;

    if (sba.GeneralStateBaseAddress) {
        fieldOffsetAndValue.push_back({offsetof(SbaTrackedAddresses, GeneralStateBaseAddress), sba.GeneralStateBaseAddress});
    }
    if (sba.SurfaceStateBaseAddress) {
        fieldOffsetAndValue.push_back({offsetof(SbaTrackedAddresses, SurfaceStateBaseAddress), sba.SurfaceStateBaseAddress});
    }
    if (sba.DynamicStateBaseAddress) {
        fieldOffsetAndValue.push_back({offsetof(SbaTrackedAddresses, DynamicStateBaseAddress), sba.DynamicStateBaseAddress});
    }
    if (sba.IndirectObjectBaseAddress) {
        fieldOffsetAndValue.push_back({offsetof(SbaTrackedAddresses, IndirectObjectBaseAddress), sba.IndirectObjectBaseAddress});
    }
    if (sba.InstructionBaseAddress) {
        fieldOffsetAndValue.push_back({offsetof(SbaTrackedAddresses, InstructionBaseAddress), sba.InstructionBaseAddress});
    }
    if (sba.BindlessSurfaceStateBaseAddress) {
        fieldOffsetAndValue.push_back({offsetof(SbaTrackedAddresses, BindlessSurfaceStateBaseAddress), sba.BindlessSurfaceStateBaseAddress});
    }
    const auto cmdStreamGpuBase = cmdStream.getGpuBase();
    const auto cmdStreamCpuBase = reinterpret_cast<uint64_t>(cmdStream.getCpuBase());

    if (fieldOffsetAndValue.size()) {
        auto arb = cmdStream.getSpaceForCmd<MI_ARB_CHECK>();
        auto arbCmd = GfxFamily::cmdInitArbCheck;
        arbCmd.setPreParserDisable(true);
        *arb = arbCmd;

        // Jump to SDI command that is modified
        auto newBuffer = cmdStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
        const auto nextCommand = ptrOffset(cmdStreamGpuBase, ptrDiff(cmdStream.getSpace(0), cmdStreamCpuBase));

        MI_BATCH_BUFFER_START bbCmd = GfxFamily::cmdInitBatchBufferStart;
        bbCmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        bbCmd.setBatchBufferStartAddress(nextCommand);
        bbCmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
        *newBuffer = bbCmd;
    }

    for (const auto &pair : fieldOffsetAndValue) {

        // Store SBA field offset to R0
        NEO::EncodeSetMMIO<GfxFamily>::encodeIMM(cmdStream, CS_GPR_R0, static_cast<uint32_t>(pair.first), true);
        // Add GPR0 to GPR15, store result in GPR1
        NEO::EncodeMath<GfxFamily>::addition(cmdStream, AluRegisters::R_0, AluRegisters::R_15, AluRegisters::R_1);

        // Cmds to store dest address - from GPR
        auto miStoreRegMemLow = cmdStream.getSpaceForCmd<MI_STORE_REGISTER_MEM>();
        auto miStoreRegMemHigh = cmdStream.getSpaceForCmd<MI_STORE_REGISTER_MEM>();

        // Cmd to store value ( SBA address )
        auto miStoreDataSettingSbaBufferAddress = cmdStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
        auto miStoreDataSettingSbaBufferAddress2 = cmdStream.getSpaceForCmd<MI_STORE_DATA_IMM>();

        auto arb = cmdStream.getSpaceForCmd<MI_ARB_CHECK>();
        auto arbCmd = GfxFamily::cmdInitArbCheck;
        arbCmd.setPreParserDisable(true);
        *arb = arbCmd;

        // Jump to SDI command that is modified
        auto newBuffer = cmdStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
        const auto addressOfSDI = ptrOffset(cmdStreamGpuBase, ptrDiff(cmdStream.getSpace(0), cmdStreamCpuBase));

        // Cmd to store value ( SBA address )
        auto miStoreSbaField = cmdStream.getSpaceForCmd<MI_STORE_DATA_IMM>();

        auto gpuVaOfAddress = addressOfSDI + offsetToAddress;
        auto gpuVaOfData = addressOfSDI + offsetToData;
        const auto gpuVaOfDataDWORD1 = gpuVaOfData + 4;

        NEO::EncodeStoreMMIO<GfxFamily>::encode(miStoreRegMemLow, CS_GPR_R1, gpuVaOfAddress, false);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(miStoreRegMemHigh, CS_GPR_R1 + 4, gpuVaOfAddress + 4, false);

        MI_STORE_DATA_IMM setSbaBufferAddress = GfxFamily::cmdInitStoreDataImm;
        setSbaBufferAddress.setAddress(gpuVaOfData);
        setSbaBufferAddress.setStoreQword(false);
        setSbaBufferAddress.setDataDword0(pair.second & 0xffffffff);
        setSbaBufferAddress.setDataDword1(0);
        setSbaBufferAddress.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD);
        *miStoreDataSettingSbaBufferAddress = setSbaBufferAddress;

        setSbaBufferAddress.setAddress(gpuVaOfDataDWORD1);
        setSbaBufferAddress.setStoreQword(false);
        setSbaBufferAddress.setDataDword0((pair.second >> 32) & 0xffffffff);
        setSbaBufferAddress.setDataDword1(0);
        setSbaBufferAddress.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD);
        *miStoreDataSettingSbaBufferAddress2 = setSbaBufferAddress;

        MI_BATCH_BUFFER_START bbCmd = GfxFamily::cmdInitBatchBufferStart;
        bbCmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        bbCmd.setBatchBufferStartAddress(addressOfSDI);
        bbCmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
        *newBuffer = bbCmd;

        auto storeSbaField = GfxFamily::cmdInitStoreDataImm;
        storeSbaField.setStoreQword(true);

        storeSbaField.setAddress(0x0);
        storeSbaField.setDataDword0(0xdeadbeef);
        storeSbaField.setDataDword1(0xbaadfeed);
        storeSbaField.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH_STORE_QWORD);
        *miStoreSbaField = storeSbaField;
    }

    if (fieldOffsetAndValue.size()) {

        auto previousBuffer = cmdStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
        const auto addressOfPreviousBuffer = ptrOffset(cmdStreamGpuBase, ptrDiff(cmdStream.getSpace(0), cmdStreamCpuBase));

        MI_BATCH_BUFFER_START bbCmd = GfxFamily::cmdInitBatchBufferStart;
        bbCmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        bbCmd.setBatchBufferStartAddress(addressOfPreviousBuffer);
        bbCmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
        *previousBuffer = bbCmd;

        auto arbCmd = GfxFamily::cmdInitArbCheck;
        auto arb = cmdStream.getSpaceForCmd<MI_ARB_CHECK>();
        arbCmd.setPreParserDisable(false);
        *arb = arbCmd;
    }
}

} // namespace NEO
