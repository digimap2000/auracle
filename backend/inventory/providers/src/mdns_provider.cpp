#include "../mdns_provider.hpp"

#include <dts/log.hpp>

#include <chrono>
#include <format>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace auracle::inventory {

namespace {

constexpr std::string_view log_tag = "mdns-provider";
constexpr int ping_interval_ms = 500;
constexpr int ping_timeout_ms = 500;
constexpr int ping_failures_before_gone = 3;

[[nodiscard]] std::uint16_t icmp_checksum(const void* data, std::size_t len) {
    auto ptr = static_cast<const std::uint16_t*>(data);
    std::uint32_t sum = 0;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *reinterpret_cast<const std::uint8_t*>(ptr);
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<std::uint16_t>(~sum);
}

// Non-privileged ICMP echo (macOS SOCK_DGRAM + IPPROTO_ICMP).
[[nodiscard]] bool icmp_ping(const std::string& ip, int timeout_ms) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (fd < 0) {
        return false;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        return false;
    }

    struct icmp echo{};
    echo.icmp_type = ICMP_ECHO;
    echo.icmp_code = 0;
    echo.icmp_id = static_cast<std::uint16_t>(::getpid() & 0xFFFF);
    echo.icmp_seq = 0;
    echo.icmp_cksum = 0;
    echo.icmp_cksum = icmp_checksum(&echo, sizeof(echo));

    auto n = ::sendto(fd, &echo, sizeof(echo), 0,
                      reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (n < 0) {
        ::close(fd);
        return false;
    }

    struct pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
    bool got_reply = false;
    if (::poll(&pfd, 1, timeout_ms) > 0) {
        char buf[128];
        got_reply = ::recv(fd, buf, sizeof(buf), 0) > 0;
    }

    ::close(fd);
    return got_reply;
}

} // namespace

MdnsCandidateProvider::MdnsCandidateProvider(
    dts::mdns::monitor& monitor,
    InventoryRegistry& registry)
    : monitor_(monitor)
    , registry_(registry) {}

void MdnsCandidateProvider::start() {
    arrived_conn_ = monitor_.on_arrived_scoped(
        [this](const dts::mdns::service_info& info) { on_arrived(info); });
    departed_conn_ = monitor_.on_departed_scoped(
        [this](const dts::mdns::service_info& info) { on_departed(info); });

    ping_thread_ = std::jthread([this](std::stop_token stop) {
        ping_loop(stop);
    });
}

void MdnsCandidateProvider::stop() {
    arrived_conn_.reset();
    departed_conn_.reset();
    if (ping_thread_.joinable()) {
        ping_thread_.request_stop();
        ping_thread_.join();
    }
}

void MdnsCandidateProvider::on_arrived(const dts::mdns::service_info& info) {
    auto candidate = make_candidate(info);
    registry_.upsert_candidate(candidate);

    // Pick the first IPv4 address for ping liveness monitoring.
    std::string ip;
    for (const auto& addr : info.addresses) {
        if (addr.find(':') == std::string::npos) {
            ip = addr;
            break;
        }
    }

    if (ip.empty()) {
        dts::log::debug(log_tag, "no IPv4 address for {}, cannot monitor liveness",
            info.instance);
        return;
    }

    std::lock_guard lock(ping_mutex_);
    ping_targets_[info.stable_id] = PingTarget{
        .ip = std::move(ip),
        .candidate = std::move(candidate),
        .consecutive_failures = 0,
        .gone = false,
    };
}

void MdnsCandidateProvider::on_departed(const dts::mdns::service_info& info) {
    {
        std::lock_guard lock(ping_mutex_);
        ping_targets_.erase(info.stable_id);
    }
    registry_.mark_candidate_gone(make_candidate_id(info));
}

void MdnsCandidateProvider::ping_loop(std::stop_token stop) {
    while (!stop.stop_requested()) {
        for (int i = 0; i < ping_interval_ms / 100 && !stop.stop_requested(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (stop.stop_requested()) {
            break;
        }

        // Snapshot targets, then ping without holding the lock.
        std::vector<std::pair<std::string, PingTarget>> snapshot;
        {
            std::lock_guard lock(ping_mutex_);
            snapshot.reserve(ping_targets_.size());
            for (const auto& [key, target] : ping_targets_) {
                snapshot.emplace_back(key, target);
            }
        }

        for (const auto& [stable_id, target] : snapshot) {
            if (stop.stop_requested()) {
                break;
            }

            bool alive = icmp_ping(target.ip, ping_timeout_ms);

            std::lock_guard lock(ping_mutex_);
            auto it = ping_targets_.find(stable_id);
            if (it == ping_targets_.end()) {
                continue; // Departed while we were pinging.
            }

            if (alive) {
                if (it->second.gone) {
                    // Device came back — mDNS may not have noticed the gap.
                    dts::log::info(log_tag, "ping recovered for {} ({})",
                        target.ip, target.candidate.id.value);
                    it->second.gone = false;
                    auto candidate = it->second.candidate;
                    candidate.last_seen = std::chrono::system_clock::now();
                    registry_.upsert_candidate(candidate);
                }
                it->second.consecutive_failures = 0;
            } else {
                it->second.consecutive_failures++;
                if (!it->second.gone &&
                    it->second.consecutive_failures >= ping_failures_before_gone) {
                    dts::log::info(log_tag, "ping failed {} times for {} ({}), marking gone",
                        it->second.consecutive_failures, target.ip,
                        target.candidate.id.value);
                    it->second.gone = true;
                    registry_.mark_candidate_gone(it->second.candidate.id);
                }
            }
        }
    }
}

CandidateId MdnsCandidateProvider::make_candidate_id(const dts::mdns::service_info& info) {
    return CandidateId{std::format("mdns:{}", info.stable_id)};
}

HardwareCandidate MdnsCandidateProvider::make_candidate(const dts::mdns::service_info& info) {
    const auto now = std::chrono::system_clock::now();

    MdnsDetails details;
    details.service = info.service_type;
    details.instance = info.instance;
    details.host = info.host;
    details.port = info.port;

    return HardwareCandidate{
        .id = make_candidate_id(info),
        .transport = Transport::Mdns,
        .present = true,
        .first_seen = now,
        .last_seen = now,
        .details = std::move(details),
    };
}

} // namespace auracle::inventory
