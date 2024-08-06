/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
enum class CommandStreamReceiverType {
    // Use receiver for real HW
    hardware = 0,
    // Capture an AUB file automatically for all traffic going through Device -> CommandStreamReceiver
    aub,
    // Capture an AUB and tunnel all commands going through Device -> CommandStreamReceiver to a TBX server
    tbx,
    // Use receiver for real HW and capture AUB file
    hardwareWithAub,
    // Use TBX server and capture AUB file
    tbxWithAub,
    // all traffic goes through AUB path but aubstream creates no file
    nullAub,
    // Number of CSR types
    typesNum
};

inline CommandStreamReceiverType obtainCsrTypeFromIntegerValue(int32_t selectedCsrType, CommandStreamReceiverType defaultType) {
    if (selectedCsrType >= 0 && selectedCsrType <= static_cast<int32_t>(CommandStreamReceiverType::typesNum)) {
        return static_cast<CommandStreamReceiverType>(selectedCsrType);
    }
    return defaultType;
}
// AUB file folder location
extern const char *folderAUB;

// Initial value for HW tag
// Set to 0 if using HW or simulator, otherwise 0xFFFFFF00, needs to be lower then CompletionStamp::notReady.
extern uint32_t initialHardwareTag;
} // namespace NEO
