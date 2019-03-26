/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "runtime/os_interface/linux/device_command_stream.inl"
#include "runtime/os_interface/linux/drm_command_stream.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<CNLFamily>;
template class DrmCommandStreamReceiver<CNLFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<CNLFamily>>;
} // namespace NEO
