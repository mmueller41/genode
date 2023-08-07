/*
 * \brief  Hoitaja — Cell Controller
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

#pragma once
#include <sandbox/child.h>

namespace Hoitaja
{
    class Cell_controller;
}

class Hoitaja::Cell_controller
{
    public:
        void create_cell();
        void destroy_cell();

        /**
         * @brief Determine which cells shall be shrinked down
         * 
         * @return Sandbox::Child* List of cells to shrink
         */
        Sandbox::Child *cells_to_shrink();
        /**
         * @brief Determine which cell shall be grown up
         * 
         * @return Sandbox::Child* List of cells to grow
         */
        Sandbox::Child *cells_to_grow();

        /**
         * @brief Regather performance metrics for next adaptation cycle
         * 
         */
        void update_metrics();
};