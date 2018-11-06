/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "aub_mapper.h"

namespace OCLRT {

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initGlobalMMIO() {
    for (auto &mmioPair : AUBFamilyMapper<GfxFamily>::globalMMIO) {
        stream->writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
MMIOList CommandStreamReceiverSimulatedCommonHw<GfxFamily>::splitMMIORegisters(const std::string &registers, char delimiter) {
    MMIOList result;
    bool firstElementInPair = false;
    std::string token;
    uint32_t registerOffset = 0;
    uint32_t registerValue = 0;
    std::istringstream stream("");

    for (std::string::const_iterator i = registers.begin();; i++) {
        if (i == registers.end() || *i == delimiter) {
            if (token.size() > 0) {
                stream.str(token);
                stream.clear();
                firstElementInPair = !firstElementInPair;
                stream >> std::hex >> (firstElementInPair ? registerOffset : registerValue);
                if (stream.fail()) {
                    result.clear();
                    break;
                }
                token.clear();
                if (!firstElementInPair) {
                    result.push_back(std::pair<uint32_t, uint32_t>(registerOffset, registerValue));
                    registerValue = 0;
                    registerOffset = 0;
                }
            }
            if (i == registers.end()) {
                break;
            }
        } else {
            token.push_back(*i);
        }
    }
    return result;
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO() {
    if (DebugManager.flags.AubDumpAddMmioRegistersList.get() != "unk") {
        auto mmioList = splitMMIORegisters(DebugManager.flags.AubDumpAddMmioRegistersList.get(), ';');
        for (auto &mmioPair : mmioList) {
            stream->writeMMIO(mmioPair.first, mmioPair.second);
        }
    }
}

template <typename GfxFamily>
PhysicalAddressAllocator *CommandStreamReceiverSimulatedCommonHw<GfxFamily>::createPhysicalAddressAllocator() {
    return new PhysicalAddressAllocator();
}

} // namespace OCLRT
