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
#include <concepts>
#include <span>
#include <vector>

namespace nj {

class Bitstream {
    public:
        Bitstream(const std::vector<std::uint8_t> &data): data(data), cur(data.begin()) { }

        template <typename T>
        T get() {
            if (this->cur + sizeof(T) >= this->data.end()) {
                this->cur = this->data.end();
                return {};
            }
            auto tmp = *reinterpret_cast<const T *>(this->cur.base());
            this->cur += sizeof(T);
            return tmp;
        }

        template <std::integral T>
        T get_be() {
            if constexpr (sizeof(T) == 1)
                return this->get<T>();
            else if constexpr (sizeof(T) == 2)
                return __builtin_bswap16(this->get<T>());
            else if constexpr (sizeof(T) == 4)
                return __builtin_bswap32(this->get<T>());
            else if constexpr (sizeof(T) == 8)
                return __builtin_bswap64(this->get<T>());
            return {};
        }

        void skip(std::size_t size) {
            this->cur += size;
        }

        void rewind(std::size_t size) {
            this->cur -= size;
        }

        bool empty() const {
            return this->cur >= this->data.end();
        }

        std::size_t size() const {
            return this->data.end() - this->cur;
        }

        auto current() const {
            return this->cur;
        }

    private:
        const std::vector<std::uint8_t> &data;
        std::vector<std::uint8_t>::const_iterator cur;
};

} // namespace nj
