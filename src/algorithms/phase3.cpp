#include "algorithms/phase3.hpp"

#include <algorithm>
#include <map>
#include <vector>

namespace {

constexpr double kEpsilon = 1e-9;

bool less_than(TimePoint lhs, TimePoint rhs) {
    return lhs + kEpsilon < rhs;
}

bool greater_than(TimePoint lhs, TimePoint rhs) {
    return lhs > rhs + kEpsilon;
}

bool candidate_less(const CandidateRoute& lhs,
                    const CandidateRoute& rhs) {
    if (less_than(lhs.route.pbat, rhs.route.pbat)) {
        return true;
    }
    if (less_than(rhs.route.pbat, lhs.route.pbat)) {
        return false;
    }

    if (lhs.route.hop_count() != rhs.route.hop_count()) {
        return lhs.route.hop_count() < rhs.route.hop_count();
    }

    if (greater_than(lhs.route.termination_time, rhs.route.termination_time)) {
        return true;
    }
    if (greater_than(rhs.route.termination_time, lhs.route.termination_time)) {
        return false;
    }

    if (lhs.route.entry_node != rhs.route.entry_node) {
        return lhs.route.entry_node < rhs.route.entry_node;
    }

    return lhs.reject_reason < rhs.reject_reason;
}

std::vector<const CandidateRoute*> preferred_candidates(
    const std::vector<const CandidateRoute*>& candidates) {
    std::vector<const CandidateRoute*> non_looping;
    for (const CandidateRoute* candidate : candidates) {
        if (candidate != nullptr && !candidate->route.possibly_looping) {
            non_looping.push_back(candidate);
        }
    }
    return non_looping.empty() ? candidates : non_looping;
}

std::optional<CandidateRoute> select_best_from_group(
    const std::vector<const CandidateRoute*>& candidates) {
    if (candidates.empty()) {
        return std::nullopt;
    }

    const auto best = std::min_element(
        candidates.begin(),
        candidates.end(),
        [](const CandidateRoute* lhs, const CandidateRoute* rhs) {
            return candidate_less(*lhs, *rhs);
        });
    return best == candidates.end() ? std::nullopt : std::optional<CandidateRoute>{**best};
}

}  // namespace

std::optional<CandidateRoute> Phase3Selector::select_best(
    const std::vector<CandidateRoute>& candidates) const {
    std::vector<const CandidateRoute*> valid_candidates;
    valid_candidates.reserve(candidates.size());
    for (const CandidateRoute& candidate : candidates) {
        if (candidate.valid) {
            valid_candidates.push_back(&candidate);
        }
    }

    return select_best_from_group(preferred_candidates(valid_candidates));
}

std::vector<CandidateRoute> Phase3Selector::select_flood_set(
    const std::vector<CandidateRoute>& candidates) const {
    std::map<NodeId, std::vector<const CandidateRoute*>> grouped_candidates;
    for (const CandidateRoute& candidate : candidates) {
        if (!candidate.valid) {
            continue;
        }
        grouped_candidates[candidate.route.entry_node].push_back(&candidate);
    }

    std::vector<CandidateRoute> selected_routes;
    selected_routes.reserve(grouped_candidates.size());
    for (const auto& entry : grouped_candidates) {
        std::optional<CandidateRoute> selected =
            select_best_from_group(preferred_candidates(entry.second));
        if (selected.has_value()) {
            selected_routes.push_back(std::move(*selected));
        }
    }
    return selected_routes;
}