#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "../../Helpers/Assert.h"
#include "../../Helpers/Console/Progress.h"
#include "../../Helpers/IO/Serialization.h"
#include "../../Helpers/Ranges/Range.h"
#include "../../Helpers/Ranges/SubRange.h"
#include "../../Helpers/String/Enumeration.h"
#include "../../Helpers/String/String.h"
#include "../CSA/Entities/Connection.h"
#include "../Graph/Graph.h"
#include "../Intermediate/Data.h"

namespace TE {

class Data {
private:
    struct Event {
        size_t id;
        StopId stop;
        TripId trip;
        int time;

        Event(size_t id = -1, StopId stop = noStop, int time = -1, TripId trip = noTripId)
            : id(id), stop(stop), trip(trip), time(time){};
    };

public:
    Data(){};

    Data(const std::string& fileName) { deserialize(fileName); }

    inline static Data FromBinary(const std::string& fileName) noexcept {
        Data data;
        data.deserialize(fileName);
        return data;
    }

    inline static Data FromIntermediate(const Intermediate::Data& inter, const bool extractFootpaths = true) noexcept {
        Data data;
        data.stopData.clear();
        data.stopData.reserve(inter.stops.size());

        data.depEventsAtStop.clear();
        data.depEventsAtStop.assign(inter.stops.size(), {});

        data.arrEventsAtStop.clear();
        data.arrEventsAtStop.assign(inter.stops.size(), {});

        for (const Intermediate::Stop& stop : inter.stops) {
            data.stopData.emplace_back(stop);
        }

        data.numTrips = inter.trips.size();

        size_t numberOfEvents(0);
        TripId currentTrip(0);

        std::vector<CSA::Connection> connections;
        for (const Intermediate::Trip& trip : inter.trips) {
            AssertMsg(!trip.stopEvents.empty(), "Intermediate data contains trip without any stop event!");
            for (size_t i = 1; i < trip.stopEvents.size(); i++) {
                const Intermediate::StopEvent& from = trip.stopEvents[i - 1];
                const Intermediate::StopEvent& to = trip.stopEvents[i];
                connections.emplace_back(from.stopId, to.stopId, from.departureTime, to.arrivalTime, currentTrip);

                numberOfEvents += 2;
            }

            ++currentTrip;
        }
        std::sort(connections.begin(), connections.end());

        DynamicTimeExpandedGraph bobTheBuilder;
        bobTheBuilder.addVertices(numberOfEvents);

        // this allows us to keep track of the previous arrival event in order to create an edge
        std::vector<size_t> lastArrivalEventOfTrip(inter.trips.size(), numberOfEvents);

        data.events.clear();
        data.events.reserve(numberOfEvents);

        for (size_t i = 0; i < connections.size(); ++i) {
            // first departure event, then arrival
            const auto& conn = connections[i];

            size_t id = (i << 1);

            data.events.emplace_back(id, conn.departureStopId, conn.departureTime, conn.tripId);
            data.events.emplace_back(id + 1, conn.arrivalStopId, conn.arrivalTime, conn.tripId);

            bobTheBuilder.set(StopVertex, Vertex(id), conn.departureStopId);
            bobTheBuilder.set(StopVertex, Vertex(id + 1), conn.arrivalStopId);

            // add the arrival event to arrEventsAtStop
            data.arrEventsAtStop[conn.arrivalStopId].emplace_back(id + 1);

            // check if can create an edge to the previous arrival event
            // ** Trip Edge **
            AssertMsg(conn.tripId < lastArrivalEventOfTrip.size(), "Trip is out of bounds!");
            if (lastArrivalEventOfTrip[conn.tripId] != numberOfEvents) {
                AssertMsg(lastArrivalEventOfTrip[conn.tripId] < numberOfEvents, "Last Arrival Event is out of bounds!");

                bobTheBuilder.addEdge(Vertex(lastArrivalEventOfTrip[conn.tripId]), Vertex(id + 1));
            }

            lastArrivalEventOfTrip[conn.tripId] = id + 1;

            // try to add ** Transfer edge ** to previous departure event at stop
            AssertMsg(conn.departureStopId < data.stopData.size(), "Departure StopId is out of bounds!");

            if (!data.depEventsAtStop[conn.departureStopId].empty()) {
                Vertex prevDep = Vertex(data.depEventsAtStop[conn.departureStopId].back());
                AssertMsg(prevDep < numberOfEvents, "Previous Departure Event is out of bounds!");

                bobTheBuilder.addEdge(prevDep, Vertex(id));
            }

            // also add departure vertex to stop
            data.depEventsAtStop[conn.departureStopId].emplace_back(id);

            // add the edge from departure => arrival vertex
            bobTheBuilder.addEdge(Vertex(id), Vertex(id + 1));
        }

        // now we need to add the edges from arrival events to the same stop && per footpath reachable stops departure
        // events

        auto addEdgeToReachableDepartureEvent =
            [&](const Vertex fromVertex, const StopId toStop, const int timeAtStop) {
            AssertMsg(fromVertex < numberOfEvents, "From Event is out of bounds!");
            AssertMsg(data.isStop(toStop), "To Stop is out of bounds!");
            AssertMsg(data.events[fromVertex].time <= timeAtStop, "Time travel!");

            const auto& departureEventAtToStop = data.depEventsAtStop[toStop];

            if (departureEventAtToStop.empty()) {
                return;
            }

            if (timeAtStop > data.events[departureEventAtToStop.back()].time) {
                return;
            }

            for (auto& event : departureEventAtToStop) {
                if (timeAtStop <= data.events[event].time) {
                    bobTheBuilder.addEdge(fromVertex, Vertex(event));
                    return;
                }
            }
            AssertMsg(false, "No edge added??");
            return;
        };

        for (size_t i = 0; i < connections.size(); ++i) {
            Vertex arrEvent = Vertex((i << 1) + 1);
            AssertMsg(data.isEvent(arrEvent), "Arrival Event is not an event!");
            AssertMsg(data.isArrivalEvent(arrEvent), "Arrival Event is not an arrival event!");

            StopId fromStop = data.events[arrEvent].stop;

            int time = data.events[arrEvent].time;

            addEdgeToReachableDepartureEvent(arrEvent, fromStop, time + data.stopData[fromStop].minTransferTime);

            if (!extractFootpaths) {
                continue;
            }

            for (const auto& edge : inter.transferGraph.edgesFrom(fromStop)) {
                StopId toStop = StopId(inter.transferGraph.get(ToVertex, edge));
                addEdgeToReachableDepartureEvent(arrEvent, toStop, time + inter.transferGraph.get(TravelTime, edge));
            }
        }

        for (const auto& [edge, fromVertex] : bobTheBuilder.edgesWithFromVertex()) {
            Vertex toVertex = bobTheBuilder.get(ToVertex, edge);

            AssertMsg(data.isEvent(fromVertex), "FromVertex is not valid!");
            AssertMsg(data.isEvent(toVertex), "ToVertex is not valid!");

            int fromTime = data.events[fromVertex].time;
            int toTime = data.events[toVertex].time;

            AssertMsg(fromTime <= toTime, "Time travel!");

            bobTheBuilder.set(TravelTime, edge, toTime - fromTime);
        }

        bobTheBuilder.sortEdges(ToVertex);
        Graph::move(std::move(bobTheBuilder), data.timeExpandedGraph);

        Graph::printInfo(data.timeExpandedGraph);

        for (StopId stop(0); stop < data.stopData.size(); ++stop) {
            std::sort(data.arrEventsAtStop[stop].begin(), data.arrEventsAtStop[stop].end(),
                      [&](const size_t left, const size_t right) {
                return data.events[left].time < data.events[right].time;
            });

            AssertMsg(
                std::is_sorted(data.arrEventsAtStop[stop].begin(), data.arrEventsAtStop[stop].end(),
                               [&](const size_t left,
                                   const size_t right) { return data.events[left].time < data.events[right].time; }),
                "Arrival Events are not sorted correctly!");

            AssertMsg(
                std::is_sorted(data.depEventsAtStop[stop].begin(), data.depEventsAtStop[stop].end(),
                               [&](const size_t left,
                                   const size_t right) { return data.events[left].time < data.events[right].time; }),
                "Departure Events are not sorted correctly!");
        }

        return data;
    }

public:
    inline size_t numberOfStops() const noexcept { return stopData.size(); }
    inline bool isStop(const StopId stop) const noexcept { return stop < numberOfStops(); }
    inline Range<StopId> stops() const noexcept { return Range<StopId>(StopId(0), StopId(numberOfStops())); }

