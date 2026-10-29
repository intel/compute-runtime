/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "aubstream/aubstream.h"

namespace NEO {

void AubHelper::setTbxConfiguration(uint32_t rootDeviceIndex) {
    aub_stream::setTbxServerIpLegacy(debugManager.flags.TbxServer.get());
    aub_stream::setTbxServerPort(getTbxPort(rootDeviceIndex));
    aub_stream::setTbxFrontdoorMode(debugManager.flags.TbxFrontdoorMode.get());
}
} // namespace NEO
