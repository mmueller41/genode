#pragma once
#include "task.h"
#include <ealanos/util/ecpp/static_vector.h>
#include <mx/system/cache.h>
#include <utility>

namespace mx::tasking {
class PrefetchItem
{
public:
    constexpr PrefetchItem() noexcept = default;

    ~PrefetchItem() noexcept = default;

    PrefetchItem &operator=(PrefetchItem &&) noexcept = default;

    [[nodiscard]] bool has_resource() const noexcept
    {
        return _resource != nullptr && _prefetch_descriptor.empty() == false;
    }

    [[nodiscard]] std::int64_t *resource() const noexcept { return _resource; }
    [[nodiscard]] PrefetchDescriptor prefetch_descriptor() const noexcept { return _prefetch_descriptor; }

    void resource(std::int64_t *resource, PrefetchDescriptor descriptor) noexcept
    {
        _resource = resource;
        _prefetch_descriptor = descriptor;
    }

private:
    std::int64_t *_resource{nullptr};
    PrefetchDescriptor _prefetch_descriptor{};
};

template <std::uint8_t S> class PrefetchMaskExecutor
{
public:
    template <std::uint8_t N> static constexpr bool is_prefetch_cl()
    {
        constexpr auto MASK = 0b1 << N;
        return (S & MASK) == MASK;
    }

    template <system::cache::level L, system::cache::access A>
    static void execute([[maybe_unused]] const std::int64_t *address, [[maybe_unused]] const std::uint8_t word_offset)
    {
        if constexpr (is_prefetch_cl<0U>())
        {
            const auto *addr = address + (word_offset * 8U);
            system::cache::prefetch<L, A, 1U>(addr);
        }

        if constexpr (is_prefetch_cl<1U>())
        {
            const auto *addr = address + ((word_offset + 1U) * 8U);
            system::cache::prefetch<L, A, 1U>(addr);
        }

        if constexpr (is_prefetch_cl<2U>())
        {
            const auto *addr = address + ((word_offset + 2U) * 8U);
            system::cache::prefetch<L, A, 1U>(addr);
        }

        if constexpr (is_prefetch_cl<3U>())
        {
            const auto *addr = address + ((word_offset + 3U) * 8U);
            system::cache::prefetch<L, A, 1U>(addr);
        }

        if constexpr (is_prefetch_cl<4U>())
        {
            const auto *addr = address + ((word_offset + 4U) * 8U);
            system::cache::prefetch<L, A, 1U>(addr);
        }

        if constexpr (is_prefetch_cl<5U>())
        {
            const auto *addr = address + ((word_offset + 5U) * 8U);
            system::cache::prefetch<L, A, 1U>(addr);
        }

        if constexpr (is_prefetch_cl<6U>())
        {
            const auto *addr = address + ((word_offset + 6U) * 8U);
            system::cache::prefetch<L, A, 1U>(addr);
        }

        if constexpr (is_prefetch_cl<7U>())
        {
            const auto *addr = address + ((word_offset + 7U) * 8U);
            system::cache::prefetch<L, A, 1U>(addr);
        }
    }
};

/**
 * A prefetch slot is part of the prefetch buffer used for task
 * and resource prefetching
 * A slot can contain up to one task and one resource that are
 * prefetched by the channel.
 */
class PrefetchSlot
{
public:
    constexpr PrefetchSlot() noexcept = default;
    ~PrefetchSlot() = default;

    void assign(resource::ptr resource, PrefetchDescriptor descriptor) noexcept;
    void prefetch() noexcept;

private:
    PrefetchItem _item;

    template <system::cache::level L, system::cache::access A>
    static void prefetch_mask(const std::int64_t *address, const PrefetchDescriptor::data_t prefetch_mask) noexcept
    {
        PrefetchSlot::prefetch_word_at_offset<L, A, 0U>(address, prefetch_mask);
        PrefetchSlot::prefetch_word_at_offset<L, A, 1U>(address, prefetch_mask);
        PrefetchSlot::prefetch_word_at_offset<L, A, 2U>(address, prefetch_mask);
        PrefetchSlot::prefetch_word_at_offset<L, A, 3U>(address, prefetch_mask);
        PrefetchSlot::prefetch_word_at_offset<L, A, 4U>(address, prefetch_mask);
        PrefetchSlot::prefetch_word_at_offset<L, A, 5U>(address, prefetch_mask);
        PrefetchSlot::prefetch_word_at_offset<L, A, 6U>(address, prefetch_mask);
        PrefetchSlot::prefetch_word_at_offset<L, A, 7U>(address, prefetch_mask);
    }

