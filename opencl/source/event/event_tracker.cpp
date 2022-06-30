/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/event/event_tracker.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/cl_helper.h"

namespace NEO {

std::unique_ptr<EventsTracker> EventsTracker::globalEvTracker = nullptr;

EventsTracker &EventsTracker::getEventsTracker() {
    static std::mutex initMutex;
    std::lock_guard<std::mutex> autolock(initMutex);

    if (!EventsTracker::globalEvTracker) {
        EventsTracker::globalEvTracker = std::unique_ptr<EventsTracker>{new EventsTracker()};
    }
    UNRECOVERABLE_IF(EventsTracker::globalEvTracker == nullptr);
    return *EventsTracker::globalEvTracker;
}

std::string EventsTracker::label(Event *node, const EventIdMap &eventsIdMapping) {
    std::string retLabel("e");

    auto eventTag = eventsIdMapping.find(node);
    if (eventTag != eventsIdMapping.end()) {
        auto id = eventTag->second;
        retLabel += std::to_string(id);
    }
    return retLabel;
}

std::string EventsTracker::label(CommandQueue *cmdQ) {
    return "cq" + std::to_string(reinterpret_cast<uintptr_t>(cmdQ));
}

void EventsTracker::dumpQueue(CommandQueue *cmdQ, std::ostream &out, CmdqSet &dumpedCmdQs) {
    if ((cmdQ == nullptr) || (dumpedCmdQs.find(cmdQ) != dumpedCmdQs.end())) {
        return;
    }

    out << label(cmdQ) << "[label=\"{------CmdQueue, ptr=" << cmdQ << "------|task count=";
    auto taskCount = cmdQ->taskCount;
    auto taskLevel = cmdQ->taskLevel;
    if (taskCount == CompletionStamp::notReady) {
        out << "NOT_READY";
    } else {
        out << taskCount;
    }

    out << ", level=";
    if (taskLevel == CompletionStamp::notReady) {
        out << "NOT_READY";
    } else {
        out << taskLevel;
    }
    out << "}\",color=blue];\n";
    dumpedCmdQs.insert(cmdQ);
}

void EventsTracker::dumpNode(Event *node, std::ostream &out, const EventIdMap &eventsIdMapping) {
    if (node == nullptr) {
        out << "eNULL[label=\"{ptr=nullptr}\",color=red];\n";
        return;
    }

    bool isUserEvent = node->isUserEvent();

    uint32_t statusId = static_cast<uint32_t>(node->peekExecutionStatus());
    // clamp to aborted
    statusId = (statusId > CL_QUEUED) ? (CL_QUEUED + 1) : statusId;

    const char *color = ((statusId == CL_COMPLETE) || (statusId > CL_QUEUED)) ? "green" : (((statusId == CL_SUBMITTED) && (isUserEvent == false)) ? "yellow" : "red");

    std::string eventType = isUserEvent ? "USER_EVENT" : (node->isCurrentCmdQVirtualEvent() ? "---V_EVENT " : "-----EVENT ");
    std::string commandType = "";
    if (isUserEvent == false) {
        commandType = NEO::cmdTypetoString(node->getCommandType());
    }

    static const char *status[] = {
        "CL_COMPLETE",
        "CL_RUNNING",
        "CL_SUBMITTED",
        "CL_QUEUED",
        "ABORTED"};

    auto taskCount = node->peekTaskCount();
    auto taskLevel = node->taskLevel.load();

    out << label(node, eventsIdMapping) << "[label=\"{------" << eventType << " ptr=" << node << "------"
                                                                                                 "|"
        << commandType << "|" << status[statusId] << "|"
                                                     "task count=";
    if (taskCount == CompletionStamp::notReady) {
        out << "NOT_READY";
    } else {
        out << taskCount;
    }

    out << ", level=";
    if (taskLevel == CompletionStamp::notReady) {
        out << "NOT_READY";
    } else {
        out << taskLevel;
    }

    out << "|CALLBACKS=" << (node->peekHasCallbacks() ? "TRUE" : "FALSE") << "}\",color=" << color << "];\n";

    if (node->isCurrentCmdQVirtualEvent()) {
        out << label(node->getCommandQueue()) << "->" << label(node, eventsIdMapping);
        out << "[label=\"VIRTUAL_EVENT\"]";
        out << ";\n";
    }
}

void EventsTracker::dumpEdge(Event *leftNode, Event *rightNode, std::ostream &out, const EventIdMap &eventsIdMapping) {
    out << label(leftNode, eventsIdMapping) << "->" << label(rightNode, eventsIdMapping) << ";\n";
}

// walk in DFS manner
void EventsTracker::dumpGraph(Event *node, std::ostream &out, CmdqSet &dumpedCmdQs, std::set<Event *> &dumpedEvents,
                              const EventIdMap &eventsIdMapping) {
    if ((node == nullptr) || (dumpedEvents.find(node) != dumpedEvents.end())) {
        return;
    }

    dumpedEvents.insert(node);

    if (node->getCommandQueue() != nullptr) {
        dumpQueue(node->getCommandQueue(), out, dumpedCmdQs);
    }
    dumpNode(node, out, eventsIdMapping);

    auto *childNode = node->peekChildEvents();
    while (childNode != nullptr) {
        dumpGraph(childNode->ref, out, dumpedCmdQs, dumpedEvents, eventsIdMapping);
        dumpEdge(node, childNode->ref, out, eventsIdMapping);
        childNode = childNode->next;
    }
}

TrackedEvent *EventsTracker::getNodes() {
    return trackedEvents.detachNodes();
}

void EventsTracker::dump() {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    auto time = std::chrono::system_clock::now();
    std::string dumpFileName = "eg_"
                               "reg" +
                               std::to_string(reinterpret_cast<uintptr_t>(this)) + "_" + std::to_string(time.time_since_epoch().count()) + ".gv";
    auto out = createDumpStream(dumpFileName);

    *out << "digraph events_registry_" << this << " {\n";
    *out << "node [shape=record]\n";
    *out << "//pragma: somePragmaData"
         << "\n";
    auto allNodes = getNodes();
    EventIdMap deadNodeTags;
    auto curr = allNodes;
    TrackedEvent *prev = nullptr;
    EventIdMap eventsIdMapping;

    while (curr != nullptr) {
        auto next = curr->next;
        bool eraseNode = false;
        if (curr->eventId < 0) {
            auto prevTag = deadNodeTags.find(curr->ev);
            if (prevTag == deadNodeTags.end()) {
                deadNodeTags[curr->ev] = -curr->eventId;
            }
            eraseNode = true;
        } else if ((deadNodeTags.find(curr->ev) != deadNodeTags.end()) && (deadNodeTags[curr->ev] > curr->eventId)) {
            eraseNode = true;
        }

        if (eraseNode) {
            if (prev != nullptr) {
                prev->next = next;
            }
            if (allNodes == curr) {
                allNodes = nullptr;
            }
            delete curr;
        } else {
            if (allNodes == nullptr) {
                allNodes = curr;
            }
            prev = curr;
            eventsIdMapping[curr->ev] = curr->eventId;
        }
        curr = next;
    }

    auto node = allNodes;
    CmdqSet dumpedCmdQs;
    std::set<Event *> dumpedEvents;
    while (node != nullptr) {
        if (node->ev->peekNumEventsBlockingThis() != 0) {
            node = node->next;
            continue;
        }
        dumpGraph(node->ev, *out, dumpedCmdQs, dumpedEvents, eventsIdMapping);
        node = node->next;
    }
    *out << "\n}\n";

    if (allNodes == nullptr) {
        return;
    }

    if (trackedEvents.peekHead() != nullptr) {
        trackedEvents.peekHead()->getTail()->insertAllNext(*allNodes);
    } else {
        auto rest = allNodes->next;
        trackedEvents.pushFrontOne(*allNodes);
        if (rest != nullptr) {
            allNodes->insertAllNext(*rest);
        }
    }
}

void EventsTracker::notifyCreation(Event *eventToTrack) {
    dump();
    auto trackedE = new TrackedEvent{eventToTrack, eventId++};
    trackedEvents.pushFrontOne(*trackedE);
}

void EventsTracker::notifyDestruction(Event *eventToDestroy) {
    auto trackedE = new TrackedEvent{eventToDestroy, -(eventId++)};
    trackedEvents.pushFrontOne(*trackedE);
    dump();
}

void EventsTracker::notifyTransitionedExecutionStatus() {
    dump();
}

std::unique_ptr<std::ostream> EventsTracker::createDumpStream(const std::string &filename) {
    return std::make_unique<std::fstream>(filename, std::ios::binary | std::ios::out);
}

} // namespace NEO
