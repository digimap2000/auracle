#pragma once

#include <inventory/registry.hpp>

#include <auracle/inventory/v1/inventory.grpc.pb.h>

#include <grpcpp/grpcpp.h>

namespace auracle::rpc {

namespace proto = ::auracle::inventory::v1;

class InventoryServiceImpl final : public proto::InventoryService::Service {
public:
    explicit InventoryServiceImpl(inventory::InventoryRegistry& registry);

    grpc::Status Watch(
        grpc::ServerContext* context,
        const proto::WatchRequest* request,
        grpc::ServerWriter<proto::InventoryEvent>* writer) override;

    grpc::Status ListCandidates(
        grpc::ServerContext* context,
        const proto::ListCandidatesRequest* request,
        proto::ListCandidatesResponse* response) override;

    grpc::Status ListUnits(
        grpc::ServerContext* context,
        const proto::ListUnitsRequest* request,
        proto::ListUnitsResponse* response) override;

private:
    inventory::InventoryRegistry& registry_;
};

} // namespace auracle::rpc
