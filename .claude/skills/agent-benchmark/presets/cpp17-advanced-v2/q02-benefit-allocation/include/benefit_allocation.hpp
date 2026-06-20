#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class ClaimCode {
    granted,
    duplicate,
    invalid,
    campaign_unknown,
    outside_window,
    customer_limit,
    campaign_exhausted,
    batch_conflict,
    batch_duplicate_claim,
};

enum class BatchCode {
    applied,
    duplicate,
    rejected,
};

struct CampaignSpec {
    std::string campaign_id;
    std::int64_t budget_minor = 0;
    std::int64_t per_customer_limit_minor = 0;
    std::uint64_t starts_at = 0;
    std::uint64_t ends_at = 0;
};

struct BenefitClaim {
    std::string_view campaign_id;
    std::string_view customer_id;
    std::string_view claim_id;
    std::int64_t amount_minor = 0;
};

struct ClaimReply {
    ClaimCode code = ClaimCode::invalid;
    std::int64_t granted_minor = 0;
    std::int64_t campaign_remaining_minor = 0;
    std::int64_t customer_used_minor = 0;
};

struct ClaimBatch {
    std::string_view batch_id;
    std::vector<BenefitClaim> claims;
};

struct BatchReply {
    BatchCode code = BatchCode::rejected;
    std::vector<ClaimReply> replies;
};

struct CampaignView {
    std::string campaign_id;
    std::int64_t campaign_remaining_minor = 0;
    std::size_t granted_claim_count = 0;
};

struct BenefitAuditRecord {
    std::string campaign_id;
    std::string customer_id;
    std::string claim_id;
    std::int64_t amount_minor = 0;
    ClaimReply reply;
};

class BenefitAllocator {
public:
    using AuditSink = std::function<void(const BenefitAuditRecord&)>;

    explicit BenefitAllocator(std::vector<CampaignSpec> campaigns,
                              AuditSink audit_sink = {});

    ClaimReply claim(const BenefitClaim& claim, std::uint64_t now);
    BatchReply claim_batch(const ClaimBatch& batch, std::uint64_t now);
    std::optional<CampaignView> view(std::string_view campaign_id) const;
    void set_audit_sink(AuditSink audit_sink);

private:
    struct State;
    std::shared_ptr<State> state_;
};
