#include "render.hpp"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

#include <ctime>
#include <format>
#include <iostream>

namespace auracle::cli {

namespace {

[[nodiscard]] std::string format_timestamp(int64_t timestamp_ms) {
    auto seconds = static_cast<time_t>(timestamp_ms / 1000);
    auto millis = static_cast<int>(timestamp_ms % 1000);
    struct tm local_tm{};
    localtime_r(&seconds, &local_tm);
    return std::format("{:02d}:{:02d}:{:02d}.{:03d}",
        local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec, millis);
}

[[nodiscard]] std::string candidate_detail(const proto::HardwareCandidate& c) {
    if (c.has_serial()) {
        return c.serial().port();
    }
    if (c.has_mdns()) {
        return std::format("{}:{}", c.mdns().host(), c.mdns().port());
    }
    if (c.has_host_bluetooth()) {
        const auto& bt = c.host_bluetooth();
        return std::format("{} [{}] {}", bt.name(), bt.address(),
            bt.powered() ? "powered" : "off");
    }
    return {};
}

} // namespace

// ---------------------------------------------------------------------------
// PrettyRenderer
// ---------------------------------------------------------------------------

void PrettyRenderer::render(const proto::InventoryEvent& event) {
    const auto ts = format_timestamp(event.timestamp_ms());

    switch (event.payload_case()) {
    case proto::InventoryEvent::kCandidateAdded: {
        const auto& c = event.candidate_added().candidate();
        std::cout << std::format("{}  candidate+  {}  {}\n", ts, c.id(), candidate_detail(c));
        break;
    }
    case proto::InventoryEvent::kCandidateUpdated: {
        const auto& c = event.candidate_updated().candidate();
        std::cout << std::format("{}  candidate~  {}\n", ts, c.id());
        break;
    }
    case proto::InventoryEvent::kCandidateGone:
        std::cout << std::format("{}  candidate-  {}\n", ts, event.candidate_gone().id());
        break;
    case proto::InventoryEvent::kUnitAdded:
        std::cout << std::format("{}  unit+       {}\n", ts, event.unit_added().unit().id());
        break;
    case proto::InventoryEvent::kUnitUpdated:
        std::cout << std::format("{}  unit~       {}\n", ts, event.unit_updated().unit().id());
        break;
    case proto::InventoryEvent::kUnitRemoved:
        std::cout << std::format("{}  unit-       {}\n", ts, event.unit_removed().id());
        break;
    case proto::InventoryEvent::kUnitOnline:
        std::cout << std::format("{}  unit-online {}\n", ts, event.unit_online().id());
        break;
    case proto::InventoryEvent::kUnitOffline:
        std::cout << std::format("{}  unit-offline {}\n", ts, event.unit_offline().id());
        break;
    case proto::InventoryEvent::kClaimChanged: {
        const auto& cl = event.claim_changed();
        if (cl.has_lease()) {
            std::cout << std::format("{}  claim       {}  client={}\n",
                ts, cl.id(), cl.lease().client_id());
        } else {
            std::cout << std::format("{}  claim       {}  released\n", ts, cl.id());
        }
        break;
    }
    default:
        break;
    }

    std::cout.flush();
}

// ---------------------------------------------------------------------------
// JsonRenderer
// ---------------------------------------------------------------------------

void JsonRenderer::render(const proto::InventoryEvent& event) {
    google::protobuf::util::JsonPrintOptions options;
    options.always_print_fields_with_no_presence = true;
    std::string json;
    auto status = google::protobuf::util::MessageToJsonString(event, &json, options);
    if (status.ok()) {
        std::cout << json << '\n';
        std::cout.flush();
    }
}

// ---------------------------------------------------------------------------
// PrototextRenderer
// ---------------------------------------------------------------------------

void PrototextRenderer::render(const proto::InventoryEvent& event) {
    std::string text;
    google::protobuf::TextFormat::PrintToString(event, &text);
    std::cout << text;
    std::cout.flush();
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<IRenderer> make_renderer(OutputFormat format) {
    switch (format) {
    case OutputFormat::Pretty:    return std::make_unique<PrettyRenderer>();
    case OutputFormat::Json:      return std::make_unique<JsonRenderer>();
    case OutputFormat::Prototext: return std::make_unique<PrototextRenderer>();
    }
    return std::make_unique<PrettyRenderer>();
}

} // namespace auracle::cli
