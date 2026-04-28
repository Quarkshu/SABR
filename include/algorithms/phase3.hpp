#pragma once

#include "models/route.hpp"

#include <optional>
#include <string>
#include <vector>

enum class RoutingAction {
    DELIVER_LOCAL,
    FORWARD_ONE,
    FORWARD_FLOOD,
    ROUTE_FAIL,
};

struct RoutingDecision {
    RoutingAction action = RoutingAction::ROUTE_FAIL;
    std::optional<CandidateRoute> primary;
    std::vector<CandidateRoute> flood_routes;
    std::string failure_reason;
};

class Phase3Selector {
public:
    std::optional<CandidateRoute> select_best(const std::vector<CandidateRoute>& candidates) const;
    std::vector<CandidateRoute> select_flood_set(const std::vector<CandidateRoute>& candidates) const;
};