/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<Gen9Family>;
template class WddmCommandStreamReceiver<Gen9Family>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<Gen9Family>>;
} // namespace NEO
