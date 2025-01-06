#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <vector>

#include "../../Helpers/Assert.h"
#include "../../Helpers/Console/Progress.h"
#include "../../Helpers/IO/Serialization.h"
#include "../../Helpers/Ranges/Range.h"
#include "../../Helpers/Ranges/SubRange.h"
#include "../../Helpers/String/Enumeration.h"
#include "../../Helpers/String/String.h"
#include "../TE/Data.h"

namespace PTL {
class Data {
public:
    using Hub = std::uint32_t;
    using Label = std::vector<Hub>;

    Data(){};
    Data(const std::string& fileName) { deserialize(fileName); }
    Data(TE::Data& teData)
        : fwdVertices(teData.events.size() >> 1), bwdVertices(teData.events.size() >> 1), teData(teData) {}

    void readLabelFile(const std::string& fileName) noexcept {
        std::ifstream inFile(fileName);

        if (!inFile.is_open()) {
            std::cerr << "Error: Unable to open file " << fileName << " for reading.\n";
            return;
        }

        std::string line;
        std::size_t vertexIndex = 0;

        while (std::getline(inFile, line)) {
            if (line.empty()) {
                continue;
            }

            char labelType = line[0];

            if (labelType != 'o' && labelType != 'i') {
                std::cerr << "Error: Unexpected line format: " << line << "\n";
                continue;
            }

            Label hubs;
            std::istringstream iss(line.substr(1));
            Hub hub;
            while (iss >> hub) {
                hubs.push_back(hub);
            }

            if (labelType == 'o' && teData.isDepartureEvent(Vertex(vertexIndex))) {
                AssertMsg((vertexIndex >> 1) < fwdVertices.size(),
                          "VertexIndex " << vertexIndex << " is out of bounds!");
                fwdVertices[vertexIndex >> 1] = std::move(hubs);
            } else if (labelType == 'i' && teData.isArrivalEvent(Vertex(vertexIndex))) {
                AssertMsg(((vertexIndex - 1) >> 1) < bwdVertices.size(),
                          "VertexIndex " << vertexIndex << " is out of bounds!");
                bwdVertices[(vertexIndex - 1) >> 1] = std::move(hubs);
            }

            vertexIndex += (labelType == 'i');
        }

        inFile.close();
        if (inFile.fail()) {
            std::cerr << "Error: Reading from file " << fileName << " failed.\n";
        } else {
            std::cout << "Labels loaded successfully from " << fileName << "\n";
        }
    }

    inline void clear() noexcept {
        AssertMsg(fwdVertices.size() == (teData.numberOfTEVertices()), "Not the same size!");
        AssertMsg(bwdVertices.size() == (teData.numberOfTEVertices()), "Not the same size!");

        for (Vertex v = Vertex(0); v < (teData.numberOfTEVertices()); ++v) {
            fwdVertices.clear();
            bwdVertices.clear();
        }
    }

    inline void sortLabels() noexcept {
        AssertMsg(fwdVertices.size() == (teData.numberOfTEVertices()), "Not the same size!");
        AssertMsg(bwdVertices.size() == (teData.numberOfTEVertices()), "Not the same size!");

        for (Vertex v = Vertex(0); v < (teData.numberOfTEVertices()); ++v) {
            std::sort(fwdVertices[v].begin(), fwdVertices[v].end());
            std::sort(bwdVertices[v].begin(), bwdVertices[v].end());
        }
    }

    inline size_t numberOfStops() const noexcept { return teData.numberOfStops(); }
    inline bool isStop(const StopId stop) const noexcept { return stop < numberOfStops(); }
    inline Range<StopId> stops() const noexcept { return Range<StopId>(StopId(0), StopId(numberOfStops())); }

    inline size_t numberOfTrips() const noexcept { return teData.numTrips; }
    inline bool isTrip(const TripId route) const noexcept { return route < numberOfTrips(); }
    inline Range<TripId> trips() const noexcept { return Range<TripId>(TripId(0), TripId(numberOfTrips())); }

    inline size_t numberOfStopEvents() const noexcept { return teData.events.size(); }

