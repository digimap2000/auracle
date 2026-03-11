#pragma once

#include <assigned-numbers/assigned_numbers.hpp>
#include <observation/types.hpp>

#include <auracle/observation/v1/observation.pb.h>

#include <span>

namespace auracle::rpc {

namespace obs_proto = ::auracle::observation::v1;

void to_proto(const observation::BleAdvertisement& src, obs_proto::BleAdvertisement* dst);
void to_proto(const observation::ObservedBleDevice& src, obs_proto::ObservedBleDevice* dst);
void to_proto(const observation::ObservationEvent& src, obs_proto::ObservationEvent* dst);
void decode_advertisement_to_proto(
    std::span<const std::uint8_t> raw_data,
    std::span<const std::uint8_t> raw_scan_response,
    obs_proto::DecodeAdvertisementResponse* dst);
void to_proto(
    const assigned_numbers::service_data_format_metadata& src,
    obs_proto::ServiceDataFormatMetadata* dst);

} // namespace auracle::rpc
