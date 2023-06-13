/*
 * \brief  Hoitaja — Core Allocator
 * \author Michael Müller, Norman Feske (Init)
 * \date   2023-04-20
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 * Copyright (C) 2023 Michael Müller, Osnabrück University
 *
 * This file is part of EalánOS, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <sandbox/child.h>
#include <sandbox/library.h>

/** Hoitaja includes **/
#include "load_controller.h"
#include "cell_controller.h"

namespace Hoitaja 
{
    class Core_allocator;
}

class Hoitaja::Core_allocator
{
    private:
        Sandbox::Child *_cells_to_grow;
        Sandbox::Child *_cells_to_shrink;

    public:
        /**
         * @brief Update core allocations for cells reported by Cell controller
         * 
         */
        void update();
};