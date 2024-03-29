/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<Gen11Family>;
template class DrmCommandStreamReceiver<Gen11Family>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<Gen11Family>>;
} // namespace NEO
