/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "StateSaveAreaHeaderWrapper.h"

#include <cstddef>
#include <cstdint>

namespace NEO {

inline constexpr uint8_t stateSaveAreaV3MajorVersion = 3;
inline constexpr uint8_t stateSaveAreaV3Minor1LayoutMinorVersion = 1;

// Fields with matching offsets are read through regHeaderV3 for both layouts
static_assert(sizeof(SIP::intelgt_state_save_area_V3) == sizeof(SIP::intelgt_state_save_area_V3_1));
static_assert(offsetof(SIP::intelgt_state_save_area_V3, num_subslices_per_slice) == offsetof(SIP::intelgt_state_save_area_V3_1, num_subslices_per_slice));
static_assert(offsetof(SIP::intelgt_state_save_area_V3, num_eus_per_subslice) == offsetof(SIP::intelgt_state_save_area_V3_1, num_eus_per_subslice));
static_assert(offsetof(SIP::intelgt_state_save_area_V3, num_threads_per_eu) == offsetof(SIP::intelgt_state_save_area_V3_1, num_threads_per_eu));
static_assert(offsetof(SIP::intelgt_state_save_area_V3, state_area_offset) == offsetof(SIP::intelgt_state_save_area_V3_1, state_area_offset));
static_assert(offsetof(SIP::intelgt_state_save_area_V3, state_save_size) == offsetof(SIP::intelgt_state_save_area_V3_1, state_save_size));
static_assert(offsetof(SIP::intelgt_state_save_area_V3, grf) == offsetof(SIP::intelgt_state_save_area_V3_1, grf));
static_assert(offsetof(SIP::intelgt_state_save_area_V3, msg) == offsetof(SIP::intelgt_state_save_area_V3_1, msg));

struct StateSaveAreaV3FifoIndexOffsets {
    size_t fifoSize;
    size_t fifoHead;
    size_t fifoTail;
};

inline bool isStateSaveAreaV3Minor1Layout(const StateSaveAreaHeader &header) {
    return header.versionHeader.version.major == stateSaveAreaV3MajorVersion &&
           header.versionHeader.version.minor >= stateSaveAreaV3Minor1LayoutMinorVersion;
}

template <typename RegHeaderFieldReader>
inline auto readStateSaveAreaV3Field(const StateSaveAreaHeader &header, RegHeaderFieldReader &&readField) {
    return isStateSaveAreaV3Minor1Layout(header) ? readField(header.regHeaderV3Minor1) : readField(header.regHeaderV3);
}

inline uint32_t getStateSaveAreaV3SrMagicOffset(const StateSaveAreaHeader &header) {
    return readStateSaveAreaV3Field(header, [](const auto &regHeader) { return regHeader.sr_magic_offset; });
}

inline uint32_t getStateSaveAreaV3SipFlags(const StateSaveAreaHeader &header) {
    return readStateSaveAreaV3Field(header, [](const auto &regHeader) { return regHeader.sip_flags; });
}

inline uint32_t getStateSaveAreaV3FifoOffset(const StateSaveAreaHeader &header) {
    return readStateSaveAreaV3Field(header, [](const auto &regHeader) { return regHeader.fifo_offset; });
}

inline uint32_t getStateSaveAreaV3FifoSize(const StateSaveAreaHeader &header) {
    return readStateSaveAreaV3Field(header, [](const auto &regHeader) { return regHeader.fifo_size; });
}

template <typename RegHeaderT>
constexpr StateSaveAreaV3FifoIndexOffsets getFifoIndexOffsetsInStateSaveArea() {
    return {sizeof(SIP::StateSaveArea) + offsetof(RegHeaderT, fifo_size),
            sizeof(SIP::StateSaveArea) + offsetof(RegHeaderT, fifo_head),
            sizeof(SIP::StateSaveArea) + offsetof(RegHeaderT, fifo_tail)};
}

inline StateSaveAreaV3FifoIndexOffsets getStateSaveAreaV3FifoIndexOffsets(const StateSaveAreaHeader &header) {
    return isStateSaveAreaV3Minor1Layout(header) ? getFifoIndexOffsetsInStateSaveArea<SIP::intelgt_state_save_area_V3_1>()
                                                 : getFifoIndexOffsetsInStateSaveArea<SIP::intelgt_state_save_area_V3>();
}

} // namespace NEO
