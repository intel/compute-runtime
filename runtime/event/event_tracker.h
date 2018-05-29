/*
 * Copyright (c) 2018, Intel Corporation
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
#include <unordered_map>
#include <set>

namespace OCLRT {

struct TrackedEvent : IFNode<TrackedEvent> {
    TrackedEvent(Event *ev, int64_t eventId)
        : ev(ev), eventId(eventId) {
    }
    Event *ev = nullptr;
    int64_t eventId = 1;
};

class EventsTracker {

    using EventIdMap = std::unordered_map<Event *, int64_t>;
    using CmdqSet = std::set<CommandQueue *>;

  protected:
    std::atomic<int64_t> eventId{0};
    static std::unique_ptr<EventsTracker> globalEvTracker;
    IFList<TrackedEvent, true, true> trackedEvents;
    EventsTracker() = default;

  public:
    MOCKABLE_VIRTUAL ~EventsTracker() = default;
    IFList<TrackedEvent, true, true> *getList();
    MOCKABLE_VIRTUAL TrackedEvent *getNodes();
    void dump();
    void notifyCreation(Event *eventToTrack);
    void notifyDestruction(Event *eventToDestroy);
    void notifyTransitionedExecutionStatus();
    MOCKABLE_VIRTUAL std::shared_ptr<std::ostream> createDumpStream(const std::string &filename);
    static EventsTracker &getEventsTracker();
    static void shutdownGlobalEvTracker();
    static std::string label(Event *node, const EventIdMap &eventsIdMapping);
    static std::string label(CommandQueue *cmdQ);
    static void dumpQueue(CommandQueue *cmdQ, std::ostream &out, CmdqSet &dumpedCmdQs);
    static void dumpEdge(Event *leftNode, Event *rightNode, std::ostream &out, const EventIdMap &eventsIdMapping);
    static void dumpNode(Event *node, std::ostream &out, const EventIdMap &eventsIdMapping);
    static void dumpGraph(Event *node, std::ostream &out, CmdqSet &dumpedCmdQs, std::set<Event *> &dumpedEvents,
                          const EventIdMap &eventsIdMapping);
};

} // namespace OCLRT
