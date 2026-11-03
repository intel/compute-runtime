/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void BlitCommandsHelper<Family>::encodeProfilingStartMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode) {
    auto timestampContextStartGpuAddress = TimestampPacketHelper::getContextStartGpuAddress(timestampPacketNode);
    auto timestampGlobalStartAddress = TimestampPacketHelper::getGlobalStartGpuAddress(timestampPacketNode);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, timestampContextStartGpuAddress + sizeof(uint32_t), false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampUn, timestampGlobalStartAddress + sizeof(uint32_t), false, nullptr, true);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextStartGpuAddress, false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampLdw, timestampGlobalStartAddress, false, nullptr, true);
}

template <>
void BlitCommandsHelper<Family>::encodeProfilingEndMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode) {
    auto timestampContextEndGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(timestampPacketNode);
    auto timestampGlobalEndAddress = TimestampPacketHelper::getGlobalEndGpuAddress(timestampPacketNode);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, timestampContextEndGpuAddress + sizeof(uint32_t), false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampUn, timestampGlobalEndAddress + sizeof(uint32_t), false, nullptr, true);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextEndGpuAddress, false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampLdw, timestampGlobalEndAddress, false, nullptr, true);
}
