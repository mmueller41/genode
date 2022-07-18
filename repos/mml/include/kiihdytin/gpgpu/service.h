/*
 * \brief  Definition of the GPGPU service's root component
 * \author Michael Müller
 * \date   2022-07-17
 */

/*
 * Copyright (C) 2022 Michael Müller
 *
 * This file is distributed under the terms of the
 * GNU Affero General Public License version 3.
 */
#pragma once

#include <base/component.h>
#include <root/component.h>
#include <base/heap.h>
#include <util/list.h>
#include "session_component.h"


namespace Kiihdytin::GPGPU {
    class Service;
}

/**
 * @brief The GPGPU service provides multiplexed accesses to GPGPU functionality to its clients.
 * 
 */

class Kiihdytin::GPGPU::Session : public Genode::Root_component<Kiihdytin::GPGPU::Session_component>
{
    private:
        Genode::List<Kiihdytin::GPGPU::Session_component> sessions;
    
    protected:
        
        Session_component *_create_session(const char*) override {
            return new (md_alloc()) Session_component();
        }

    public:
        Session(Genode::Entrypoint &ep, Genode::Allocator &alloc) : Genode::Root_component<Session_component>(ep, alloc) {}
};