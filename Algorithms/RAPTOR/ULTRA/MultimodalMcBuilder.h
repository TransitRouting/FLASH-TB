#pragma once

#include "MultimodalMcShortcutSearch.h"
#include <algorithm>

#include "../../../DataStructures/RAPTOR/Data.h"
#include "../../../Helpers/Console/Progress.h"
#include "../../../Helpers/MultiThreading.h"
#include "../../../Helpers/Timer.h"

namespace RAPTOR::ULTRA {

template <bool DEBUG = false, int TIME_FACTOR = 1>
class MultimodalMcBuilder {
public:
    inline static constexpr bool Debug = DEBUG;
    inline static constexpr int TimeFactor = TIME_FACTOR;
    using Type = MultimodalMcBuilder<Debug, TimeFactor>;

public:
    MultimodalMcBuilder(const Data& data, const TransferGraph& transitiveTransferGraph)
        : data(data), transitiveTransferGraph(transitiveTransferGraph) {
        shortcutGraph.addVertices(data.numberOfStops());
        for (const Vertex vertex : shortcutGraph.vertices()) {
            shortcutGraph.set(Coordinates, vertex, data.transferGraph.get(Coordinates, vertex));
        }
    }

    void computeShortcuts(const ThreadPinning& threadPinning, const int intermediateWitnessTransferLimit = 0,
                          const int finalWitnessTransferLimit = 0, const int minDepartureTime = -never,
                          const int maxDepartureTime = never, const bool verbose = true) noexcept {
        if (verbose)
            std::cout << "Computing shortcuts with " << threadPinning.numberOfThreads << " threads." << std::endl;

        Progress progress(data.numberOfStops(), verbose);
        omp_set_num_threads(threadPinning.numberOfThreads);
#pragma omp parallel
        {
            threadPinning.pinThread();

            DynamicTransferGraph localShortcutGraph = shortcutGraph;
            MultimodalMcShortcutSearch<Debug, TimeFactor> shortcutSearch(
                data, transitiveTransferGraph, localShortcutGraph, intermediateWitnessTransferLimit,
                finalWitnessTransferLimit);

#pragma omp for schedule(dynamic)
            for (size_t i = 0; i < data.numberOfStops(); i++) {
                shortcutSearch.run(StopId(i), minDepartureTime, maxDepartureTime);
                progress++;
            }

#pragma omp critical
            {
                for (const Vertex from : shortcutGraph.vertices()) {
                    for (const Edge edge : localShortcutGraph.edgesFrom(from)) {
                        const Vertex to = localShortcutGraph.get(ToVertex, edge);
                        if (!shortcutGraph.hasEdge(from, to)) {
                            shortcutGraph.addEdge(from, to).set(TravelTime, localShortcutGraph.get(TravelTime, edge));
                        } else {
                            AssertMsg(shortcutGraph.get(TravelTime, shortcutGraph.findEdge(from, to))
                                          == localShortcutGraph.get(TravelTime, edge),
                                      "Edge from " << from << " to " << to << " has inconclusive travel time ("
                                                   << shortcutGraph.get(TravelTime, shortcutGraph.findEdge(from, to))
                                                   << ", " << localShortcutGraph.get(TravelTime, edge) << ")");
                        }
                    }
                }
            }
        }
        progress.finished();
    }

    inline const DynamicTransferGraph& getShortcutGraph() const noexcept { return shortcutGraph; }

    inline DynamicTransferGraph& getShortcutGraph() noexcept { return shortcutGraph; }

private:
    const Data& data;
    const TransferGraph& transitiveTransferGraph;
    DynamicTransferGraph shortcutGraph;
};

} // namespace RAPTOR::ULTRA
