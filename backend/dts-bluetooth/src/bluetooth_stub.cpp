#include <dts/bluetooth.hpp>

namespace dts::bluetooth {

std::vector<adapter_info> enumerate_adapters() {
    return {};
}

struct monitor::impl {};

monitor::monitor() = default;
monitor::~monitor() noexcept = default;

monitor::monitor(monitor&&) noexcept = default;
monitor& monitor::operator=(monitor&&) noexcept = default;

void monitor::start(start_options /*opts*/) {}
void monitor::stop() noexcept {}
bool monitor::running() const noexcept { return false; }

struct scanner::impl {};

scanner::scanner() = default;
scanner::~scanner() noexcept = default;

scanner::scanner(scanner&&) noexcept = default;
scanner& scanner::operator=(scanner&&) noexcept = default;

void scanner::start(scan_options /*opts*/) {}
void scanner::stop() noexcept {}
bool scanner::scanning() const noexcept { return false; }

} // namespace dts::bluetooth
