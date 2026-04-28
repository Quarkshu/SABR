#pragma once

#include "models/bundle.hpp"
#include "models/route.hpp"

#include <cstddef>
#include <vector>

enum class RedundancyMode {
    NONE,
    SINGLE_BACKUP,
    MULTI_BACKUP,
    TRUNK_ONLY,
};

struct RedundancyConfig {
    RedundancyMode mode = RedundancyMode::NONE;
    std::size_t max_extra_copies = 1;
    double risk_threshold = 0.65;
    double min_diversity_score = 0.50;
    double min_capacity_reserve_ratio = 0.15;
    double min_ttl_slack = 0.0;
    bool enable_for_unicast = true;
    bool enable_for_multicast_trunk = true;
};

struct RedundancyRouteAssessment {
    CandidateRoute candidate;
    double diversity_score = 0.0;
    double delivery_risk = 0.0;
    double capacity_reserve_ratio = 0.0;
    double ttl_slack = 0.0;
};

class RouteDiversityAnalyzer {
public:
    double score(const Route& primary,
                 const Route& alternate) const;
};

class DeliveryRiskEstimator {
public:
    double estimate(const Bundle& bundle,
                    const CandidateRoute& candidate,
                    TimePoint current_time) const;
    double estimate(const Bundle& bundle,
                    const Route& route,
                    TimePoint current_time) const;
    double capacity_reserve_ratio(const Bundle& bundle,
                                  const Route& route) const;
    double ttl_slack(const Bundle& bundle,
                     const Route& route) const;
};

class RedundancyManager {
public:
    bool should_consider(const Bundle& bundle,
                         bool is_multicast_trunk,
                         const RedundancyConfig& config) const;
    std::vector<CandidateRoute> select_backups(const Bundle& bundle,
                                               const std::vector<CandidateRoute>& candidates,
                                               const CandidateRoute& primary,
                                               TimePoint current_time,
                                               const RedundancyConfig& config,
                                               bool is_multicast_trunk = false) const;

private:
    std::size_t max_backup_count(const RedundancyConfig& config,
                                 bool is_multicast_trunk) const;

    RouteDiversityAnalyzer diversity_analyzer_;
    DeliveryRiskEstimator risk_estimator_;
};