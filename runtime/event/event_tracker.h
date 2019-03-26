/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/event/event.h"
#include "runtime/utilities/iflist.h"

#include <set>
#include <unordered_map>

namespace NEO {

class CommandQueue;

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
    void dump();
    void notifyCreation(Event *eventToTrack);
    void notifyDestruction(Event *eventToDestroy);
    void notifyTransitionedExecutionStatus();

    MOCKABLE_VIRTUAL ~EventsTracker() = default;
    MOCKABLE_VIRTUAL TrackedEvent *getNodes();
    MOCKABLE_VIRTUAL std::unique_ptr<std::ostream> createDumpStream(const std::string &filename);

    static EventsTracker &getEventsTracker();
    static std::string label(Event *node, const EventIdMap &eventsIdMapping);
    static std::string label(CommandQueue *cmdQ);
    static void dumpQueue(CommandQueue *cmdQ, std::ostream &out, CmdqSet &dumpedCmdQs);
    static void dumpEdge(Event *leftNode, Event *rightNode, std::ostream &out, const EventIdMap &eventsIdMapping);
    static void dumpNode(Event *node, std::ostream &out, const EventIdMap &eventsIdMapping);
    static void dumpGraph(Event *node, std::ostream &out, CmdqSet &dumpedCmdQs, std::set<Event *> &dumpedEvents,
                          const EventIdMap &eventsIdMapping);
};

} // namespace NEO
