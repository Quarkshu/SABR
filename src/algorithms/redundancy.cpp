#include "algorithms/redundancy.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

namespace {

constexpr double kEpsilon = 1e-9;

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

int priority_index(Priority priority) {
    return static_cast<int>(priority);
}

std::vector<ContactId> route_signature(const Route& route) {
    std::vector<ContactId> ids;
    ids.reserve(route.legs.size());
    for (const RouteLeg& leg : route.legs) {
        if (leg.contact != nullptr) {
            ids.push_back(leg.contact->contact_id);
        }
    }
    return ids;
}

bool same_route(const Route& lhs,
                const Route& rhs) {
    return route_signature(lhs) == route_signature(rhs);
}

bool assessment_better(const RedundancyRouteAssessment& lhs,
                       const RedundancyRouteAssessment& rhs) {
    if (std::abs(lhs.diversity_score - rhs.diversity_score) > kEpsilon) {
        return lhs.diversity_score > rhs.diversity_score;
    }
    if (std::abs(lhs.delivery_risk - rhs.delivery_risk) > kEpsilon) {
        return lhs.delivery_risk < rhs.delivery_risk;
    }
    if (std::abs(lhs.ttl_slack - rhs.ttl_slack) > kEpsilon) {
        return lhs.ttl_slack > rhs.ttl_slack;
    }
    return lhs.candidate.route.pbat < rhs.candidate.route.pbat;
}

}  // namespace

double RouteDiversityAnalyzer::score(const Route& primary,
                                     const Route& alternate) const {
    const std::vector<ContactId> primary_contacts = route_signature(primary);
    const std::vector<ContactId> alternate_contacts = route_signature(alternate);
    if (primary_contacts.empty() && alternate_contacts.empty()) {
        return 0.0;
    }

    const std::set<ContactId> primary_set(primary_contacts.begin(), primary_contacts.end());
    const std::set<ContactId> alternate_set(alternate_contacts.begin(), alternate_contacts.end());

    std::size_t overlap_count = 0;
    for (ContactId contact_id : primary_set) {
        if (alternate_set.count(contact_id) != 0u) {
            ++overlap_count;
        }
    }

    const std::size_t union_count = primary_set.size() + alternate_set.size() - overlap_count;
    const double contact_distance = union_count == 0
        ? 0.0
        : 1.0 - static_cast<double>(overlap_count) / static_cast<double>(union_count);
    const double entry_diversity = primary.entry_node == alternate.entry_node ? 0.0 : 1.0;
    return clamp01(0.6 * contact_distance + 0.4 * entry_diversity);
}

double DeliveryRiskEstimator::estimate(const Bundle& bundle,
                                       const CandidateRoute& candidate,
                                       TimePoint current_time) const {
    return estimate(bundle, candidate.route, current_time);
}

double DeliveryRiskEstimator::estimate(const Bundle& bundle,
                                       const Route& route,
                                       TimePoint) const {
    const double hop_component = std::min(0.30 * static_cast<double>(route.hop_count()), 0.75);
    const double slack = ttl_slack(bundle, route);
    const double ttl_component = 0.25 * (1.0 - clamp01(slack / std::max(bundle.ttl, 1.0)));
    const double reserve_component = 0.35 * (1.0 - clamp01(capacity_reserve_ratio(bundle, route)));
    return clamp01(hop_component + ttl_component + reserve_component);
}

double DeliveryRiskEstimator::capacity_reserve_ratio(const Bundle& bundle,
                                                     const Route& route) const {
    if (route.local_delivery || route.legs.empty()) {
        return 1.0;
    }

    const Volume evc = std::max(bundle.evc(), 1.0);
    double min_ratio = 1.0;
    for (const RouteLeg& leg : route.legs) {
        if (leg.contact == nullptr) {
            continue;
        }

        const Volume available = leg.contact->mtv[priority_index(bundle.priority)];
        const double reserve_ratio = (available - evc) / evc;
        min_ratio = std::min(min_ratio, reserve_ratio);
    }
    return std::max(0.0, min_ratio);
}

