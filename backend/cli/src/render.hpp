#pragma once

#include "watch.hpp"

#include <auracle/inventory/v1/inventory.pb.h>

#include <memory>

namespace auracle::cli {

namespace proto = ::auracle::inventory::v1;

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(const proto::InventoryEvent& event) = 0;
};

class PrettyRenderer final : public IRenderer {
public:
    void render(const proto::InventoryEvent& event) override;
};

class JsonRenderer final : public IRenderer {
public:
    void render(const proto::InventoryEvent& event) override;
};

class PrototextRenderer final : public IRenderer {
public:
    void render(const proto::InventoryEvent& event) override;
};

[[nodiscard]] std::unique_ptr<IRenderer> make_renderer(OutputFormat format);

} // namespace auracle::cli
