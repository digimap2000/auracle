#pragma once

#include <inventory/inventory.hpp>

#include <auracle/inventory/v1/inventory.pb.h>

namespace auracle::rpc {

namespace proto = ::auracle::inventory::v1;

void to_proto(const inventory::SerialDetails& src, proto::SerialDetails* dst);
void to_proto(const inventory::MdnsDetails& src, proto::MdnsDetails* dst);
void to_proto(const inventory::Identity& src, proto::Identity* dst);
void to_proto(const inventory::Lease& src, proto::Lease* dst);
void to_proto(const inventory::HardwareCandidate& src, proto::HardwareCandidate* dst);
void to_proto(const inventory::HardwareUnit& src, proto::HardwareUnit* dst);
void to_proto(const inventory::InventoryEvent& src, proto::InventoryEvent* dst);

} // namespace auracle::rpc
