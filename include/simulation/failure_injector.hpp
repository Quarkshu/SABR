#pragma once

#include "simulation/event.hpp"

#include <cstdint>
#include <memory>
#include <vector>

enum class FailureRuleMode {
    CONTACT_SET,
    PROBABILISTIC_CONTACTS,
    PLAN_REPLACEMENT,
};

struct FailureRule {
    FailureRuleMode mode = FailureRuleMode::CONTACT_SET;
    TimePoint trigger_time = 0.0;
    std::vector<ContactId> contact_ids;
    double probability = 0.0;
    std::shared_ptr<ContactPlan> replacement_plan;
    std::string label;
};

class FailureInjector {
public:
    explicit FailureInjector(std::uint32_t seed = 0,
                             std::vector<FailureRule> rules = {});

    void set_seed(std::uint32_t seed) noexcept { seed_ = seed; }
    std::uint32_t seed() const noexcept { return seed_; }

    void set_rules(std::vector<FailureRule> rules);
    const std::vector<FailureRule>& rules() const noexcept { return rules_; }

    std::vector<PlanUpdateData> materialize(const ContactPlan& base_plan,
                                            TimePoint window_start,
                                            TimePoint window_end) const;

private:
    std::vector<ContactId> active_candidates(const ContactPlan& base_plan,
                                             const FailureRule& rule) const;

private:
    std::uint32_t seed_ = 0;
    std::vector<FailureRule> rules_;
};