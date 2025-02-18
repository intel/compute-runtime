/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"
#include "shared/source/xe2_hpg_core/hw_cmds.h"

namespace NEO {

template class DeviceCommandStreamReceiver<Xe2HpgCoreFamily>;
template class DrmCommandStreamReceiver<Xe2HpgCoreFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<Xe2HpgCoreFamily>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<Xe2HpgCoreFamily>>>);
} // namespace NEO