    template <system::cache::level L, system::cache::access A, std::uint8_t O>
    static void prefetch_word_at_offset(const std::int64_t *address, const PrefetchDescriptor::data_t mask)
    {
        if constexpr (O == 0U)
        {
            const auto prefetch_word = std::uint8_t(mask & std::numeric_limits<std::uint8_t>::max());
            PrefetchSlot::prefetch_word<L, A>(address, prefetch_word, 0U);
        }
        else
        {
            constexpr auto offset = O * 8U;
            const auto prefetch_word = std::uint8_t((mask >> offset) & std::numeric_limits<std::uint8_t>::max());
            if (prefetch_word > 0U)
            {
                PrefetchSlot::prefetch_word<L, A>(address, prefetch_word, offset);
            }
        }
    }

    template <system::cache::level L, system::cache::access A>
    static void prefetch_word(const std::int64_t *address, const std::uint8_t word, const std::uint8_t word_offset)
    {
        switch (word)
        {
        case 0:
            return;
        case 1:
            PrefetchMaskExecutor<1>::execute<L, A>(address, word_offset);
            return;
        case 2:
            PrefetchMaskExecutor<2>::execute<L, A>(address, word_offset);
            return;
        case 3:
            PrefetchMaskExecutor<3>::execute<L, A>(address, word_offset);
            return;
        case 4:
            PrefetchMaskExecutor<4>::execute<L, A>(address, word_offset);
            return;
        case 5:
            PrefetchMaskExecutor<5>::execute<L, A>(address, word_offset);
            return;
        case 6:
            PrefetchMaskExecutor<6>::execute<L, A>(address, word_offset);
            return;
        case 7:
            PrefetchMaskExecutor<7>::execute<L, A>(address, word_offset);
            return;
        case 8:
            PrefetchMaskExecutor<8>::execute<L, A>(address, word_offset);
            return;
        case 9:
            PrefetchMaskExecutor<9>::execute<L, A>(address, word_offset);
            return;
        case 10:
            PrefetchMaskExecutor<10>::execute<L, A>(address, word_offset);
            return;
        case 11:
            PrefetchMaskExecutor<11>::execute<L, A>(address, word_offset);
            return;
        case 12:
            PrefetchMaskExecutor<12>::execute<L, A>(address, word_offset);
            return;
        case 13:
            PrefetchMaskExecutor<13>::execute<L, A>(address, word_offset);
            return;
        case 14:
            PrefetchMaskExecutor<14>::execute<L, A>(address, word_offset);
            return;
        case 15:
            PrefetchMaskExecutor<15>::execute<L, A>(address, word_offset);
            return;
        case 16:
            PrefetchMaskExecutor<16>::execute<L, A>(address, word_offset);
            return;
        case 17:
            PrefetchMaskExecutor<17>::execute<L, A>(address, word_offset);
            return;
        case 18:
            PrefetchMaskExecutor<18>::execute<L, A>(address, word_offset);
            return;
        case 19:
            PrefetchMaskExecutor<19>::execute<L, A>(address, word_offset);
            return;
        case 20:
            PrefetchMaskExecutor<20>::execute<L, A>(address, word_offset);
            return;
        case 21:
            PrefetchMaskExecutor<21>::execute<L, A>(address, word_offset);
            return;
        case 22:
            PrefetchMaskExecutor<22>::execute<L, A>(address, word_offset);
            return;
        case 23:
            PrefetchMaskExecutor<23>::execute<L, A>(address, word_offset);
            return;
        case 24:
            PrefetchMaskExecutor<24>::execute<L, A>(address, word_offset);
            return;
        case 25:
            PrefetchMaskExecutor<25>::execute<L, A>(address, word_offset);
            return;
        case 26:
            PrefetchMaskExecutor<26>::execute<L, A>(address, word_offset);
            return;
        case 27:
            PrefetchMaskExecutor<27>::execute<L, A>(address, word_offset);
            return;
        case 28:
            PrefetchMaskExecutor<28>::execute<L, A>(address, word_offset);
            return;
        case 29:
            PrefetchMaskExecutor<29>::execute<L, A>(address, word_offset);
            return;
        case 30:
            PrefetchMaskExecutor<30>::execute<L, A>(address, word_offset);
            return;
        case 31:
            PrefetchMaskExecutor<31>::execute<L, A>(address, word_offset);
            return;
        case 32:
            PrefetchMaskExecutor<32>::execute<L, A>(address, word_offset);
            return;
        case 33:
            PrefetchMaskExecutor<33>::execute<L, A>(address, word_offset);
            return;
        case 34:
            PrefetchMaskExecutor<34>::execute<L, A>(address, word_offset);
            return;
        case 35:
            PrefetchMaskExecutor<35>::execute<L, A>(address, word_offset);
            return;
        case 36:
            PrefetchMaskExecutor<36>::execute<L, A>(address, word_offset);
            return;
        case 37:
            PrefetchMaskExecutor<37>::execute<L, A>(address, word_offset);
            return;
        case 38:
            PrefetchMaskExecutor<38>::execute<L, A>(address, word_offset);
            return;
        case 39:
            PrefetchMaskExecutor<39>::execute<L, A>(address, word_offset);
            return;
        case 40:
            PrefetchMaskExecutor<40>::execute<L, A>(address, word_offset);
            return;
        case 41:
            PrefetchMaskExecutor<41>::execute<L, A>(address, word_offset);
            return;
        case 42:
            PrefetchMaskExecutor<42>::execute<L, A>(address, word_offset);
            return;
        case 43:
            PrefetchMaskExecutor<43>::execute<L, A>(address, word_offset);
            return;
        case 44:
            PrefetchMaskExecutor<44>::execute<L, A>(address, word_offset);
            return;
        case 45:
            PrefetchMaskExecutor<45>::execute<L, A>(address, word_offset);
            return;
        case 46:
            PrefetchMaskExecutor<46>::execute<L, A>(address, word_offset);
            return;
        case 47:
            PrefetchMaskExecutor<47>::execute<L, A>(address, word_offset);
            return;
        case 48:
            PrefetchMaskExecutor<48>::execute<L, A>(address, word_offset);
            return;
        case 49:
            PrefetchMaskExecutor<49>::execute<L, A>(address, word_offset);
            return;
        case 50:
            PrefetchMaskExecutor<50>::execute<L, A>(address, word_offset);
            return;
        case 51:
            PrefetchMaskExecutor<51>::execute<L, A>(address, word_offset);
            return;
        case 52:
            PrefetchMaskExecutor<52>::execute<L, A>(address, word_offset);
            return;
        case 53:
            PrefetchMaskExecutor<53>::execute<L, A>(address, word_offset);
            return;
        case 54:
            PrefetchMaskExecutor<54>::execute<L, A>(address, word_offset);
            return;
        case 55:
            PrefetchMaskExecutor<55>::execute<L, A>(address, word_offset);
            return;
        case 56:
            PrefetchMaskExecutor<56>::execute<L, A>(address, word_offset);
            return;
        case 57:
            PrefetchMaskExecutor<57>::execute<L, A>(address, word_offset);
            return;
        case 58:
            PrefetchMaskExecutor<58>::execute<L, A>(address, word_offset);
            return;
        case 59:
            PrefetchMaskExecutor<59>::execute<L, A>(address, word_offset);
            return;
        case 60:
            PrefetchMaskExecutor<60>::execute<L, A>(address, word_offset);
            return;
        case 61:
            PrefetchMaskExecutor<61>::execute<L, A>(address, word_offset);
            return;
        case 62:
            PrefetchMaskExecutor<62>::execute<L, A>(address, word_offset);
            return;
        case 63:
            PrefetchMaskExecutor<63>::execute<L, A>(address, word_offset);
            return;
        case 64:
            PrefetchMaskExecutor<64>::execute<L, A>(address, word_offset);
            return;
        case 65:
            PrefetchMaskExecutor<65>::execute<L, A>(address, word_offset);
            return;
        case 66:
            PrefetchMaskExecutor<66>::execute<L, A>(address, word_offset);
            return;
        case 67:
            PrefetchMaskExecutor<67>::execute<L, A>(address, word_offset);
            return;
        case 68:
            PrefetchMaskExecutor<68>::execute<L, A>(address, word_offset);
            return;
        case 69:
            PrefetchMaskExecutor<69>::execute<L, A>(address, word_offset);
            return;
        case 70:
            PrefetchMaskExecutor<70>::execute<L, A>(address, word_offset);
            return;
        case 71:
            PrefetchMaskExecutor<71>::execute<L, A>(address, word_offset);
            return;
        case 72:
            PrefetchMaskExecutor<72>::execute<L, A>(address, word_offset);
            return;
        case 73:
            PrefetchMaskExecutor<73>::execute<L, A>(address, word_offset);
            return;
        case 74:
            PrefetchMaskExecutor<74>::execute<L, A>(address, word_offset);
            return;
        case 75:
            PrefetchMaskExecutor<75>::execute<L, A>(address, word_offset);
            return;
        case 76:
            PrefetchMaskExecutor<76>::execute<L, A>(address, word_offset);
            return;
        case 77:
            PrefetchMaskExecutor<77>::execute<L, A>(address, word_offset);
            return;
        case 78:
            PrefetchMaskExecutor<78>::execute<L, A>(address, word_offset);
            return;
        case 79:
            PrefetchMaskExecutor<79>::execute<L, A>(address, word_offset);
            return;
        case 80:
            PrefetchMaskExecutor<80>::execute<L, A>(address, word_offset);
            return;
        case 81:
            PrefetchMaskExecutor<81>::execute<L, A>(address, word_offset);
            return;
        case 82:
            PrefetchMaskExecutor<82>::execute<L, A>(address, word_offset);
            return;
        case 83:
            PrefetchMaskExecutor<83>::execute<L, A>(address, word_offset);
            return;
        case 84:
            PrefetchMaskExecutor<84>::execute<L, A>(address, word_offset);
            return;
        case 85:
            PrefetchMaskExecutor<85>::execute<L, A>(address, word_offset);
            return;
        case 86:
            PrefetchMaskExecutor<86>::execute<L, A>(address, word_offset);
            return;
        case 87:
            PrefetchMaskExecutor<87>::execute<L, A>(address, word_offset);
            return;
        case 88:
            PrefetchMaskExecutor<88>::execute<L, A>(address, word_offset);
            return;
        case 89:
            PrefetchMaskExecutor<89>::execute<L, A>(address, word_offset);
            return;
        case 90:
            PrefetchMaskExecutor<90>::execute<L, A>(address, word_offset);
            return;
        case 91:
            PrefetchMaskExecutor<91>::execute<L, A>(address, word_offset);
            return;
        case 92:
            PrefetchMaskExecutor<92>::execute<L, A>(address, word_offset);
            return;
        case 93:
            PrefetchMaskExecutor<93>::execute<L, A>(address, word_offset);
            return;
        case 94:
            PrefetchMaskExecutor<94>::execute<L, A>(address, word_offset);
            return;
        case 95:
            PrefetchMaskExecutor<95>::execute<L, A>(address, word_offset);
            return;
        case 96:
            PrefetchMaskExecutor<96>::execute<L, A>(address, word_offset);
            return;
        case 97:
            PrefetchMaskExecutor<97>::execute<L, A>(address, word_offset);
            return;
        case 98:
            PrefetchMaskExecutor<98>::execute<L, A>(address, word_offset);
            return;
        case 99:
            PrefetchMaskExecutor<99>::execute<L, A>(address, word_offset);
            return;
        case 100:
            PrefetchMaskExecutor<100>::execute<L, A>(address, word_offset);
            return;
        case 101:
            PrefetchMaskExecutor<101>::execute<L, A>(address, word_offset);
            return;
        case 102:
            PrefetchMaskExecutor<102>::execute<L, A>(address, word_offset);
            return;
        case 103:
            PrefetchMaskExecutor<103>::execute<L, A>(address, word_offset);
            return;
        case 104:
            PrefetchMaskExecutor<104>::execute<L, A>(address, word_offset);
            return;
        case 105:
            PrefetchMaskExecutor<105>::execute<L, A>(address, word_offset);
            return;
        case 106:
            PrefetchMaskExecutor<106>::execute<L, A>(address, word_offset);
            return;
        case 107:
            PrefetchMaskExecutor<107>::execute<L, A>(address, word_offset);
            return;
        case 108:
            PrefetchMaskExecutor<108>::execute<L, A>(address, word_offset);
            return;
        case 109:
            PrefetchMaskExecutor<109>::execute<L, A>(address, word_offset);
            return;
        case 110:
            PrefetchMaskExecutor<110>::execute<L, A>(address, word_offset);
            return;
        case 111:
            PrefetchMaskExecutor<111>::execute<L, A>(address, word_offset);
            return;
        case 112:
            PrefetchMaskExecutor<112>::execute<L, A>(address, word_offset);
            return;
        case 113:
            PrefetchMaskExecutor<113>::execute<L, A>(address, word_offset);
            return;
        case 114:
            PrefetchMaskExecutor<114>::execute<L, A>(address, word_offset);
            return;
        case 115:
            PrefetchMaskExecutor<115>::execute<L, A>(address, word_offset);
            return;
        case 116:
            PrefetchMaskExecutor<116>::execute<L, A>(address, word_offset);
            return;
        case 117:
            PrefetchMaskExecutor<117>::execute<L, A>(address, word_offset);
            return;
        case 118:
            PrefetchMaskExecutor<118>::execute<L, A>(address, word_offset);
            return;
        case 119:
            PrefetchMaskExecutor<119>::execute<L, A>(address, word_offset);
            return;
        case 120:
            PrefetchMaskExecutor<120>::execute<L, A>(address, word_offset);
            return;
        case 121:
            PrefetchMaskExecutor<121>::execute<L, A>(address, word_offset);
            return;
        case 122:
            PrefetchMaskExecutor<122>::execute<L, A>(address, word_offset);
            return;
        case 123:
            PrefetchMaskExecutor<123>::execute<L, A>(address, word_offset);
            return;
        case 124:
            PrefetchMaskExecutor<124>::execute<L, A>(address, word_offset);
            return;
        case 125:
            PrefetchMaskExecutor<125>::execute<L, A>(address, word_offset);
            return;
        case 126:
            PrefetchMaskExecutor<126>::execute<L, A>(address, word_offset);
            return;
        case 127:
            PrefetchMaskExecutor<127>::execute<L, A>(address, word_offset);
            return;
        case 128:
            PrefetchMaskExecutor<128>::execute<L, A>(address, word_offset);
            return;
        case 129:
            PrefetchMaskExecutor<129>::execute<L, A>(address, word_offset);
            return;
        case 130:
            PrefetchMaskExecutor<130>::execute<L, A>(address, word_offset);
            return;
        case 131:
            PrefetchMaskExecutor<131>::execute<L, A>(address, word_offset);
            return;
        case 132:
            PrefetchMaskExecutor<132>::execute<L, A>(address, word_offset);
            return;
        case 133:
            PrefetchMaskExecutor<133>::execute<L, A>(address, word_offset);
            return;
        case 134:
            PrefetchMaskExecutor<134>::execute<L, A>(address, word_offset);
            return;
        case 135:
            PrefetchMaskExecutor<135>::execute<L, A>(address, word_offset);
            return;
        case 136:
            PrefetchMaskExecutor<136>::execute<L, A>(address, word_offset);
            return;
        case 137:
            PrefetchMaskExecutor<137>::execute<L, A>(address, word_offset);
            return;
        case 138:
            PrefetchMaskExecutor<138>::execute<L, A>(address, word_offset);
            return;
        case 139:
            PrefetchMaskExecutor<139>::execute<L, A>(address, word_offset);
            return;
        case 140:
            PrefetchMaskExecutor<140>::execute<L, A>(address, word_offset);
            return;
        case 141:
            PrefetchMaskExecutor<141>::execute<L, A>(address, word_offset);
            return;
        case 142:
            PrefetchMaskExecutor<142>::execute<L, A>(address, word_offset);
            return;
        case 143:
            PrefetchMaskExecutor<143>::execute<L, A>(address, word_offset);
            return;
        case 144:
            PrefetchMaskExecutor<144>::execute<L, A>(address, word_offset);
            return;
        case 145:
            PrefetchMaskExecutor<145>::execute<L, A>(address, word_offset);
            return;
        case 146:
            PrefetchMaskExecutor<146>::execute<L, A>(address, word_offset);
            return;
        case 147:
            PrefetchMaskExecutor<147>::execute<L, A>(address, word_offset);
            return;
        case 148:
            PrefetchMaskExecutor<148>::execute<L, A>(address, word_offset);
            return;
        case 149:
            PrefetchMaskExecutor<149>::execute<L, A>(address, word_offset);
            return;
        case 150:
            PrefetchMaskExecutor<150>::execute<L, A>(address, word_offset);
            return;
        case 151:
            PrefetchMaskExecutor<151>::execute<L, A>(address, word_offset);
            return;
        case 152:
            PrefetchMaskExecutor<152>::execute<L, A>(address, word_offset);
            return;
        case 153:
            PrefetchMaskExecutor<153>::execute<L, A>(address, word_offset);
            return;
        case 154:
            PrefetchMaskExecutor<154>::execute<L, A>(address, word_offset);
            return;
        case 155:
            PrefetchMaskExecutor<155>::execute<L, A>(address, word_offset);
            return;
        case 156:
            PrefetchMaskExecutor<156>::execute<L, A>(address, word_offset);
            return;
        case 157:
            PrefetchMaskExecutor<157>::execute<L, A>(address, word_offset);
            return;
        case 158:
            PrefetchMaskExecutor<158>::execute<L, A>(address, word_offset);
            return;
        case 159:
            PrefetchMaskExecutor<159>::execute<L, A>(address, word_offset);
            return;
        case 160:
            PrefetchMaskExecutor<160>::execute<L, A>(address, word_offset);
            return;
        case 161:
            PrefetchMaskExecutor<161>::execute<L, A>(address, word_offset);
            return;
        case 162:
            PrefetchMaskExecutor<162>::execute<L, A>(address, word_offset);
            return;
        case 163:
            PrefetchMaskExecutor<163>::execute<L, A>(address, word_offset);
            return;
        case 164:
            PrefetchMaskExecutor<164>::execute<L, A>(address, word_offset);
            return;
        case 165:
            PrefetchMaskExecutor<165>::execute<L, A>(address, word_offset);
            return;
        case 166:
            PrefetchMaskExecutor<166>::execute<L, A>(address, word_offset);
            return;
        case 167:
            PrefetchMaskExecutor<167>::execute<L, A>(address, word_offset);
            return;
        case 168:
            PrefetchMaskExecutor<168>::execute<L, A>(address, word_offset);
            return;
        case 169:
            PrefetchMaskExecutor<169>::execute<L, A>(address, word_offset);
            return;
        case 170:
            PrefetchMaskExecutor<170>::execute<L, A>(address, word_offset);
            return;
        case 171:
            PrefetchMaskExecutor<171>::execute<L, A>(address, word_offset);
            return;
        case 172:
            PrefetchMaskExecutor<172>::execute<L, A>(address, word_offset);
            return;
        case 173:
            PrefetchMaskExecutor<173>::execute<L, A>(address, word_offset);
            return;
        case 174:
            PrefetchMaskExecutor<174>::execute<L, A>(address, word_offset);
            return;
        case 175:
            PrefetchMaskExecutor<175>::execute<L, A>(address, word_offset);
            return;
        case 176:
            PrefetchMaskExecutor<176>::execute<L, A>(address, word_offset);
            return;
        case 177:
            PrefetchMaskExecutor<177>::execute<L, A>(address, word_offset);
            return;
        case 178:
            PrefetchMaskExecutor<178>::execute<L, A>(address, word_offset);
            return;
        case 179:
            PrefetchMaskExecutor<179>::execute<L, A>(address, word_offset);
            return;
        case 180:
            PrefetchMaskExecutor<180>::execute<L, A>(address, word_offset);
            return;
        case 181:
            PrefetchMaskExecutor<181>::execute<L, A>(address, word_offset);
            return;
        case 182:
            PrefetchMaskExecutor<182>::execute<L, A>(address, word_offset);
            return;
        case 183:
            PrefetchMaskExecutor<183>::execute<L, A>(address, word_offset);
            return;
        case 184:
            PrefetchMaskExecutor<184>::execute<L, A>(address, word_offset);
            return;
        case 185:
            PrefetchMaskExecutor<185>::execute<L, A>(address, word_offset);
            return;
        case 186:
            PrefetchMaskExecutor<186>::execute<L, A>(address, word_offset);
            return;
        case 187:
            PrefetchMaskExecutor<187>::execute<L, A>(address, word_offset);
            return;
        case 188:
            PrefetchMaskExecutor<188>::execute<L, A>(address, word_offset);
            return;
        case 189:
            PrefetchMaskExecutor<189>::execute<L, A>(address, word_offset);
            return;
        case 190:
            PrefetchMaskExecutor<190>::execute<L, A>(address, word_offset);
            return;
        case 191:
            PrefetchMaskExecutor<191>::execute<L, A>(address, word_offset);
            return;
        case 192:
            PrefetchMaskExecutor<192>::execute<L, A>(address, word_offset);
            return;
        case 193:
            PrefetchMaskExecutor<193>::execute<L, A>(address, word_offset);
            return;
        case 194:
            PrefetchMaskExecutor<194>::execute<L, A>(address, word_offset);
            return;
        case 195:
            PrefetchMaskExecutor<195>::execute<L, A>(address, word_offset);
            return;
        case 196:
            PrefetchMaskExecutor<196>::execute<L, A>(address, word_offset);
            return;
        case 197:
            PrefetchMaskExecutor<197>::execute<L, A>(address, word_offset);
            return;
        case 198:
            PrefetchMaskExecutor<198>::execute<L, A>(address, word_offset);
            return;
        case 199:
            PrefetchMaskExecutor<199>::execute<L, A>(address, word_offset);
            return;
        case 200:
            PrefetchMaskExecutor<200>::execute<L, A>(address, word_offset);
            return;
        case 201:
            PrefetchMaskExecutor<201>::execute<L, A>(address, word_offset);
            return;
        case 202:
            PrefetchMaskExecutor<202>::execute<L, A>(address, word_offset);
            return;
        case 203:
            PrefetchMaskExecutor<203>::execute<L, A>(address, word_offset);
            return;
        case 204:
            PrefetchMaskExecutor<204>::execute<L, A>(address, word_offset);
            return;
        case 205:
            PrefetchMaskExecutor<205>::execute<L, A>(address, word_offset);
            return;
        case 206:
            PrefetchMaskExecutor<206>::execute<L, A>(address, word_offset);
            return;
        case 207:
            PrefetchMaskExecutor<207>::execute<L, A>(address, word_offset);
            return;
        case 208:
            PrefetchMaskExecutor<208>::execute<L, A>(address, word_offset);
            return;
        case 209:
            PrefetchMaskExecutor<209>::execute<L, A>(address, word_offset);
            return;
        case 210:
            PrefetchMaskExecutor<210>::execute<L, A>(address, word_offset);
            return;
        case 211:
            PrefetchMaskExecutor<211>::execute<L, A>(address, word_offset);
            return;
        case 212:
            PrefetchMaskExecutor<212>::execute<L, A>(address, word_offset);
            return;
        case 213:
            PrefetchMaskExecutor<213>::execute<L, A>(address, word_offset);
            return;
        case 214:
            PrefetchMaskExecutor<214>::execute<L, A>(address, word_offset);
            return;
        case 215:
            PrefetchMaskExecutor<215>::execute<L, A>(address, word_offset);
            return;
        case 216:
            PrefetchMaskExecutor<216>::execute<L, A>(address, word_offset);
            return;
        case 217:
            PrefetchMaskExecutor<217>::execute<L, A>(address, word_offset);
            return;
        case 218:
            PrefetchMaskExecutor<218>::execute<L, A>(address, word_offset);
            return;
        case 219:
            PrefetchMaskExecutor<219>::execute<L, A>(address, word_offset);
            return;
        case 220:
            PrefetchMaskExecutor<220>::execute<L, A>(address, word_offset);
            return;
        case 221:
            PrefetchMaskExecutor<221>::execute<L, A>(address, word_offset);
            return;
        case 222:
            PrefetchMaskExecutor<222>::execute<L, A>(address, word_offset);
            return;
        case 223:
            PrefetchMaskExecutor<223>::execute<L, A>(address, word_offset);
            return;
        case 224:
            PrefetchMaskExecutor<224>::execute<L, A>(address, word_offset);
            return;
        case 225:
            PrefetchMaskExecutor<225>::execute<L, A>(address, word_offset);
            return;
        case 226:
            PrefetchMaskExecutor<226>::execute<L, A>(address, word_offset);
            return;
        case 227:
            PrefetchMaskExecutor<227>::execute<L, A>(address, word_offset);
            return;
        case 228:
            PrefetchMaskExecutor<228>::execute<L, A>(address, word_offset);
            return;
        case 229:
            PrefetchMaskExecutor<229>::execute<L, A>(address, word_offset);
            return;
        case 230:
            PrefetchMaskExecutor<230>::execute<L, A>(address, word_offset);
            return;
        case 231:
            PrefetchMaskExecutor<231>::execute<L, A>(address, word_offset);
            return;
        case 232:
            PrefetchMaskExecutor<232>::execute<L, A>(address, word_offset);
            return;
        case 233:
            PrefetchMaskExecutor<233>::execute<L, A>(address, word_offset);
            return;
        case 234:
            PrefetchMaskExecutor<234>::execute<L, A>(address, word_offset);
            return;
        case 235:
            PrefetchMaskExecutor<235>::execute<L, A>(address, word_offset);
            return;
        case 236:
            PrefetchMaskExecutor<236>::execute<L, A>(address, word_offset);
            return;
        case 237:
            PrefetchMaskExecutor<237>::execute<L, A>(address, word_offset);
            return;
        case 238:
            PrefetchMaskExecutor<238>::execute<L, A>(address, word_offset);
            return;
        case 239:
            PrefetchMaskExecutor<239>::execute<L, A>(address, word_offset);
            return;
        case 240:
            PrefetchMaskExecutor<240>::execute<L, A>(address, word_offset);
            return;
        case 241:
            PrefetchMaskExecutor<241>::execute<L, A>(address, word_offset);
            return;
        case 242:
            PrefetchMaskExecutor<242>::execute<L, A>(address, word_offset);
            return;
        case 243:
            PrefetchMaskExecutor<243>::execute<L, A>(address, word_offset);
            return;
        case 244:
            PrefetchMaskExecutor<244>::execute<L, A>(address, word_offset);
            return;
        case 245:
            PrefetchMaskExecutor<245>::execute<L, A>(address, word_offset);
            return;
        case 246:
            PrefetchMaskExecutor<246>::execute<L, A>(address, word_offset);
            return;
        case 247:
            PrefetchMaskExecutor<247>::execute<L, A>(address, word_offset);
            return;
        case 248:
            PrefetchMaskExecutor<248>::execute<L, A>(address, word_offset);
            return;
        case 249:
            PrefetchMaskExecutor<249>::execute<L, A>(address, word_offset);
            return;
        case 250:
            PrefetchMaskExecutor<250>::execute<L, A>(address, word_offset);
            return;
        case 251:
            PrefetchMaskExecutor<251>::execute<L, A>(address, word_offset);
            return;
        case 252:
            PrefetchMaskExecutor<252>::execute<L, A>(address, word_offset);
            return;
        case 253:
            PrefetchMaskExecutor<253>::execute<L, A>(address, word_offset);
            return;
        case 254:
            PrefetchMaskExecutor<254>::execute<L, A>(address, word_offset);
            return;
        case 255:
            PrefetchMaskExecutor<255>::execute<L, A>(address, word_offset);
            return;
        }
    }
};

} // namespace mx::tasking