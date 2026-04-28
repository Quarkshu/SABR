#pragma once

#include "models/node.hpp"

class Phase1 {
public:
    struct Config {
        int k_paths = 3;
        TimePoint owlt_margin = 0.0;
        bool one_route_per_neighbor = false;
    };

    static const std::vector<Route>& compute(Node& node,
                                             NodeId destination,
                                             TimePoint current_time,
                               const Config& config);

    static const std::vector<Route>& recompute_more(Node& node,
                                                    NodeId destination,
                                                    TimePoint current_time,
                                   const Config& config,
                                                    int additional_paths = 1);

private:
    static std::vector<Route> compute_routes(Node& node,
                                             NodeId destination,
                                             TimePoint current_time,
                                             const Config& config,
                                             int requested_paths);
};