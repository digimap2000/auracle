#pragma once

namespace auracle::inventory {

struct ICandidateProvider {
    virtual ~ICandidateProvider() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

} // namespace auracle::inventory
