/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<Gen11Family>;
template class WddmCommandStreamReceiver<Gen11Family>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<Gen11Family>>;
} // namespace NEO
