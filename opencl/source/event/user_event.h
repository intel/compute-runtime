/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "event.h"

namespace NEO {
class CommandQueue;
class Context;

class UserEvent : public Event {
  public:
    UserEvent(Context *ctx = nullptr);

    ~UserEvent() override = default;

    bool wait(bool blocking, bool useQuickKmdSleep) override;

    void updateExecutionStatus() override;

    uint32_t getTaskLevel() override;

    bool isInitialEventStatus() const;
};

class VirtualEvent : public Event {
  public:
    VirtualEvent(CommandQueue *cmdQ = nullptr, Context *ctx = nullptr);

    ~VirtualEvent() override = default;

    bool wait(bool blocking, bool useQuickKmdSleep) override;

    bool setStatus(cl_int status) override;

    void updateExecutionStatus() override;

    uint32_t getTaskLevel() override;
};
} // namespace NEO
