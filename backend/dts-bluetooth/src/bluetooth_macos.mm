#include <dts/bluetooth.hpp>

#import <IOBluetooth/IOBluetooth.h>

#include <thread>
#include <unordered_map>

namespace dts::bluetooth {

// ---------------------------------------------------------------------------
// enumerate_adapters (free function)
// ---------------------------------------------------------------------------

std::vector<adapter_info> enumerate_adapters() {
    IOBluetoothHostController* controller = [IOBluetoothHostController defaultController];
    if (controller == nil) {
        return {};
    }

    adapter_info info;
    info.adapter_key = "default";
    info.name = controller.nameAsString
        ? std::string([controller.nameAsString UTF8String])
        : "Bluetooth Adapter";
    info.address = controller.addressAsString
        ? std::string([controller.addressAsString UTF8String])
        : "";
    info.present = true;
    info.powered = (controller.powerState == kBluetoothHCIPowerStateON);

    return {std::move(info)};
}

// ---------------------------------------------------------------------------
// monitor::impl
// ---------------------------------------------------------------------------

struct monitor::impl {
    std::shared_ptr<dts::signal<void(adapter_info const&)>>  arrived;
    std::shared_ptr<dts::signal<void(adapter_info const&)>>  departed;
    std::shared_ptr<dts::signal<void(adapter_info const&)>>  changed;
    std::shared_ptr<dts::signal<void(monitor_error const&)>> error;

    std::jthread poll_thread;
    std::atomic<bool> is_running{false};
    std::unordered_map<std::string, adapter_info> previous;

    void poll_loop(std::stop_token stop) {
        while (!stop.stop_requested()) {
            poll_once();
            // Interruptible sleep: 100ms chunks over ~2s
            for (int i = 0; i < 20 && !stop.stop_requested(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    void poll_once() {
        std::vector<adapter_info> current;
        try {
            current = enumerate_adapters();
        } catch (const std::system_error& e) {
            if (error) {
                error->emit(monitor_error{e.code(), e.what()});
            }
            return;
        }

        // Build lookup of current adapters
        std::unordered_map<std::string, adapter_info> current_map;
        for (auto& a : current) {
            current_map.emplace(a.adapter_key, a);
        }

        // Detect departed (was in previous, not in current)
        for (const auto& [key, prev] : previous) {
            if (current_map.find(key) == current_map.end()) {
                if (departed) {
                    departed->emit(prev);
                }
            }
        }

        // Detect arrived and changed
        for (const auto& [key, cur] : current_map) {
            auto it = previous.find(key);
            if (it == previous.end()) {
                // New adapter
                if (arrived) {
                    arrived->emit(cur);
                }
            } else {
                // Existing — check for state changes
                const auto& prev = it->second;
                if (cur.powered != prev.powered ||
                    cur.name != prev.name ||
                    cur.address != prev.address ||
                    cur.present != prev.present) {
                    if (changed) {
                        changed->emit(cur);
                    }
                }
            }
        }

        previous = std::move(current_map);
    }

    void emit_snapshot() {
        try {
            auto adapters = enumerate_adapters();
            for (const auto& a : adapters) {
                if (arrived) {
                    arrived->emit(a);
                }
                previous.emplace(a.adapter_key, a);
            }
        } catch (const std::system_error& e) {
            if (error) {
                error->emit(monitor_error{e.code(), e.what()});
            }
        }
    }
};

// ---------------------------------------------------------------------------
// monitor lifecycle
// ---------------------------------------------------------------------------

monitor::monitor() = default;
monitor::~monitor() noexcept { stop(); }

monitor::monitor(monitor&&) noexcept = default;
monitor& monitor::operator=(monitor&&) noexcept = default;

void monitor::start(start_options opts) {
    if (impl_ && impl_->is_running.load()) {
        return;
    }

    impl_ = std::make_shared<impl>();
    impl_->arrived = arrived_;
    impl_->departed = departed_;
    impl_->changed = changed_;
    impl_->error = error_;

    if (opts.emit_initial_snapshot) {
        impl_->emit_snapshot();
    }

    impl_->is_running.store(true);
    impl_->poll_thread = std::jthread([p = impl_](std::stop_token stop) {
        p->poll_loop(stop);
    });
}

void monitor::stop() noexcept {
    if (!impl_) {
        return;
    }
    impl_->is_running.store(false);
    if (impl_->poll_thread.joinable()) {
        impl_->poll_thread.request_stop();
        impl_->poll_thread.join();
    }
    impl_.reset();
}

bool monitor::running() const noexcept {
    return impl_ && impl_->is_running.load();
}

} // namespace dts::bluetooth
