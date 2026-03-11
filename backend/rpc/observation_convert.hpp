#pragma once

#include <observation/types.hpp>

#include <auracle/observation/v1/observation.pb.h>

namespace auracle::rpc {

namespace obs_proto = ::auracle::observation::v1;

void to_proto(const observation::BleAdvertisement& src, obs_proto::BleAdvertisement* dst);
void to_proto(const observation::ObservedBleDevice& src, obs_proto::ObservedBleDevice* dst);
void to_proto(const observation::ObservationEvent& src, obs_proto::ObservationEvent* dst);

} // namespace auracle::rpc
