#pragma once

#include "algorithms/contact_graph.hpp"
#include "algorithms/dijkstra.hpp"

#include <vector>

class YenKSP {
public:
    std::vector<Route> compute(const ContactGraph& graph,
                               const ContactPlan& plan,
                               TimePoint current_time,
                               TimePoint owlt_margin,
                               int k) const;

private:
    DijkstraRouter router_;
};