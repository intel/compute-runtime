/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

namespace NEO {

template class DeviceCommandStreamReceiver<Xe2HpgCoreFamily>;
template class WddmCommandStreamReceiver<Xe2HpgCoreFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<Xe2HpgCoreFamily>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<Xe2HpgCoreFamily>>>);
} // namespace NEO
