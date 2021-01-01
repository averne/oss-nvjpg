// Copyright (C) 2021 averne
//
// This file is part of oss-nvjpg.
//
// oss-nvjpg is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// oss-nvjpg is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with oss-nvjpg.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include <tuple>
#include <vector>

#include <nvjpg/nv/map.hpp>
#include <nvjpg/nv/ioctl_types.h>
#include <nvjpg/nv/registers.hpp>

namespace nj {

struct Host1xOpcode {
    std::uint32_t value = 0;

    constexpr inline operator std::uint32_t() const {
        return this->value;
    }
};

struct OpcodeSetClass: public Host1xOpcode {
    constexpr OpcodeSetClass(std::uint32_t class_id, std::uint32_t offset = 0, std::uint32_t mask = 0) {
        this->value = (0 << 28) | (offset << 16) | (class_id << 6) | mask;
    }
};

struct OpcodeIncr: public Host1xOpcode {
    constexpr OpcodeIncr(std::uint32_t offset, std::uint32_t count) {
        this->value = (1 << 28) | (offset << 16) | count;
    }
};

struct OpcodeNonIncr: public Host1xOpcode {
    constexpr OpcodeNonIncr(std::uint32_t offset, std::uint32_t count) {
        this->value = (2 << 28) | (offset << 16) | count;
    }
};

struct OpcodeMask: public Host1xOpcode {
    constexpr OpcodeMask(std::uint32_t offset, std::uint32_t mask) {
        this->value = (3 << 28) | (offset << 16) | mask;
    }
};

struct OpcodeImm: public Host1xOpcode {
    constexpr OpcodeImm(std::uint32_t offset, std::uint32_t value) {
        this->value = (4 << 28) | (offset << 16) | value;
    }
};

class CmdBuf {
    public:
        using Word = std::uint32_t;

    public:
        CmdBuf(const NvMap &map): map(map), cur_word(static_cast<Word *>(map.address())) { }

        std::size_t size() const {
            return static_cast<std::size_t>(this->cur_word - static_cast<Word *>(this->map.address()));
        }

        void begin(std::uint32_t class_id, std::int32_t pre_fence = -1) {
            this->bufs.push_back({ this->map.handle(), static_cast<std::uint32_t>(this->size() * sizeof(Word)), 0 });
            this->exts.push_back({ pre_fence, 0 });
            this->class_ids.push_back(class_id);

            this->cur_buf_begin = this->size();
        }

        void end() {
            this->bufs.back().words = this->size() - this->cur_buf_begin;
        }

        void clear() {
            this->cur_word = static_cast<Word *>(this->map.address());
            this->bufs  .clear(), this->exts  .clear(), this->class_ids.clear();
            this->relocs.clear(), this->shifts.clear(), this->types    .clear();
        }

        void push_raw(Word word) {
            *this->cur_word++ = word;
        }

        void push_value(std::uint32_t offset, std::uint32_t value) {
            constexpr auto op = OpcodeIncr(NJ_REGPOS(ThiRegisters, method_0), 2);

            this->push_raw(op);
            this->push_raw(offset);
            this->push_raw(value);
        }

        void push_reloc(std::uint32_t offset, const NvMap &target, std::uint32_t target_offset = 0,
                std::uint32_t shift = 8, std::uint32_t type = NVHOST_RELOC_TYPE_DEFAULT) {
            this->push_value(offset, 0xdeadbeef); // Officially used placeholder value

            this->relocs.push_back({
                .cmdbuf_mem    = this->map.handle(),
                .cmdbuf_offset = static_cast<std::uint32_t>((this->size() - 1) * sizeof(Word)),
                .target_mem    = target.handle(),
                .target_offset = target_offset,
            });
            this->shifts.push_back({ shift });
            this->types.push_back({ type, 0 });
        }

        auto get_bufs() const {
            return std::make_tuple(this->bufs, this->exts, this->class_ids);
        }

        auto get_relocs() const {
            return std::make_tuple(this->relocs, this->shifts, this->types);
        }

    private:
        const NvMap &map;
        Word *cur_word;

        std::size_t cur_buf_begin = 0;

        std::vector<nvhost_cmdbuf>      bufs;
        std::vector<nvhost_cmdbuf_ext>  exts;
        std::vector<std::uint32_t>      class_ids;

        std::vector<nvhost_reloc>       relocs;
        std::vector<nvhost_reloc_shift> shifts;
        std::vector<nvhost_reloc_type>  types;
};

} // namespace nj
