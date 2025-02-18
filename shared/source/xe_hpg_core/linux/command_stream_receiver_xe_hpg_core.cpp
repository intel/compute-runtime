/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {

template class DeviceCommandStreamReceiver<XeHpgCoreFamily>;
template class DrmCommandStreamReceiver<XeHpgCoreFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<XeHpgCoreFamily>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<XeHpgCoreFamily>>>);
} // namespace NEO
