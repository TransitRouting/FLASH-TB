#pragma once

#include <iostream>
#include <string>

#include "../../Algorithms/DepthFirstSearch.h"
#include "../../DataStructures/PTL/Data.h"
#include "../../DataStructures/TE/Data.h"
#include "../../Helpers/MultiThreading.h"
#include "../../Helpers/String/String.h"
#include "../../Shell/Shell.h"

using namespace Shell;

class TEToPTL : public ParameterizedCommand {
public:
    TEToPTL(BasicShell& shell) : ParameterizedCommand(shell, "tEToPTL", "Creates a PTL object given the TE binary.") {
        addParameter("Input file (TE binary)");
        addParameter("Output file (PTL binary)");
    }

    virtual void execute() noexcept {
        const std::string inputFile = getParameter("Input file (TE binary)");
        const std::string outputFile = getParameter("Output file (PTL binary)");

        TE::Data data(inputFile);
        data.printInfo();

        PTL::Data ptl(data);
        ptl.printInfo();

        ptl.serialize(outputFile);
    }
};

class LoadLabelFile : public ParameterizedCommand {
public:
    LoadLabelFile(BasicShell& shell)
        : ParameterizedCommand(shell, "loadLabelFile", "Loads labels from the given file and saves it into PTL.") {
        addParameter("Input file (label file)");
        addParameter("Input file (PTL binary)");
        addParameter("Output file (PTL binary)");
    }

    virtual void execute() noexcept {
        const std::string labelFile = getParameter("Input file (label file)");
        const std::string inputFile = getParameter("Input file (PTL binary)");
        const std::string outputFile = getParameter("Output file (PTL binary)");

        PTL::Data data(inputFile);
        data.printInfo();

        data.readLabelFile(labelFile);
        data.printInfo();

        data.serialize(outputFile);
    }
};
