#ifndef DAWNSEEKER_GLOBAL_H
#define DAWNSEEKER_GLOBAL_H

#include <chrono>
#include <filesystem>
#include <numbers>
#include <ranges>
#include <version>

// Abbreviations for std sub-namespaces
namespace ch = std::chrono;
namespace fs = std::filesystem;
namespace ns = std::numbers; 
namespace rs = std::ranges;
namespace vs = std::views; 

// Enables all the literals in namespace std
using namespace std::literals;

// format is disabled

// For compatibility with old code
#include "graph/graph.h"

using namespace utils;

#endif //DAWNSEEKER_GLOBAL_H