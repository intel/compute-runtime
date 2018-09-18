/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/device_command_stream.inl"
#include "runtime/os_interface/linux/drm_command_stream.inl"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.inl"

namespace OCLRT {

template class DeviceCommandStreamReceiver<SKLFamily>;
template class DrmCommandStreamReceiver<SKLFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<SKLFamily>>;
} // namespace OCLRT
