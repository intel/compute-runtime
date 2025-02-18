/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<Gen12LpFamily>;
template class WddmCommandStreamReceiver<Gen12LpFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<Gen12LpFamily>>;
static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<Gen12LpFamily>>>);
} // namespace NEO
