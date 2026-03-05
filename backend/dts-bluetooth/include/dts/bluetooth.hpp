#pragma once

#include <dts/signal.hpp>

#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace dts::bluetooth {

struct adapter_info final {
    std::string adapter_key;   // stable OS-level identifier
    std::string name;          // human-readable adapter name
    std::string address;       // Bluetooth device address
    bool present{true};        // adapter exists in OS
    bool powered{false};       // adapter is powered on
};

// Thread-safe snapshot of all host Bluetooth adapters.
// Returns empty vector on unsupported platforms.
// Throws std::system_error on backend/OS query failures.
[[nodiscard]] std::vector<adapter_info> enumerate_adapters();

struct start_options final {
    // When true, existing adapters are emitted as on_arrived events
    // synchronously in the start() caller's thread, before start() returns.
    // Default: false.
    bool emit_initial_snapshot{false};
};

struct monitor_error final {
    std::error_code code;
    std::string message;
};

class monitor final {
public:
    monitor();
    ~monitor() noexcept;

    monitor(const monitor&) = delete;
    monitor& operator=(const monitor&) = delete;
    monitor(monitor&&) noexcept;
    monitor& operator=(monitor&&) noexcept;

    // Idempotent lifecycle control.
    void start(start_options opts = {});
    void stop() noexcept;
    [[nodiscard]] bool running() const noexcept;

    // --- on_arrived -----------------------------------------------------------

    template <typename Fn>
    requires std::is_invocable_r_v<void, Fn&, adapter_info const&>
    [[nodiscard]] dts::connection on_arrived(Fn&& slot) {
        if (!arrived_) { return {}; }
        auto handler = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(slot));
        return arrived_->connect([handler](adapter_info const& info) {
            std::invoke(*handler, info);
        });
    }

    template <typename Fn>
    requires std::is_invocable_r_v<void, Fn&, adapter_info const&>
    [[nodiscard]] dts::scoped_connection on_arrived_scoped(Fn&& slot) {
        return dts::scoped_connection{on_arrived(std::forward<Fn>(slot))};
    }

    // --- on_departed ----------------------------------------------------------

    template <typename Fn>
    requires std::is_invocable_r_v<void, Fn&, adapter_info const&>
    [[nodiscard]] dts::connection on_departed(Fn&& slot) {
        if (!departed_) { return {}; }
        auto handler = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(slot));
        return departed_->connect([handler](adapter_info const& info) {
            std::invoke(*handler, info);
        });
    }

    template <typename Fn>
    requires std::is_invocable_r_v<void, Fn&, adapter_info const&>
    [[nodiscard]] dts::scoped_connection on_departed_scoped(Fn&& slot) {
        return dts::scoped_connection{on_departed(std::forward<Fn>(slot))};
    }

    // --- on_changed -----------------------------------------------------------

    template <typename Fn>
    requires std::is_invocable_r_v<void, Fn&, adapter_info const&>
    [[nodiscard]] dts::connection on_changed(Fn&& slot) {
        if (!changed_) { return {}; }
        auto handler = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(slot));
        return changed_->connect([handler](adapter_info const& info) {
            std::invoke(*handler, info);
        });
    }

    template <typename Fn>
    requires std::is_invocable_r_v<void, Fn&, adapter_info const&>
    [[nodiscard]] dts::scoped_connection on_changed_scoped(Fn&& slot) {
        return dts::scoped_connection{on_changed(std::forward<Fn>(slot))};
    }

    // --- on_error -------------------------------------------------------------

    template <typename Fn>
    requires std::is_invocable_r_v<void, Fn&, monitor_error const&>
    [[nodiscard]] dts::connection on_error(Fn&& slot) {
        if (!error_) { return {}; }
        auto handler = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(slot));
        return error_->connect([handler](monitor_error const& err) {
            std::invoke(*handler, err);
        });
    }

    template <typename Fn>
    requires std::is_invocable_r_v<void, Fn&, monitor_error const&>
    [[nodiscard]] dts::scoped_connection on_error_scoped(Fn&& slot) {
        return dts::scoped_connection{on_error(std::forward<Fn>(slot))};
    }

private:
    struct impl;
    std::shared_ptr<dts::signal<void(adapter_info const&)>>  arrived_{std::make_shared<dts::signal<void(adapter_info const&)>>()};
    std::shared_ptr<dts::signal<void(adapter_info const&)>>  departed_{std::make_shared<dts::signal<void(adapter_info const&)>>()};
    std::shared_ptr<dts::signal<void(adapter_info const&)>>  changed_{std::make_shared<dts::signal<void(adapter_info const&)>>()};
    std::shared_ptr<dts::signal<void(monitor_error const&)>> error_{std::make_shared<dts::signal<void(monitor_error const&)>>()};
    std::shared_ptr<impl> impl_;
};

// Contract:
// - Public API hides backend/library types; no OS-specific headers exposed.
// - enumerate_adapters() is thread-safe.
// - monitor start/stop are thread-safe and idempotent.
// - monitor::running() is thread-safe.
// - monitor emits arrived/departed/changed events from a background polling thread.
// - Initial-snapshot arrived events are emitted on the start() caller thread.
// - monitor shutdown is safe; destroying monitor stops observation.
// - Event subscription uses dts::signal connection/scoped_connection semantics.
// - Subscription helpers store handlers in shared ownership so move-only
//   callables are accepted.
// - on_changed fires when an existing adapter's mutable state changes
//   (powered, name, address) but the adapter remains present.

} // namespace dts::bluetooth
