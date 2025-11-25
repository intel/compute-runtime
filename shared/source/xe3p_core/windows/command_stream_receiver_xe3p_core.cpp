/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"
#include "shared/source/xe3p_core/hw_cmds_base.h"

namespace NEO {

template class DeviceCommandStreamReceiver<Xe3pCoreFamily>;
template class WddmCommandStreamReceiver<Xe3pCoreFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<Xe3pCoreFamily>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<Xe3pCoreFamily>>>);
} // namespace NEO
