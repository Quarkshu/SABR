#include "simulation/failure_injector.hpp"

#include <algorithm>
#include <random>

namespace {

void normalize_contact_ids(std::vector<ContactId>& contact_ids) {
    std::sort(contact_ids.begin(), contact_ids.end());
    contact_ids.erase(std::unique(contact_ids.begin(), contact_ids.end()), contact_ids.end());
}

}  // namespace

FailureInjector::FailureInjector(std::uint32_t seed,
                                 std::vector<FailureRule> rules)
    : seed_(seed),
      rules_(std::move(rules)) {}

void FailureInjector::set_rules(std::vector<FailureRule> rules) {
    rules_ = std::move(rules);
}

std::vector<PlanUpdateData> FailureInjector::materialize(const ContactPlan& base_plan,
                                                         TimePoint window_start,
                                                         TimePoint window_end) const {
    std::vector<PlanUpdateData> updates;
    std::mt19937 generator(seed_);

    for (const FailureRule& rule : rules_) {
        if (rule.trigger_time < window_start || rule.trigger_time > window_end) {
            continue;
        }

        PlanUpdateData update;
        update.trigger_time = rule.trigger_time;
        update.label = rule.label;

        switch (rule.mode) {
        case FailureRuleMode::CONTACT_SET:
            update.removed_contact_ids = rule.contact_ids;
            normalize_contact_ids(update.removed_contact_ids);
            break;
        case FailureRuleMode::PROBABILISTIC_CONTACTS: {
            std::vector<ContactId> candidates = active_candidates(base_plan, rule);
            std::bernoulli_distribution distribution(rule.probability);
            for (ContactId contact_id : candidates) {
                if (distribution(generator)) {
                    update.removed_contact_ids.push_back(contact_id);
                }
            }
            normalize_contact_ids(update.removed_contact_ids);
            break;
        }
        case FailureRuleMode::PLAN_REPLACEMENT:
            update.replacement_plan = rule.replacement_plan;
            break;
        }

        if (update.removed_contact_ids.empty() && update.replacement_plan == nullptr) {
            continue;
        }

        updates.push_back(std::move(update));
    }

    std::stable_sort(updates.begin(), updates.end(),
        [](const PlanUpdateData& lhs, const PlanUpdateData& rhs) {
            if (lhs.trigger_time != rhs.trigger_time) {
                return lhs.trigger_time < rhs.trigger_time;
            }
            return lhs.label < rhs.label;
        });

    return updates;
}

std::vector<ContactId> FailureInjector::active_candidates(const ContactPlan& base_plan,
                                                          const FailureRule& rule) const {
    std::vector<ContactId> candidates;

    if (!rule.contact_ids.empty()) {
        candidates = rule.contact_ids;
        normalize_contact_ids(candidates);

        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                [&base_plan, &rule](ContactId contact_id) {
                    const Contact* contact = base_plan.get_contact(contact_id);
                    return contact == nullptr || contact->end_time <= rule.trigger_time;
                }),
            candidates.end());
        return candidates;
    }

    for (const Contact& contact : base_plan.contacts()) {
        if (contact.end_time > rule.trigger_time) {
            candidates.push_back(contact.contact_id);
        }
    }

    normalize_contact_ids(candidates);
    return candidates;
}