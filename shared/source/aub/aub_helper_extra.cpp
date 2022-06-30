/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "third_party/aub_stream/headers/aubstream.h"

namespace NEO {

void AubHelper::setTbxConfiguration() {
    aub_stream::setTbxServerIp(DebugManager.flags.TbxServer.get());
    aub_stream::setTbxServerPort(DebugManager.flags.TbxPort.get());
    aub_stream::setTbxFrontdoorMode(DebugManager.flags.TbxFrontdoorMode.get());
}
} // namespace NEO
