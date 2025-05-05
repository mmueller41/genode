#include "prefetch_slot.h"
#include <mx/system/cache.h>
#include <mx/system/environment.h>

using namespace mx::tasking;

void PrefetchSlot::assign(const resource::ptr resource, const PrefetchDescriptor descriptor) noexcept
{
    if (this->_item.has_resource() == false)
    {
        this->_item.resource(resource.get<std::int64_t>(), descriptor);
    }
}

void PrefetchSlot::prefetch() noexcept
{
    const auto prefetch_type = this->_item.prefetch_descriptor().id();
    const auto prefetch_data = this->_item.prefetch_descriptor().data_without_descriptor_bits();
    auto *resource = this->_item.resource();
    switch (prefetch_type)
    {
    case PrefetchDescriptor::Type::SizeNonTemporal: {
        const auto size = PrefetchSizeView{prefetch_data}.get();
        system::cache::prefetch_range<system::cache::NTA, system::cache::read>(resource, size);
        break;
    }
    case PrefetchDescriptor::Type::SizeTemporal: {
        const auto size = PrefetchSizeView{prefetch_data}.get();
        system::cache::prefetch_range<system::cache::L2, system::cache::read>(resource, size);
        break;
    }
    case PrefetchDescriptor::Type::SizeWrite: {
        const auto size = PrefetchSizeView{prefetch_data}.get();
        system::cache::prefetch_range<system::cache::ALL, system::cache::write>(resource, size);
        break;
    }
    case PrefetchDescriptor::Type::CallbackAny: {
        auto *callback = PrefetchCallbackView{prefetch_data}.get();
        callback(reinterpret_cast<void *>(resource));
        break;
    }
    case PrefetchDescriptor::Type::None:
        return;
    case PrefetchDescriptor::Type::MaskTemporal: {
        PrefetchSlot::prefetch_mask<system::cache::L2, system::cache::read>(resource, prefetch_data);
        break;
    }
    case PrefetchDescriptor::Type::MaskNonTemporal: {
        PrefetchSlot::prefetch_mask<system::cache::NTA, system::cache::read>(resource, prefetch_data);
        break;
    }
    case PrefetchDescriptor::Type::MaskWrite: {
        PrefetchSlot::prefetch_mask<system::cache::ALL, system::cache::write>(resource, prefetch_data);
        break;
    }
    }

    this->_item = PrefetchItem{};
}
