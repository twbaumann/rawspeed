/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "decompressors/PentaxDecompressor.h"
#include "adt/Array1DRef.h"               // for Array1DRef
#include "adt/Array2DRef.h"               // for Array2DRef
#include "adt/Invariant.h"                // for invariant
#include "adt/Point.h"                    // for iPoint2D
#include "common/Common.h"                // for extractHighBits, isIntN
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include "decompressors/HuffmanCode.h"
#include "decompressors/PrefixCodeDecoder.h" // for PrefixCodeDecoder
#include "io/BitPumpMSB.h" // for BitPumpMSB, BitStream<>::f...
#include "io/Buffer.h"     // for Buffer
#include "io/ByteStream.h" // for ByteStream
#include <algorithm>       // for max, fill, fill_n, copy
#include <cassert>         // for assert
#include <cstdint>         // for uint8_t, uint32_t, uint16_t
#include <vector>          // for vector

namespace rawspeed {

// 16 entries of codes per bit length
// 13 entries of code values
const std::array<std::array<std::array<uint8_t, 16>, 2>, 1>
    PentaxDecompressor::pentax_tree = {{
        {{{0, 2, 3, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
          {3, 4, 2, 5, 1, 6, 0, 7, 8, 9, 10, 11, 12}}},
    }};

PentaxDecompressor::PentaxDecompressor(const RawImage& img,
                                       std::optional<ByteStream> metaData)
    : mRaw(img), ht(SetupPrefixCodeDecoder(metaData)) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.x || !mRaw->dim.y || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 8384 || mRaw->dim.y > 6208) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }
}

HuffmanCode<BaselineCodeTag>
PentaxDecompressor::SetupPrefixCodeDecoder_Legacy() {
  // Temporary table, used during parsing LJpeg.
  HuffmanCode<BaselineCodeTag> hc;

  /* Initialize with legacy data */
  auto nCodes = hc.setNCodesPerLength(Buffer(pentax_tree[0][0].data(), 16));
  invariant(nCodes == 13); // see pentax_tree definition
  hc.setCodeValues(Array1DRef<const uint8_t>(pentax_tree[0][1].data(), nCodes));

  return hc;
}

HuffmanCode<BaselineCodeTag>
PentaxDecompressor::SetupPrefixCodeDecoder_Modern(ByteStream stream) {
  // Temporary table, used during parsing LJpeg.
  HuffmanCode<BaselineCodeTag> hc;

  const uint32_t depth = stream.getU16() + 12;
  if (depth > 15)
    ThrowRDE("Depth of huffman table is too great (%u).", depth);

  stream.skipBytes(12);

  std::array<uint32_t, 16> v0;
  std::array<uint32_t, 16> v1;
  for (uint32_t i = 0; i < depth; i++)
    v0[i] = stream.getU16();
  for (uint32_t i = 0; i < depth; i++) {
    v1[i] = stream.getByte();

    if (v1[i] == 0 || v1[i] > 12)
      ThrowRDE("Data corrupt: v1[%i]=%i, expected [1..12]", depth, v1[i]);
  }

  std::vector<uint8_t> nCodesPerLength;
  nCodesPerLength.resize(17);

  std::array<uint32_t, 16> v2;
  /* Calculate codes and store bitcounts */
  for (uint32_t c = 0; c < depth; c++) {
    v2[c] = extractHighBits(v0[c], v1[c], /*effectiveBitwidth=*/12);
    nCodesPerLength.at(v1[c])++;
  }

  assert(nCodesPerLength.size() == 17);
  assert(nCodesPerLength[0] == 0);
  auto nCodes = hc.setNCodesPerLength(Buffer(&nCodesPerLength[1], 16));
  invariant(nCodes == depth);

  std::vector<uint8_t> codeValues;
  codeValues.reserve(nCodes);

  /* Find smallest */
  for (uint32_t i = 0; i < depth; i++) {
    uint32_t sm_val = 0xfffffff;
    uint32_t sm_num = 0xff;
    for (uint32_t j = 0; j < depth; j++) {
      if (v2[j] <= sm_val) {
        sm_num = j;
        sm_val = v2[j];
      }
    }
    invariant(sm_num < 16);
    codeValues.push_back(sm_num);
    v2[sm_num] = 0xffffffff;
  }

  assert(codeValues.size() == nCodes);
  hc.setCodeValues(Array1DRef<const uint8_t>(codeValues.data(), nCodes));

  return hc;
}

PrefixCodeDecoder<>
PentaxDecompressor::SetupPrefixCodeDecoder(std::optional<ByteStream> metaData) {
  std::optional<HuffmanCode<BaselineCodeTag>> hc;

  if (metaData)
    hc = SetupPrefixCodeDecoder_Modern(*metaData);
  else
    hc = SetupPrefixCodeDecoder_Legacy();

  PrefixCodeDecoder<> ht(std::move(*hc));
  ht.setup(true, false);

  return ht;
}

void PentaxDecompressor::decompress(ByteStream data) const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  invariant(out.height > 0);
  invariant(out.width > 0);
  invariant(out.width % 2 == 0);

  BitPumpMSB bs(data);
  for (int row = 0; row < out.height; row++) {
    std::array<int, 2> pred = {{}};
    if (row >= 2)
      pred = {out(row - 2, 0), out(row - 2, 1)};

    for (int col = 0; col < out.width; col++) {
      pred[col & 1] += ht.decodeDifference(bs);
      int value = pred[col & 1];
      if (!isIntN(value, 16))
        ThrowRDE("decoded value out of bounds at %d:%d", col, row);
      out(row, col) = value;
    }
  }
}

} // namespace rawspeed
