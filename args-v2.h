//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ARGS_V2_H
#define DAWNSEEKER_GRAPH_ARGS_V2_H

#include "argparse/argparse.hpp"
#include "args/args.h"

using ProgramArgs = args::CIAnyArgSet;

struct AlgorithmArgs {
    std::size_t n;
    std::size_t k;

    std::size_t sampleLimit;
    std::size_t sampleLimitSA;
};

args::CIAnyArgSet prepareProgramArgs(int argc, char** argv);

void prepareDerivedArgs(args::CIAnyArgSet& delta, std::size_t n);

#endif //DAWNSEEKER_GRAPH_ARGS_V2_H