    inline bool isEvent(const Vertex event) const noexcept { return teData.isEvent(event); }
    inline bool isDepartureEvent(const Vertex event) const noexcept { return teData.isDepartureEvent(event); }
    inline bool isArrivalEvent(const Vertex event) const noexcept { return teData.isArrivalEvent(event); }

    void printInfo() const noexcept {
        std::cout << "PTL public transit data:" << std::endl;
        std::cout << "   Number of Stops:          " << std::setw(12) << String::prettyInt(teData.numberOfStops())
                  << std::endl;
        std::cout << "   Number of Trips:          " << std::setw(12) << String::prettyInt(teData.numberOfTrips())
                  << std::endl;
        std::cout << "   Number of Stop Events:    " << std::setw(12) << String::prettyInt(teData.numberOfStopEvents())
                  << std::endl;
        std::cout << "   Number of TE Vertices:    " << std::setw(12)
                  << String::prettyInt(teData.timeExpandedGraph.numVertices()) << std::endl;
        std::cout << "   Number of TE Edges:       " << std::setw(12)
                  << String::prettyInt(teData.timeExpandedGraph.numEdges()) << std::endl;

        auto computeStats = [](const std::vector<Label>& currentLabels) {
            std::size_t minSize = std::numeric_limits<std::size_t>::max();
            std::size_t maxSize = 0;
            std::size_t totalSize = 0;

            for (const auto& label : currentLabels) {
                std::size_t size = label.size();
                minSize = std::min(minSize, size);
                maxSize = std::max(maxSize, size);
                totalSize += size;
            }

            double avgSize = static_cast<double>(totalSize) / currentLabels.size();
            return std::make_tuple(minSize, maxSize, avgSize, totalSize);
        };

        auto [outMin, outMax, outAvg, outTotal] = computeStats(fwdVertices);
        auto [inMin, inMax, inAvg, inTotal] = computeStats(bwdVertices);

        std::cout << "Forward Labels Statistics:   " << std::endl;
        std::cout << "  Min Size:                  " << std::setw(12) << outMin << std::endl;
        std::cout << "  Max Size:                  " << std::setw(12) << outMax << std::endl;
        std::cout << "  Avg Size:                  " << std::setw(12) << outAvg << std::endl;

        std::cout << "Backward Labels Statistics:" << std::endl;

        std::cout << "  Max Size:                  " << std::setw(12) << inMin << std::endl;
        std::cout << "  Min Size:                  " << std::setw(12) << inMax << std::endl;
        std::cout << "  Avg Size:                  " << std::setw(12) << inAvg << std::endl;

        std::cout << "FWD # count:                 " << std::setw(12) << outTotal << std::endl;
        std::cout << "BWD # count:                 " << std::setw(12) << inTotal << std::endl;
        std::cout << "Both # count:                " << std::setw(12) << (outTotal + inTotal) << std::endl;

        std::cout << "   Total Size:               " << std::setw(12) << String::bytesToString(byteSize()) << std::endl;
    }

    inline void serialize(const std::string& fileName) const noexcept {
        IO::serialize(fileName, fwdVertices, bwdVertices);
        teData.serialize(fileName + ".te");
    }

    inline void deserialize(const std::string& fileName) noexcept {
        IO::deserialize(fileName, fwdVertices, bwdVertices);
        teData.deserialize(fileName + ".te");
    }

    inline long long byteSize() const noexcept {
        long long result = Vector::byteSize(fwdVertices);
        result += Vector::byteSize(bwdVertices);
        result += teData.byteSize();
        return result;
    }

    inline Label& getFwdHubs(const Vertex vertex) noexcept {
        AssertMsg(teData.isDepartureEvent(vertex), "Vertex is not valid!");

        return fwdVertices[(vertex >> 1)];
    }

    inline Label& getBwdHubs(const Vertex vertex) noexcept {
        AssertMsg(teData.isArrivalEvent(vertex), "Vertex is not valid!");

        return bwdVertices[(vertex - 1) >> 1];
    }

    std::vector<Label> fwdVertices;
    std::vector<Label> bwdVertices;
    TE::Data teData;
};
} // namespace PTL
