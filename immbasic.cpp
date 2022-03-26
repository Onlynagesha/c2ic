//
// Created by dawnseeker on 2022/3/26.
//

#include "immbasic.h"
#include <compare>

/*
* Compares two node states by priority
* A > B means A has higher priority than B
*/
std::strong_ordering operator <=> (NodeState A, NodeState B) {
    return nodeStatePriority[static_cast<int>(A)] <=> nodeStatePriority[static_cast<int>(B)];
}

/*
* Compares two link states as order Boosted > Active > Blocked
*/
std::strong_ordering operator <=> (LinkState A, LinkState B) {
    return static_cast<int>(A) <=> static_cast<int>(B);
}