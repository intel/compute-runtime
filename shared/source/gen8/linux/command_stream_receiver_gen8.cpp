/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream_bdw_and_later.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<Gen8Family>;
template class DrmCommandStreamReceiver<Gen8Family>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<Gen8Family>>;
} // namespace NEO
