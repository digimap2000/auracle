#include <dts/bluetooth.hpp>

#import <CoreBluetooth/CoreBluetooth.h>

#include <atomic>
#include <mutex>

// ---------------------------------------------------------------------------
// CBCentralManager delegate — bridges CoreBluetooth callbacks to C++
// ---------------------------------------------------------------------------

@interface DtsBleDelegate : NSObject <CBCentralManagerDelegate>
@property (nonatomic, assign) std::shared_ptr<dts::signal<void(dts::bluetooth::advertisement const&)>> advSignal;
@property (nonatomic, assign) std::shared_ptr<dts::signal<void(dts::bluetooth::monitor_error const&)>> errSignal;
@property (nonatomic, assign) std::atomic<bool>* readyFlag;
@property (nonatomic, assign) bool allowDuplicates;
@end

@implementation DtsBleDelegate {
    CBCentralManager* _manager;
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    _manager = central;
    switch (central.state) {
        case CBManagerStatePoweredOn:
            if (_readyFlag) {
                _readyFlag->store(true);
            }
            // Start scanning now that BLE is ready
            [self startScanIfReady:central];
            break;
        case CBManagerStateUnsupported:
        case CBManagerStateUnauthorized:
        case CBManagerStatePoweredOff:
            if (_errSignal) {
                auto code = std::make_error_code(std::errc::not_supported);
                NSString* desc = [NSString stringWithFormat:@"CBCentralManager state: %ld", (long)central.state];
                _errSignal->emit(dts::bluetooth::monitor_error{code, std::string([desc UTF8String])});
            }
            break;
        default:
            break;
    }
}

- (void)startScanIfReady:(CBCentralManager *)central {
    if (central.state != CBManagerStatePoweredOn) {
        return;
    }
    NSDictionary* opts = @{
        CBCentralManagerScanOptionAllowDuplicatesKey: @(_allowDuplicates),
    };
    [central scanForPeripheralsWithServices:nil options:opts];
}

- (void)centralManager:(CBCentralManager *)central
 didDiscoverPeripheral:(CBPeripheral *)peripheral
     advertisementData:(NSDictionary<NSString *,id> *)advData
                  RSSI:(NSNumber *)RSSI
{
    (void)central;
    if (!_advSignal) {
        return;
    }

    dts::bluetooth::advertisement adv;

    // Device identifier (stable per-app UUID on macOS/iOS)
    adv.device_id = std::string([[peripheral.identifier UUIDString] UTF8String]);

    // Local name — prefer advertisement data, fall back to peripheral name
    NSString* advName = advData[CBAdvertisementDataLocalNameKey];
    if (advName != nil) {
        adv.name = std::string([advName UTF8String]);
    } else if (peripheral.name != nil) {
        adv.name = std::string([peripheral.name UTF8String]);
    }

    // RSSI
    adv.rssi = [RSSI intValue];

    // TX power level
    NSNumber* txPower = advData[CBAdvertisementDataTxPowerLevelKey];
    if (txPower != nil) {
        adv.tx_power = [txPower intValue];
    }

    // Service UUIDs
    NSArray<CBUUID*>* serviceUUIDs = advData[CBAdvertisementDataServiceUUIDsKey];
    if (serviceUUIDs != nil) {
        for (CBUUID* uuid in serviceUUIDs) {
            adv.service_uuids.emplace_back(std::string([[uuid UUIDString] UTF8String]));
        }
    }

    // Manufacturer-specific data
    NSData* mfgData = advData[CBAdvertisementDataManufacturerDataKey];
    if (mfgData != nil && mfgData.length >= 2) {
        const auto* bytes = static_cast<const std::uint8_t*>(mfgData.bytes);
        // First two bytes are the company identifier (little-endian)
        adv.company_id = static_cast<std::uint16_t>(bytes[0]) |
                         (static_cast<std::uint16_t>(bytes[1]) << 8);
        adv.manufacturer_data.assign(bytes + 2, bytes + mfgData.length);
    }

    _advSignal->emit(adv);
}

- (void)stopScan {
    if (_manager != nil && _manager.state == CBManagerStatePoweredOn) {
        [_manager stopScan];
    }
}

@end

// ---------------------------------------------------------------------------
// scanner::impl
// ---------------------------------------------------------------------------

namespace dts::bluetooth {

struct scanner::impl {
    std::shared_ptr<dts::signal<void(advertisement const&)>> adv_signal;
    std::shared_ptr<dts::signal<void(monitor_error const&)>> err_signal;

    dispatch_queue_t queue{nullptr};
    DtsBleDelegate* delegate{nil};
    CBCentralManager* manager{nil};

    std::atomic<bool> is_scanning{false};
    std::atomic<bool> bt_ready{false};

    void create(scan_options opts) {
        queue = dispatch_queue_create("dts.bluetooth.scanner", DISPATCH_QUEUE_SERIAL);

        delegate = [[DtsBleDelegate alloc] init];
        delegate.advSignal = adv_signal;
        delegate.errSignal = err_signal;
        delegate.readyFlag = &bt_ready;
        delegate.allowDuplicates = opts.allow_duplicates;

        // CBCentralManager must be created on the dispatch queue
        dispatch_sync(queue, ^{
            manager = [[CBCentralManager alloc] initWithDelegate:delegate
                                                          queue:queue];
        });

        is_scanning.store(true);
    }

    void destroy() {
        if (delegate != nil) {
            [delegate stopScan];
        }
        // Nil out the manager on the queue to avoid races
        if (queue != nullptr) {
            dispatch_sync(queue, ^{
                manager.delegate = nil;
                manager = nil;
            });
        }
        delegate = nil;
        queue = nullptr;
        is_scanning.store(false);
        bt_ready.store(false);
    }
};

// ---------------------------------------------------------------------------
// scanner lifecycle
// ---------------------------------------------------------------------------

scanner::scanner() = default;
scanner::~scanner() noexcept { stop(); }

scanner::scanner(scanner&&) noexcept = default;
scanner& scanner::operator=(scanner&&) noexcept = default;

void scanner::start(scan_options opts) {
    if (impl_ && impl_->is_scanning.load()) {
        return;
    }

    impl_ = std::make_shared<impl>();
    impl_->adv_signal = advertisement_;
    impl_->err_signal = error_;
    impl_->create(opts);
}

void scanner::stop() noexcept {
    if (!impl_) {
        return;
    }
    impl_->destroy();
    impl_.reset();
}

bool scanner::scanning() const noexcept {
    return impl_ && impl_->is_scanning.load();
}

} // namespace dts::bluetooth
