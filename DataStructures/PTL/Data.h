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
    Data(TE::Data& data) : fwdLabels(data.events.size() >> 1), bwdLabels(data.events.size() >> 1), data(data) {}

    void serialize(const std::string& fileName) const noexcept {
        IO::serialize(fileName, fwdLabels, bwdLabels);
        data.serialize(fileName + ".te");
    }

    void deserialize(const std::string& fileName) noexcept {
        IO::deserialize(fileName, fwdLabels, bwdLabels);
        data.deserialize(fileName + ".te");
    }

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

            if (labelType == 'o' && data.isDepartureEvent(Vertex(vertexIndex))) {
                AssertMsg((vertexIndex >> 1) < fwdLabels.size(), "VertexIndex " << vertexIndex << " is out of bounds!");
                fwdLabels[vertexIndex >> 1] = std::move(hubs);
            } else if (labelType == 'i' && data.isArrivalEvent(Vertex(vertexIndex))) {
                AssertMsg(((vertexIndex - 1) >> 1) < bwdLabels.size(),
                          "VertexIndex " << vertexIndex << " is out of bounds!");
                bwdLabels[(vertexIndex - 1) >> 1] = std::move(hubs);
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

    void printInfo() const noexcept {
        std::cout << "PTL public transit data:" << std::endl;
        std::cout << "   Number of Stops:          " << std::setw(12) << String::prettyInt(data.numberOfStops())
                  << std::endl;
        std::cout << "   Number of Trips:          " << std::setw(12) << String::prettyInt(data.numberOfTrips())
                  << std::endl;
        std::cout << "   Number of Stop Events:    " << std::setw(12) << String::prettyInt(data.numberOfStopEvents())
                  << std::endl;
        std::cout << "   Number of TE Vertices:    " << std::setw(12)
                  << String::prettyInt(data.timeExpandedGraph.numVertices()) << std::endl;
        std::cout << "   Number of TE Edges:       " << std::setw(12)
                  << String::prettyInt(data.timeExpandedGraph.numEdges()) << std::endl;

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

        auto [outMin, outMax, outAvg, outTotal] = computeStats(fwdLabels);
        auto [inMin, inMax, inAvg, inTotal] = computeStats(bwdLabels);

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

    long long byteSize() const noexcept {
        long long result = Vector::byteSize(fwdLabels);
        result += Vector::byteSize(bwdLabels);
        result += data.byteSize();
        return result;
    }

    std::size_t numberOfEvents() const { return data.numberOfStopEvents(); }

    std::size_t numberOfStops() const { return data.numberOfStops(); }

    std::size_t numberOfTrips() const { return data.numberOfTrips(); }

    std::size_t numberOfVerticesInTE() const { return data.timeExpandedGraph.numVertices(); }

    std::size_t numberOfEdgesInTE() const { return data.timeExpandedGraph.numEdges(); }

    Label& getFwdLabel(const Vertex v) {
        AssertMsg(v < numberOfVerticesInTE(), "Vertex is not valid!");
        AssertMsg(data.isDepartureEvent(v), "Vertex should be a departure event!");
        return fwdLabels[v >> 1];
    }

    Label& getBwdLabel(const Vertex v) {
        AssertMsg(v < numberOfVerticesInTE(), "Vertex is not valid!");
        AssertMsg(data.isArrivalEvent(v), "Vertex should be an arrival event!");
        return bwdLabels[(v - 1) >> 1];
    }

private:
    std::vector<Label> fwdLabels;
    std::vector<Label> bwdLabels;
    TE::Data data;
};
} // namespace PTL
