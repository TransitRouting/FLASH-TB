#pragma once

#include <string>

#include "../../DataStructures/CSA/Data.h"
#include "../../DataStructures/Graph/Graph.h"
#include "../../DataStructures/Graph/Utils/IO.h"
#include "../../DataStructures/GTFS/Data.h"
#include "../../DataStructures/Intermediate/Data.h"
#include "../../DataStructures/RAPTOR/Data.h"
#include "../../DataStructures/RAPTOR/MultimodalData.h"
#include "../../DataStructures/TD/Data.h"
#include "../../DataStructures/TE/Data.h"
#include "../../DataStructures/TripBased/MultimodalData.h"
#include "../../Shell/Shell.h"

using namespace Shell;

class ParseGTFS : public ParameterizedCommand {
public:
    ParseGTFS(BasicShell& shell)
        : ParameterizedCommand(shell, "parseGTFS",
                               "Parses raw GTFS data from the given directory "
                               "and converts it to a binary representation.") {
        addParameter("Input directory");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const std::string gtfsDirectory = getParameter("Input directory");
        const std::string outputFile = getParameter("Output file");

        GTFS::Data data = GTFS::Data::FromGTFS(gtfsDirectory);
        data.printInfo();
        data.serialize(outputFile);
    }
};

class GTFSToIntermediate : public ParameterizedCommand {
public:
    GTFSToIntermediate(BasicShell& shell)
        : ParameterizedCommand(shell, "gtfsToIntermediate",
                               "Converts binary GTFS data to the intermediate network format.") {
        addParameter("Input directory");
        addParameter("First day");
        addParameter("Last day");
        addParameter("Use days of operation?");
        addParameter("Use frequencies?");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const std::string gtfsDirectory = getParameter("Input directory");
        const std::string outputFile = getParameter("Output file");
        const int firstDay = stringToDay(getParameter("First day"));
        const int lastDay = stringToDay(getParameter("Last day"));
        const bool useDaysOfOperation = getParameter<bool>("Use days of operation?");
        const bool useFrequencies = getParameter<bool>("Use frequencies?");

        GTFS::Data gtfs = GTFS::Data::FromBinary(gtfsDirectory);
        gtfs.printInfo();
        Intermediate::Data inter =
            Intermediate::Data::FromGTFS(gtfs, firstDay, lastDay, !useDaysOfOperation, !useFrequencies);
        inter.printInfo();
        inter.serialize(outputFile);
    }
};

class IntermediateToCSA : public ParameterizedCommand {
public:
    IntermediateToCSA(BasicShell& shell)
        : ParameterizedCommand(shell, "intermediateToCSA", "Converts binary intermediate data to CSA network format.") {
        addParameter("Input file");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const std::string inputFile = getParameter("Input file");
        const std::string outputFile = getParameter("Output file");

        Intermediate::Data inter = Intermediate::Data::FromBinary(inputFile);
        inter.printInfo();
        CSA::Data data = CSA::Data::FromIntermediate(inter);
        data.printInfo();
        data.serialize(outputFile);
    }
};

class IntermediateToRAPTOR : public ParameterizedCommand {
public:
    IntermediateToRAPTOR(BasicShell& shell)
        : ParameterizedCommand(shell, "intermediateToRAPTOR",
                               "Converts binary intermediate data to RAPTOR network format.") {
        addParameter("Input file");
        addParameter("Output file");
        addParameter("Route type", "FIFO", {"Geographic", "FIFO", "Opt-FIFO", "Offset"});
    }

    virtual void execute() noexcept {
        const std::string inputFile = getParameter("Input file");
        const std::string outputFile = getParameter("Output file");
        const std::string routeTypeString = getParameter("Route type");
        int routeType;
        if (routeTypeString == "Geographic") {
            routeType = 0;
        } else if (routeTypeString == "FIFO") {
            routeType = 1;
        } else if (routeTypeString == "Opt-FIFO") {
            routeType = 2;
        } else if (routeTypeString == "Offset") {
            routeType = 3;
        } else {
            std::cout << "Please provide one of the valid arguments for 'Route Type'" << std::endl;
            return;
        }

        Intermediate::Data inter = Intermediate::Data::FromBinary(inputFile);
        inter.printInfo();
        RAPTOR::Data data = RAPTOR::Data::FromIntermediate(inter, routeType);
        data.printInfo();
        Graph::printInfo(data.transferGraph);
        data.transferGraph.printAnalysis();
        data.serialize(outputFile);
    }
};

class IntermediateToTD : public ParameterizedCommand {
public:
    IntermediateToTD(BasicShell& shell)
        : ParameterizedCommand(shell, "intermediateToTD", "Converts binary intermediate data to TD format.") {
        addParameter("Input file");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const std::string inputFile = getParameter("Input file");
        const std::string outputFile = getParameter("Output file");

        Intermediate::Data inter = Intermediate::Data::FromBinary(inputFile);
        inter.printInfo();

        TD::Data data = TD::Data::FromIntermediate(inter);
        data.printInfo();
        Graph::printInfo(data.timeDependentGraph);
        data.serialize(outputFile);
    }
};

