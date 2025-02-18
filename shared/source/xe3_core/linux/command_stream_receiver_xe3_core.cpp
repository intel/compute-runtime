/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {

template class DeviceCommandStreamReceiver<Xe3CoreFamily>;
template class DrmCommandStreamReceiver<Xe3CoreFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<Xe3CoreFamily>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<Xe3CoreFamily>>>);
} // namespace NEO
