#include "benefit_allocation.hpp"

#include <mutex>
#include <utility>

struct BenefitAllocator::State {
    explicit State(std::vector<CampaignSpec> initial_campaigns, AuditSink initial_sink)
        : campaigns(std::move(initial_campaigns)), audit_sink(std::move(initial_sink)) {}

    mutable std::mutex mutex;
    std::vector<CampaignSpec> campaigns;
    AuditSink audit_sink;
};

BenefitAllocator::BenefitAllocator(std::vector<CampaignSpec> campaigns,
                                   AuditSink audit_sink)
    : state_(std::make_shared<State>(std::move(campaigns), std::move(audit_sink))) {}

ClaimReply BenefitAllocator::claim(const BenefitClaim&, std::uint64_t) {
    return {};
}

BatchReply BenefitAllocator::claim_batch(const ClaimBatch&, std::uint64_t) {
    return {};
}

std::optional<CampaignView> BenefitAllocator::view(
    std::string_view campaign_id) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    for (const CampaignSpec& campaign : state_->campaigns) {
        if (campaign.campaign_id == campaign_id) {
            return CampaignView{campaign.campaign_id, campaign.budget_minor, 0};
        }
    }
    return std::nullopt;
}

void BenefitAllocator::set_audit_sink(AuditSink audit_sink) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->audit_sink = std::move(audit_sink);
}