class IntermediateToTE : public ParameterizedCommand {
public:
    IntermediateToTE(BasicShell& shell)
        : ParameterizedCommand(shell, "intermediateToTE", "Converts binary intermediate data to TE format.") {
        addParameter("Input file");
        addParameter("Output file");
        addParameter("Extract Footpaths?", "true");
    }

    virtual void execute() noexcept {
        const std::string inputFile = getParameter("Input file");
        const std::string outputFile = getParameter("Output file");
        const bool extractFootpaths = getParameter<bool>("Extract Footpaths?");

        Intermediate::Data inter = Intermediate::Data::FromBinary(inputFile);
        inter.printInfo();

        TE::Data data = TE::Data::FromIntermediate(inter, extractFootpaths);
        data.printInfo();
        Graph::printInfo(data.timeExpandedGraph);
        data.serialize(outputFile);
    }
};

class ExportTEGraphToDimacs : public ParameterizedCommand {
public:
    ExportTEGraphToDimacs(BasicShell& shell)
        : ParameterizedCommand(shell, "exportTEGraphToDimacs",
                               "Write the TE-Graph to a file in Dimacs format. As edge weights, the transfer cost is "
                               "chosen (i.e., edges are weighted either 0 or 1).") {
        addParameter("TE binary");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const std::string inputFile = getParameter("TE binary");
        const std::string outputFile = getParameter("Output file");

        TE::Data data(inputFile);
        data.printInfo();

        Graph::toDimacs(outputFile, data.timeExpandedGraph, data.timeExpandedGraph[TransferCost]);
    }
};

class BuildMultimodalRAPTORData : public ParameterizedCommand {
public:
    BuildMultimodalRAPTORData(BasicShell& shell)
        : ParameterizedCommand(shell, "buildMultimodalRAPTORData",
                               "Builds multimodal RAPTOR data based on RAPTOR data.") {
        addParameter("RAPTOR data");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const RAPTOR::Data raptorData(getParameter("RAPTOR data"));
        raptorData.printInfo();
        const RAPTOR::MultimodalData multimodalData(raptorData);
        multimodalData.printInfo();
        multimodalData.serialize(getParameter("Output file"));
    }
};

class AddModeToMultimodalRAPTORData : public ParameterizedCommand {
public:
    AddModeToMultimodalRAPTORData(BasicShell& shell)
        : ParameterizedCommand(shell, "addModeToMultimodalRAPTORData",
                               "Adds a transfer graph for the specified mode to "
                               "multimodal RAPTOR data.") {
        addParameter("Multimodal RAPTOR data");
        addParameter("Transfer graph");
        addParameter("Mode");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        RAPTOR::MultimodalData multimodalData(getParameter("Multimodal RAPTOR data"));
        multimodalData.printInfo();
        RAPTOR::TransferGraph graph;
        graph.readBinary(getParameter("Transfer graph"));
        const size_t mode = RAPTOR::getTransferModeFromName(getParameter("Mode"));
        multimodalData.addTransferGraph(mode, graph);
        multimodalData.printInfo();
        multimodalData.serialize(getParameter("Output file"));
    }
};

class BuildMultimodalTripBasedData : public ParameterizedCommand {
public:
    BuildMultimodalTripBasedData(BasicShell& shell)
        : ParameterizedCommand(shell, "buildMultimodalTripBasedData",
                               "Builds multimodal Trip-Based data based on Trip-Based data.") {
        addParameter("Trip-Based data");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const TripBased::Data tripBasedData(getParameter("Trip-Based data"));
        tripBasedData.printInfo();
        const TripBased::MultimodalData multimodalData(tripBasedData);
        multimodalData.printInfo();
        multimodalData.serialize(getParameter("Output file"));
    }
};

class AddModeToMultimodalTripBasedData : public ParameterizedCommand {
public:
    AddModeToMultimodalTripBasedData(BasicShell& shell)
        : ParameterizedCommand(shell, "addModeToMultimodalTripBasedData",
                               "Adds a transfer graph for the specified mode to "
                               "multimodal Trip-Based data.") {
        addParameter("Multimodal Trip-Based data");
        addParameter("Transfer graph");
        addParameter("Mode");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        TripBased::MultimodalData multimodalData(getParameter("Multimodal Trip-Based data"));
        multimodalData.printInfo();
        TransferGraph graph;
        graph.readBinary(getParameter("Transfer graph"));
        const size_t mode = RAPTOR::getTransferModeFromName(getParameter("Mode"));
        multimodalData.addTransferGraph(mode, graph);
        multimodalData.printInfo();
        multimodalData.serialize(getParameter("Output file"));
    }
};

