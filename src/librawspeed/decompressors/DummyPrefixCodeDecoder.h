/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2022 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#pragma once

#include "decompressors/AbstractPrefixCode.h" // for CodeTraits
#include "decompressors/HuffmanCode.h"        // for HuffmanCode...
#include "decompressors/PrefixCode.h"         // for PrefixCode...
#include "io/BitStream.h"                     // for BitStreamTraits
#include <cassert>                            // for invariant
#include <cstdint>                            // for uint32_t
#include <tuple>                              // for tie
#include <utility>                            // for pair
#include <vector>                             // for vector

namespace rawspeed {
class Buffer;

template <typename CodeTag = BaselineCodeTag>
class DummyPrefixCodeDecoder final {
public:
  using Tag = CodeTag;
  using Traits = CodeTraits<CodeTag>;

  explicit DummyPrefixCodeDecoder(HuffmanCode<CodeTag> code) {}
  explicit DummyPrefixCodeDecoder(PrefixCode<CodeTag> code) {}

private:
  bool fullDecode = true;
  bool fixDNGBug16 = false;

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    fullDecode = fullDecode_;
    fixDNGBug16 = fixDNGBug16_;
  }

  [[nodiscard]] bool isFullDecode() const { return fullDecode; }

  template <typename BIT_STREAM>
  inline typename Traits::CodeValueTy decodeCodeValue(BIT_STREAM& bs) const {
    static_assert(
        BitStreamTraits<typename BIT_STREAM::tag>::canUseWithPrefixCodeDecoder,
        "This BitStream specialization is not marked as usable here");
    invariant(!fullDecode);
    return decode<BIT_STREAM, false>(bs);
  }

  template <typename BIT_STREAM>
  inline int decodeDifference(BIT_STREAM& bs) const {
    static_assert(
        BitStreamTraits<typename BIT_STREAM::tag>::canUseWithPrefixCodeDecoder,
        "This BitStream specialization is not marked as usable here");
    invariant(fullDecode);
    return decode<BIT_STREAM, true>(bs);
  }

  // The bool template paraeter is to enable two versions:
  // one returning only the length of the of diff bits (see Hasselblad),
  // one to return the fully decoded diff.
  // All ifs depending on this bool will be optimized out by the compiler
  template <typename BIT_STREAM, bool FULL_DECODE>
  inline int decode(BIT_STREAM& bs) const {
    static_assert(
        BitStreamTraits<typename BIT_STREAM::tag>::canUseWithPrefixCodeDecoder,
        "This BitStream specialization is not marked as usable here");
    invariant(FULL_DECODE == fullDecode);

    (void)bs;

    return 0; // The answer is always the same.
  }
};

} // namespace rawspeed
