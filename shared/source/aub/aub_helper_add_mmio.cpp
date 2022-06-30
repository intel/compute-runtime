/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "third_party/aub_stream/headers/aubstream.h"

#include <sstream>

namespace NEO {

MMIOList AubHelper::getAdditionalMmioList() {
    return splitMMIORegisters(DebugManager.flags.AubDumpAddMmioRegistersList.get(), ';');
}

MMIOList AubHelper::splitMMIORegisters(const std::string &registers, char delimiter) {
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

} // namespace NEO
