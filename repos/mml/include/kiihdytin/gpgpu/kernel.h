/*
 * \brief  Definition of a GPGPU kernel, i.e. OpenCL-slang for an executable unit of code for an OpenCL device 
 * \author Michael Müller
 * \date   2022-07-15
 */

/*
 * Copyright (C) 2022 Michael Müller
 *
 * This file is distributed under the terms of the
 * GNU Affero General Public License version 3.
 */
#pragma once

#include <gpgpu_driver.h>
#include <cstdint>
#include <util/list.h>
#include <driver/lib/chain.h>

namespace Kiihdytin::GPGPU {

typedef uint8_t Kernel_image;
    /**
     * @class This class represents an OpenCL kernel 
     * 
     */
    class Kernel : public Chain
    {
        private:
            struct kernel_config _configuration;
            Kernel_image *_image;

        public:
            /**
             * @brief get configuration for this kernel
             * @return reference to kernel configuration
             */
            inline struct kernel_config &get_config() { return _configuration; }

            /**
             * @brief get pointer to kernel image 
             * @return pointer to kernel's binary image
             */
            inline Kernel_image *get_image() { return _image; }
    };
}