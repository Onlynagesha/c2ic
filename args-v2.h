//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ARGS_V2_H
#define DAWNSEEKER_GRAPH_ARGS_V2_H

#include "argparse/argparse.hpp"
#include "args/args.h"

struct ProgramArgs {
    args::CIAnyArgSet argSet;
    argparse::ArgumentParser argParser;
};

ProgramArgs prepareProgramArgs(int argc, char** argv);

void prepareDerivedArgs(args::CIAnyArgSet& delta, std::size_t kappa);

#endif //DAWNSEEKER_GRAPH_ARGS_V2_H
