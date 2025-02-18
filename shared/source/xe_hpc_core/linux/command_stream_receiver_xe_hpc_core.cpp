/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {

template class DeviceCommandStreamReceiver<XeHpcCoreFamily>;
template class DrmCommandStreamReceiver<XeHpcCoreFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<XeHpcCoreFamily>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<XeHpcCoreFamily>>>);
} // namespace NEO