    inline size_t numberOfTrips() const noexcept { return numTrips; }
    inline bool isTrip(const TripId route) const noexcept { return route < numberOfTrips(); }
    inline Range<TripId> trips() const noexcept { return Range<TripId>(TripId(0), TripId(numberOfTrips())); }

    inline size_t numberOfStopEvents() const noexcept { return events.size(); }
    inline size_t numberOfTEVertices() const noexcept { return events.size(); }
    inline bool isEvent(const Vertex event) const noexcept { return event < events.size(); }
    inline bool isDepartureEvent(const Vertex event) const noexcept { return !isArrivalEvent(event); }
    inline bool isArrivalEvent(const Vertex event) const noexcept { return (event & 1); }

    inline int getTimeOfVertex(Vertex vertex) const noexcept {
        AssertMsg(isEvent(vertex), "Vertex " << vertex << " is not valid!");

        return events[vertex].time;
    }

    inline Vertex getFirstReachableDepartureVertexAtStop(const StopId stop, const int time) const noexcept {
        AssertMsg(isStop(stop), "Stop is not valid");

        const auto& departureEventAtToStop = depEventsAtStop[stop];

        if ((departureEventAtToStop.empty()) || (time > events[departureEventAtToStop.back()].time)) {
            return Vertex(numberOfTEVertices());
        }

        for (auto& event : departureEventAtToStop) {
            if (time <= events[event].time) {
                return Vertex(event);
            }
        }
        AssertMsg(false, "No edge added??");
        return Vertex(numberOfTEVertices());
    }