double DeliveryRiskEstimator::ttl_slack(const Bundle& bundle,
                                        const Route& route) const {
    return bundle.expiration_time() - route.best_case_delivery_time;
}

bool RedundancyManager::should_consider(const Bundle& bundle,
                                        bool is_multicast_trunk,
                                        const RedundancyConfig& config) const {
    if (config.mode == RedundancyMode::NONE) {
        return false;
    }
    if (bundle.is_critical) {
        return false;
    }
    if (bundle.redundancy_applied) {
        return false;
    }
    if (bundle.is_multicast) {
        return is_multicast_trunk && config.enable_for_multicast_trunk;
    }
    return config.enable_for_unicast && config.mode != RedundancyMode::TRUNK_ONLY;
}

std::vector<CandidateRoute> RedundancyManager::select_backups(const Bundle& bundle,
                                                              const std::vector<CandidateRoute>& candidates,
                                                              const CandidateRoute& primary,
                                                              TimePoint current_time,
                                                              const RedundancyConfig& config,
                                                              bool is_multicast_trunk) const {
    if (!should_consider(bundle, is_multicast_trunk, config)) {
        return {};
    }

    const std::size_t max_backups = max_backup_count(config, is_multicast_trunk);
    if (max_backups == 0u) {
        return {};
    }

    const double primary_risk = risk_estimator_.estimate(bundle, primary, current_time);
    if (primary_risk + kEpsilon < config.risk_threshold) {
        return {};
    }

    std::vector<RedundancyRouteAssessment> assessments;
    assessments.reserve(candidates.size());
    for (const CandidateRoute& candidate : candidates) {
        if (!candidate.valid || same_route(candidate.route, primary.route)) {
            continue;
        }

        RedundancyRouteAssessment assessment;
        assessment.candidate = candidate;
        assessment.diversity_score = diversity_analyzer_.score(primary.route, candidate.route);
        assessment.delivery_risk = risk_estimator_.estimate(bundle, candidate, current_time);
        assessment.capacity_reserve_ratio = risk_estimator_.capacity_reserve_ratio(bundle, candidate.route);
        assessment.ttl_slack = risk_estimator_.ttl_slack(bundle, candidate.route);

        if (assessment.diversity_score + kEpsilon < config.min_diversity_score) {
            continue;
        }
        if (assessment.capacity_reserve_ratio + kEpsilon < config.min_capacity_reserve_ratio) {
            continue;
        }
        if (assessment.ttl_slack + kEpsilon < config.min_ttl_slack) {
            continue;
        }

        assessments.push_back(std::move(assessment));
    }

    std::sort(assessments.begin(), assessments.end(), assessment_better);

    std::vector<CandidateRoute> selected;
    selected.reserve(std::min(max_backups, assessments.size()));
    std::set<NodeId> used_entries = {primary.route.entry_node};
    for (const RedundancyRouteAssessment& assessment : assessments) {
        if (used_entries.count(assessment.candidate.route.entry_node) != 0u) {
            continue;
        }
        selected.push_back(assessment.candidate);
        used_entries.insert(assessment.candidate.route.entry_node);
        if (selected.size() >= max_backups) {
            break;
        }
    }

    return selected;
}

std::size_t RedundancyManager::max_backup_count(const RedundancyConfig& config,
                                                bool is_multicast_trunk) const {
    if (config.mode == RedundancyMode::NONE) {
        return 0u;
    }
    if (config.mode == RedundancyMode::TRUNK_ONLY && !is_multicast_trunk) {
        return 0u;
    }
    if (config.mode == RedundancyMode::SINGLE_BACKUP || config.mode == RedundancyMode::TRUNK_ONLY) {
        return std::min<std::size_t>(1u, config.max_extra_copies);
    }
    return config.max_extra_copies;
}