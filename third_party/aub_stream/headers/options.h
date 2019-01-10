/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>
#include <cstdint>
#include <ostream>
#include <vector>

namespace AubDump {

extern std::ostream &log;
extern std::string tbxServerIp;
extern uint16_t tbxServerPort;

using MMIOPair = std::pair<uint32_t, uint32_t>;
using MMIOList = std::vector<MMIOPair>;
extern MMIOList injectMMIOList;

} // namespace AubDump
