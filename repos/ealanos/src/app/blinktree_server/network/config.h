#pragma once

namespace application::blinktree_server::network {
class config
{
public:
    static constexpr auto max_connections() noexcept { return 64U; }
};
} // namespace mx::io::network