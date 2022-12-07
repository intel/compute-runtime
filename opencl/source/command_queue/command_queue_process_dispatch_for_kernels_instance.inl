/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
template void CommandQueueHw<Family>::processDispatchForKernels<CL_COMMAND_NDRANGE_KERNEL>(const MultiDispatchInfo &multiDispatchInfo,
                                                                                           std::unique_ptr<PrintfHandler> &printfHandler,
                                                                                           Event *event,
                                                                                           TagNodeBase *&hwTimeStamps,
                                                                                           bool blockQueue,
                                                                                           CsrDependencies &csrDeps,
                                                                                           KernelOperation *blockedCommandsData,
                                                                                           TimestampPacketDependencies &timestampPacketDependencies);