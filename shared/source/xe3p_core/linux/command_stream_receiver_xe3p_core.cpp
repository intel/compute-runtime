/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"

#include "hw_cmds_xe3p_core.h"

namespace NEO {

template class DeviceCommandStreamReceiver<Xe3pCoreFamily>;
template class DrmCommandStreamReceiver<Xe3pCoreFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<Xe3pCoreFamily>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<Xe3pCoreFamily>>>);
} // namespace NEO
