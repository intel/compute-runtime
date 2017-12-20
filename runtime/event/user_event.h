/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "event.h"

namespace OCLRT {
class CommandQueue;
class Context;

class UserEvent : public Event {
  public:
    UserEvent(Context *ctx = nullptr);

    ~UserEvent() override = default;

    bool wait(bool blocking) override;

    void updateExecutionStatus() override;

    uint32_t getTaskLevel() override;

    bool isInitialEventStatus() const;
};

class VirtualEvent : public Event {
  public:
    VirtualEvent(CommandQueue *cmdQ = nullptr, Context *ctx = nullptr);

    ~VirtualEvent() override = default;

    bool wait(bool blocking) override;

    bool setStatus(cl_int status) override;

    void updateExecutionStatus() override;

    uint32_t getTaskLevel() override;
};
} // namespace OCLRT
