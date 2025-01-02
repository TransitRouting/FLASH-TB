#pragma once

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "../RAPTOR/Entities/Route.h"
#include "../RAPTOR/Entities/Stop.h"
// #include "Entities/Journey.h"

#include "../../Helpers/Assert.h"
#include "../../Helpers/IO/Serialization.h"
#include "../../Helpers/Ranges/Range.h"
#include "../../Helpers/Ranges/SubRange.h"
#include "../../Helpers/String/Enumeration.h"
#include "../../Helpers/String/String.h"
#include "../../Helpers/Timer.h"
#include "../Container/Map.h"
#include "../Container/Set.h"
#include "../Graph/Graph.h"
#include "../Intermediate/Data.h"

namespace TD {

class Data {
public:
    Data() {}

    Data(const std::string& fileName) { deserialize(fileName); }

    inline static Data FromBinary(const std::string& fileName) noexcept {
        Data data;
        data.deserialize(fileName);
        return data;
    }

    inline static Data FromIntermediate(const Intermediate::Data& inter) noexcept {
        return FromIntermediate(inter, inter.greedyfifoRoutes());
    }

    inline static Data FromIntermediate(const Intermediate::Data& inter,
                                        const std::vector<std::vector<Intermediate::Trip>>& routes) noexcept {
        Data data;
        for (const Intermediate::Stop& stop : inter.stops) {
            data.stopData.emplace_back(stop);
        }

        size_t numberOfRouteNodesToAdd = 0;

        data.stopsOfRoute.assign(routes.size(), {});
        data.numberOfStopEvents = 0;

        for (RouteId i(0); i < routes.size(); ++i) {
            const auto& route = routes[i];
            AssertMsg(!route.empty(), "A route should not be empty!");

            numberOfRouteNodesToAdd += route[0].stopEvents.size();

            for (auto& event : route[0].stopEvents) {
                data.stopsOfRoute[i].emplace_back(event.stopId);
            }
        }

        DynamicTimeDependentRouteGraph builderGraph;
        builderGraph.addVertices(inter.numberOfStops() + numberOfRouteNodesToAdd);

        for (Vertex v(0); v < inter.numberOfStops(); ++v) {
            builderGraph.set(RouteVertex, v, noRouteId);
        }

        Vertex currentVertex = Vertex(inter.numberOfStops());
        for (RouteId i = RouteId(0); i < routes.size(); i++) {
            const auto& route = routes[i];
            AssertMsg(!route.empty(), "A route should not be empty!");

            const auto& firstTrip = route[0];
            data.routeData.emplace_back(firstTrip.routeName, firstTrip.type);

            data.numberOfStopEvents += route.size() * firstTrip.stopEvents.size();

            AssertMsg(!firstTrip.stopEvents.empty(), "Trip is empty??");
            for (size_t j = 1; j < firstTrip.stopEvents.size(); ++j) {
                std::vector<std::pair<uint32_t, uint32_t>> durationFunction;
                durationFunction.reserve(32);

                for (const Intermediate::Trip& trip : route) {
                    AssertMsg(trip.stopEvents[j - 1].departureTime <= trip.stopEvents[j].arrivalTime, "Time travel!");
                    durationFunction.emplace_back(trip.stopEvents[j - 1].departureTime,
                                                  trip.stopEvents[j].arrivalTime
                                                      - trip.stopEvents[j - 1].departureTime);
                }

                auto newEdge = builderGraph.addEdge(Vertex(currentVertex + j - 1), Vertex(currentVertex + j));

                newEdge.set(DurationFunction, durationFunction);
                newEdge.set(TravelTime, -1);
                newEdge.set(TransferCost, 0);
            }

            for (size_t j = 0; j < firstTrip.stopEvents.size(); ++j, ++currentVertex) {
                auto enteringEdge = builderGraph.addEdge(currentVertex, Vertex(firstTrip.stopEvents[j].stopId));
                auto exitingEdge = builderGraph.addEdge(Vertex(firstTrip.stopEvents[j].stopId), currentVertex);

                enteringEdge.set(TravelTime, 0);
                exitingEdge.set(TravelTime, 0);

                enteringEdge.set(TransferCost, 0);
                exitingEdge.set(TransferCost, 1);

                builderGraph.set(RouteVertex, currentVertex, i);
            }
        }

        for (const auto [edge, from] : inter.transferGraph.edgesWithFromVertex()) {
            builderGraph.addEdge(from, inter.transferGraph.get(ToVertex, edge))
                .set(TravelTime, inter.transferGraph.get(TravelTime, edge));
        }

        for (auto& val : builderGraph[DurationFunction]) {
            val.emplace_back(intMax, intMax);
        }

        builderGraph.sortEdges(ToVertex);

        Graph::move(std::move(builderGraph), data.timeDependentGraph);

        return data;
    }

public:
    inline size_t numberOfStops() const noexcept { return stopData.size(); }
    inline bool isStop(const Vertex stop) const noexcept { return stop < numberOfStops(); }
    inline Range<StopId> stops() const noexcept { return Range<StopId>(StopId(0), StopId(numberOfStops())); }

