//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ARGS_V2_H
#define DAWNSEEKER_GRAPH_ARGS_V2_H

#include "argparse/argparse.hpp"
#include "args/args.h"

using AlgorithmArgs = args::CIAnyArgSet;

args::CIAnyArgSet prepareProgramArgs(int argc, char** argv);

void prepareDerivedArgs(args::CIAnyArgSet& delta, std::size_t n);

#endif //DAWNSEEKER_GRAPH_ARGS_V2_H
