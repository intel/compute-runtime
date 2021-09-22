/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
uint32_t TbxCommandStreamReceiverHw<Family>::getMaskAndValueForPollForCompletion() const {
    return 0x80;
}

template <>
bool TbxCommandStreamReceiverHw<Family>::getpollNotEqualValueForPollForCompletion() const {
    return true;
}