    inline size_t numberOfRoutes() const noexcept { return routeData.size(); }
    inline bool isRoute(const RouteId route) const noexcept { return route < numberOfRoutes(); }
    inline Range<RouteId> routes() const noexcept { return Range<RouteId>(RouteId(0), RouteId(numberOfRoutes())); }

    inline size_t getNumberOfStopEvents() const noexcept { return numberOfStopEvents; }

    inline size_t numberOfStopsInRoute(const RouteId route) const noexcept {
        AssertMsg(isRoute(route), "The id " << route << " does not represent a route!");
        return stopsOfRoute[route].size();
    }

    inline std::vector<StopId>& getStopsOfRoute(const RouteId route) noexcept {
        AssertMsg(isRoute(route), "The id " << route << " does not represent a route!");
        return stopsOfRoute[route];
    }

    inline void printInfo() const noexcept {
        size_t totalNumOfEntries = 0;
        size_t maxNumOfEntries = 0;
        size_t totalNumOfRouteEdges = 0;

        for (const auto edge : timeDependentGraph.edges()) {
            totalNumOfEntries += timeDependentGraph.get(DurationFunction, edge).size();
            maxNumOfEntries = std::max(maxNumOfEntries, timeDependentGraph.get(DurationFunction, edge).size());

            totalNumOfRouteEdges += (timeDependentGraph.get(DurationFunction, edge).size() > 1);
        }

        std::cout << "TD public transit data:" << std::endl;
        std::cout << "   Number of Stops:          " << std::setw(12) << String::prettyInt(numberOfStops())
                  << std::endl;
        std::cout << "   Number of Routes:         " << std::setw(12) << String::prettyInt(numberOfRoutes())
                  << std::endl;
        std::cout << "   Number of Stop Events:    " << std::setw(12) << String::prettyInt(getNumberOfStopEvents())
                  << std::endl;
        std::cout << "   Number of TD Vertices:    " << std::setw(12)
                  << String::prettyInt(timeDependentGraph.numVertices()) << std::endl;
        std::cout << "   Number of TD Edges:       " << std::setw(12)
                  << String::prettyInt(timeDependentGraph.numEdges()) << std::endl;
        std::cout << "   Total Size:               " << std::setw(12) << String::bytesToString(byteSize()) << std::endl;
        std::cout << "   Avg # entries on edge:    " << std::setw(12)
                  << String::prettyDouble(static_cast<double>(totalNumOfEntries / totalNumOfRouteEdges)) << std::endl;
        std::cout << "   Max # entries on edge:    " << std::setw(12) << String::prettyInt(maxNumOfEntries)
                  << std::endl;
    }

    inline void serialize(const std::string& fileName) const noexcept {
        IO::serialize(fileName, stopData, routeData, stopsOfRoute, numberOfStopEvents);
        timeDependentGraph.writeBinary(fileName + ".graph");
    }

    inline void deserialize(const std::string& fileName) noexcept {
        IO::deserialize(fileName, stopData, routeData, stopsOfRoute, numberOfStopEvents);
        timeDependentGraph.readBinary(fileName + ".graph");
    }

    inline long long byteSize() const noexcept {
        long long result = Vector::byteSize(stopData);
        result += Vector::byteSize(routeData);
        result += Vector::byteSize(stopsOfRoute);
        result += sizeof(size_t);
        result += timeDependentGraph.byteSize();
        return result;
    }

public:
    std::vector<RAPTOR::Stop> stopData;
    std::vector<RAPTOR::Route> routeData;
    std::vector<std::vector<StopId>> stopsOfRoute;

    size_t numberOfStopEvents;

    TimeDependentRouteGraph timeDependentGraph;
};

} // namespace TD
