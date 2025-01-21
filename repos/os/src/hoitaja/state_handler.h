#pragma once

#include <util/interface.h>

namespace Hoitaja {
    struct State_handler;
    class Cell;
}

struct Hoitaja::State_handler : Genode::Interface
{
    virtual void handle_habitat_state(Cell &cell) = 0;
};