class LoadDimacsGraph : public ParameterizedCommand {
public:
    LoadDimacsGraph(BasicShell& shell)
        : ParameterizedCommand(shell, "loadDimacsGraph", "Converts DIMACS graph data to our transfer graph format.") {
        addParameter("Input file");
        addParameter("Output file");
        addParameter("Graph type", "dynamic", {"static", "dynamic"});
        addParameter("Coordinate factor", "0.000001");
    }

    virtual void execute() noexcept {
        std::string graphType = getParameter("Graph type");
        if (graphType == "static") {
            load<TransferGraph>();
        } else {
            load<DynamicTransferGraph>();
        }
    }

private:
    template <typename GRAPH_TYPE>
    inline void load() const noexcept {
        DimacsGraphWithCoordinates dimacs;
        dimacs.fromDimacs<true>(getParameter("Input file"), getParameter<double>("Coordinate factor"));
        Graph::printInfo(dimacs);
        dimacs.printAnalysis();
        GRAPH_TYPE graph;
        Graph::move(std::move(dimacs), graph);
        Graph::printInfo(graph);
        graph.printAnalysis();
        graph.writeBinary(getParameter("Output file"));
    }
};

class WriteIntermediateToCSV : public ParameterizedCommand {
public:
    WriteIntermediateToCSV(BasicShell& shell)
        : ParameterizedCommand(shell, "writeIntermediateToCSV", "Writes all the intermediate Data into csv files.") {
        addParameter("Intermediate Binary");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const std::string networkFile = getParameter("Intermediate Binary");
        const std::string outputFile = getParameter("Output file");

        Intermediate::Data network = Intermediate::Data::FromBinary(networkFile);
        network.writeCSV(outputFile);
    }
};

class WriteRAPTORToCSV : public ParameterizedCommand {
public:
    WriteRAPTORToCSV(BasicShell& shell)
        : ParameterizedCommand(shell, "writeRAPTORToCSV", "Writes all the RAPTOR Data into csv files.") {
        addParameter("RAPTOR Binary");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const std::string networkFile = getParameter("RAPTOR Binary");
        const std::string outputFile = getParameter("Output file");

        RAPTOR::Data network = RAPTOR::Data::FromBinary(networkFile);
        network.dontUseImplicitDepartureBufferTimes();
        network.dontUseImplicitArrivalBufferTimes();
        network.writeCSV(outputFile);
    }
};

class WriteLayoutGraphToGraphML : public ParameterizedCommand {
public:
    WriteLayoutGraphToGraphML(BasicShell& shell)
        : ParameterizedCommand(shell, "writeLayoutGraphToGraphML", "Writes the Layout Graph into a GraphML file.") {
        addParameter("RAPTOR Binary");
        addParameter("Output file (Layout Graph)");
    }

    virtual void execute() noexcept {
        const std::string networkFile = getParameter("RAPTOR Binary");
        const std::string outputFileLayout = getParameter("Output file (Layout Graph)");

        RAPTOR::Data network = RAPTOR::Data(networkFile);
        network.createGraphForMETIS(RAPTOR::TRIP_WEIGHTED | RAPTOR::TRANSFER_WEIGHTED, true);

        Graph::toGML(outputFileLayout, network.layoutGraph);
    }
};

class WriteTripBasedToCSV : public ParameterizedCommand {
public:
    WriteTripBasedToCSV(BasicShell& shell)
        : ParameterizedCommand(shell, "writeTripBasedToCSV", "Writes all the TripBased Data into csv files.") {
        addParameter("Trip Based Binary");
        addParameter("Output file");
    }

    virtual void execute() noexcept {
        const std::string networkFile = getParameter("Trip Based Binary");
        const std::string outputFile = getParameter("Output file");

        TripBased::Data network = TripBased::Data(networkFile);
        network.raptorData.dontUseImplicitDepartureBufferTimes();
        network.raptorData.dontUseImplicitArrivalBufferTimes();
        network.raptorData.writeCSV(outputFile);

        Graph::toEdgeListCSV(outputFile + "transfers", network.stopEventGraph);
    }
};

class WriteTripBasedToGraphML : public ParameterizedCommand {
public:
    WriteTripBasedToGraphML(BasicShell& shell)
        : ParameterizedCommand(shell, "writeTripBasedToGraphML", "Writes the StopEvent Graph into a GraphML file.") {
        addParameter("Trip Based Binary");
        addParameter("Output file (StopEvent Graph)");
        addParameter("Output file (Layout Graph)");
    }

    virtual void execute() noexcept {
        const std::string networkFile = getParameter("Trip Based Binary");
        const std::string outputFileStopEvent = getParameter("Output file (StopEvent Graph)");
        const std::string outputFileLayout = getParameter("Output file (Layout Graph)");

        TripBased::Data network = TripBased::Data(networkFile);
        network.raptorData.createGraphForMETIS(RAPTOR::TRIP_WEIGHTED | RAPTOR::TRANSFER_WEIGHTED, true);

        Graph::toGML(outputFileLayout, network.raptorData.layoutGraph);
        Graph::toGML(outputFileStopEvent, network.stopEventGraph);
    }
};
