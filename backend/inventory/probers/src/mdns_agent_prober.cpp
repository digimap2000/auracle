#include "../mdns_agent_prober.hpp"

#include <dts/log.hpp>

#include <variant>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace auracle::inventory {

namespace {

constexpr std::string_view log_tag = "mdns-prober";
constexpr int connect_timeout_ms = 3000;

// Try to TCP-connect to host:port with a timeout.
// Returns true if the port accepted the connection.
[[nodiscard]] bool tcp_reachable(const std::string& host, std::uint16_t port) {
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const auto port_str = std::to_string(port);
    struct addrinfo* res{nullptr};
    if (::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
        return false;
    }

    bool connected = false;
    for (auto* rp = res; rp != nullptr; rp = rp->ai_next) {
        int fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) {
            continue;
        }

        int flags = ::fcntl(fd, F_GETFL, 0);
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = ::connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (rc == 0) {
            connected = true;
        } else if (errno == EINPROGRESS) {
            struct pollfd pfd{.fd = fd, .events = POLLOUT, .revents = 0};
            rc = ::poll(&pfd, 1, connect_timeout_ms);
            if (rc > 0 && (pfd.revents & POLLOUT) != 0) {
                int err = 0;
                socklen_t len = sizeof(err);
                ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
                connected = (err == 0);
            }
        }

        ::close(fd);
        if (connected) {
            break;
        }
    }

    ::freeaddrinfo(res);
    return connected;
}

} // namespace

ProbeResult MdnsAgentProber::probe(const HardwareCandidate& candidate) {
    if (candidate.transport != Transport::Mdns) {
        return {.outcome = ProbeOutcome::Unsupported};
    }

    const auto* details = std::get_if<MdnsDetails>(&candidate.details);
    if (details == nullptr) {
        return {.outcome = ProbeOutcome::Unsupported};
    }

    if (details->host.empty() || details->port == 0) {
        return {.outcome = ProbeOutcome::TransientFailure,
                .kind = HardwareKind::Unknown};
    }

    if (!tcp_reachable(details->host, details->port)) {
        dts::log::debug(log_tag, "connect failed: {}:{}", details->host, details->port);
        return {.outcome = ProbeOutcome::TransientFailure,
                .kind = HardwareKind::Unknown};
    }

    dts::log::info(log_tag, "port open: {}:{} instance={}",
        details->host, details->port, details->instance);

    return {
        .outcome = ProbeOutcome::Supported,
        .kind = HardwareKind::Nrf5340AudioDK,
        .identity = {
            .vendor = "Focal Naim",
            .product = details->instance,
            .serial = details->instance,
        },
        .present = true,
    };
}

} // namespace auracle::inventory
