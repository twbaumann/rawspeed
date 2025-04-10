/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017-2018 Roman Lebeedv

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

#include "adt/Invariant.h"                      // for invariant
#include "adt/Point.h"                          // for iPoint2D
#include "common/Common.h"                      // for roundUpDivision
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/ByteStream.h"                      // for ByteStream
#include <cstdint>                              // for uint32_t
#include <utility>                              // for move
#include <vector>                               // for vector

namespace rawspeed {

struct DngTilingDescription final {
  // The dimensions of the whole image.
  const iPoint2D& dim;

  // How many horizontal pixels does one tile represent?
  const uint32_t tileW;

  // How many vertical pixels does one tile represent?
  const uint32_t tileH;

  // How many tiles per row is there?
  const uint32_t tilesX;

  // How many rows is there?
  const uint32_t tilesY;

  // How many tiles are there total?
  const unsigned numTiles;

  DngTilingDescription(const iPoint2D& dim_, uint32_t tileW_, uint32_t tileH_)
      : dim(dim_), tileW(tileW_), tileH(tileH_),
        tilesX(roundUpDivision(dim.x, tileW)),
        tilesY(roundUpDivision(dim.y, tileH)), numTiles(tilesX * tilesY) {
    invariant(dim.area() > 0);
    invariant(tileW > 0);
    invariant(tileH > 0);
    invariant(tilesX > 0);
    invariant(tilesY > 0);
    invariant(tileW * tilesX >= static_cast<unsigned>(dim.x));
    invariant(tileH * tilesY >= static_cast<unsigned>(dim.y));
    invariant(tileW * (tilesX - 1) < static_cast<unsigned>(dim.x));
    invariant(tileH * (tilesY - 1) < static_cast<unsigned>(dim.y));
    invariant(numTiles > 0);
  }
};

struct DngSliceElement final {
  const DngTilingDescription& dsc;

  // Which slice is this?
  const unsigned n;

  // The actual data of the tile.
  const ByteStream bs;

  // Which tile is this?
  const unsigned column;
  const unsigned row;

  const bool lastColumn;
  const bool lastRow;

  // Where does it start?
  const unsigned offX;
  const unsigned offY;

  // What's it's actual size?
  const unsigned width;
  const unsigned height;

  DngSliceElement() = delete;
  DngSliceElement(const DngSliceElement&) = default;
  DngSliceElement(DngSliceElement&&) noexcept = default;
  DngSliceElement& operator=(const DngSliceElement&) noexcept = delete;
  DngSliceElement& operator=(DngSliceElement&&) noexcept = delete;

  DngSliceElement(const DngTilingDescription& dsc_, unsigned n_, ByteStream bs_)
      : dsc(dsc_), n(n_), bs(bs_), column(n % dsc.tilesX), row(n / dsc.tilesX),
        lastColumn((column + 1) == dsc.tilesX),
        lastRow((row + 1) == dsc.tilesY), offX(dsc.tileW * column),
        offY(dsc.tileH * row),
        width(!lastColumn ? dsc.tileW : dsc.dim.x - offX),
        height(!lastRow ? dsc.tileH : dsc.dim.y - offY) {
    invariant(n < dsc.numTiles);
    invariant(bs.getRemainSize() > 0);
    invariant(column < dsc.tilesX);
    invariant(row < dsc.tilesY);
    invariant(offX < static_cast<unsigned>(dsc.dim.x));
    invariant(offY < static_cast<unsigned>(dsc.dim.y));
    invariant(width > 0);
    invariant(height > 0);
    invariant(offX + width <= static_cast<unsigned>(dsc.dim.x));
    invariant(offY + height <= static_cast<unsigned>(dsc.dim.y));
    invariant(!lastColumn ||
              (offX + width == static_cast<unsigned>(dsc.dim.x)));
    invariant(!lastRow || (offY + height == static_cast<unsigned>(dsc.dim.y)));
  }
};

class AbstractDngDecompressor final : public AbstractDecompressor {
  RawImage mRaw;

  template <int compression> void decompressThread() const noexcept;

  void decompressThread() const noexcept;

public:
  AbstractDngDecompressor(const RawImage& img, const DngTilingDescription& dsc_,
                          int compression_, bool mFixLjpeg_, uint32_t mBps_,
                          uint32_t mPredictor_)
      : mRaw(img), dsc(dsc_), compression(compression_), mFixLjpeg(mFixLjpeg_),
        mBps(mBps_), mPredictor(mPredictor_) {}

  void decompress() const;

  const DngTilingDescription dsc;

  std::vector<DngSliceElement> slices;

  const int compression;
  const bool mFixLjpeg = false;
  const uint32_t mBps;
  const uint32_t mPredictor;
};

} // namespace rawspeed