    inline void printInfo() const noexcept {
        std::cout << "TE public transit data:" << std::endl;
        std::cout << "   Number of Stops:          " << std::setw(12) << String::prettyInt(numberOfStops())
                  << std::endl;
        std::cout << "   Number of Trips:          " << std::setw(12) << String::prettyInt(numberOfTrips())
                  << std::endl;
        std::cout << "   Number of Stop Events:    " << std::setw(12) << String::prettyInt(numberOfStopEvents())
                  << std::endl;
        std::cout << "   Number of TE Vertices:    " << std::setw(12)
                  << String::prettyInt(timeExpandedGraph.numVertices()) << std::endl;
        std::cout << "   Number of TE Edges:       " << std::setw(12) << String::prettyInt(timeExpandedGraph.numEdges())
                  << std::endl;
    }

    inline void serialize(const std::string& fileName) const noexcept {
        IO::serialize(fileName, stopData, events, depEventsAtStop, arrEventsAtStop, numTrips);
        timeExpandedGraph.writeBinary(fileName + ".graph");
    }

    inline void deserialize(const std::string& fileName) noexcept {
        IO::deserialize(fileName, stopData, events, depEventsAtStop, arrEventsAtStop, numTrips);
        timeExpandedGraph.readBinary(fileName + ".graph");
    }

    inline long long byteSize() const noexcept {
        long long result = Vector::byteSize(stopData);
        result += Vector::byteSize(events);
        result += Vector::byteSize(depEventsAtStop);
        result += Vector::byteSize(arrEventsAtStop);
        result += sizeof(numTrips);
        result += timeExpandedGraph.byteSize();
        return result;
    }

    inline std::vector<size_t>& getDeparturesOfStop(const StopId stop) noexcept {
        AssertMsg(isStop(stop), "Stop is not a stop!");
        return depEventsAtStop[stop];
    }

    inline std::vector<size_t>& getArrivalsOfStop(const StopId stop) noexcept {
        AssertMsg(isStop(stop), "Stop is not a stop!");
        return arrEventsAtStop[stop];
    }

    inline void writeOrderForAkiba(const std::string& fileName) const noexcept {
        std::vector<size_t> order;
        order.reserve(numberOfStopEvents());

        // first all departure events in reverse order
        for (StopId stop(0); stop < numberOfStops(); ++stop) {
            order.insert(order.end(), depEventsAtStop[stop].begin(), depEventsAtStop[stop].end());
            order.insert(order.end(), arrEventsAtStop[stop].begin(), arrEventsAtStop[stop].end());
        }
        // // then all arrival events in reverse order
        // for (StopId stop(0); stop < numberOfStops(); ++stop) {
        //     order.insert(order.end(), arrEventsAtStop[stop].begin(), arrEventsAtStop[stop].end());
        // }

        std::ofstream file;

        file.open(fileName);

        file << order.size() << std::endl;

        for (auto& val : order) {
            file << val << std::endl;
        }
        file.close();
        AssertMsg(file.good(), "Something went wrong!");
    }

public:
    std::vector<RAPTOR::Stop> stopData;
    std::vector<Event> events;
    std::vector<std::vector<size_t>> depEventsAtStop;
    std::vector<std::vector<size_t>> arrEventsAtStop;
    size_t numTrips;

    TimeExpandedGraph timeExpandedGraph;
};
} // namespace TE
