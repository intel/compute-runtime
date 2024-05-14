/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "aubstream/aubstream.h"

namespace NEO {

void AubHelper::setTbxConfiguration() {
    aub_stream::setTbxServerIpLegacy(debugManager.flags.TbxServer.get());
    aub_stream::setTbxServerPort(debugManager.flags.TbxPort.get());
    aub_stream::setTbxFrontdoorMode(debugManager.flags.TbxFrontdoorMode.get());
}
} // namespace NEO
