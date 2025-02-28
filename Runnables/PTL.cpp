#include "../Helpers/Console/CommandLineParser.h"
#include "../Shell/Shell.h"
#include "Commands/PTLPreprocessing.h"
#include "Commands/QueryBenchmark.h"

using namespace Shell;

int main(int argc, char** argv) {
    CommandLineParser clp(argc, argv);
    pinThreadToCoreId(clp.value<int>("core", 1));
    checkAsserts();
    ::Shell::Shell shell;

    new TEToPTL(shell);
    new LoadLabelFile(shell);
    new RunPTLQueries(shell);

    shell.run();
    return 0;
}
