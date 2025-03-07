/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017 Roman Lebedev

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

#include "decompressors/AbstractLJpegDecoder.h"
#include "adt/Array1DRef.h"                  // for Array1DRef
#include "adt/Invariant.h"                   // for invariant
#include "adt/Point.h"                       // for iPoint2D
#include "common/RawspeedException.h"        // for ThrowException
#include "decoders/RawDecoderException.h"    // for ThrowRDE
#include "decompressors/HuffmanCode.h"       // for HuffmanCode
#include "decompressors/PrefixCodeDecoder.h" // for PrefixCodeDecoder, Huffma...
#include "io/Buffer.h"                       // for Buffer
#include "io/ByteStream.h"                   // for ByteStream
#include "io/Endianness.h"                   // for Endianness, Endianne...
#include <array>                             // for array
#include <memory>                            // for unique_ptr, make_unique
#include <optional>                          // for optional
#include <utility>                           // for move
#include <vector>                            // for vector

namespace rawspeed {

AbstractLJpegDecoder::AbstractLJpegDecoder(ByteStream bs, const RawImage& img)
    : input(bs), mRaw(img) {
  input.setByteOrder(Endianness::big);

  if (mRaw->dim.x == 0 || mRaw->dim.y == 0)
    ThrowRDE("Image has zero size");

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  // Yeah, sure, here it would be just dumb to leave this for production :)
  if (mRaw->dim.x > 19440 || mRaw->dim.y > 6304) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }
#endif
}

void AbstractLJpegDecoder::decodeSOI() {
  if (getNextMarker(false) != JpegMarker::SOI)
    ThrowRDE("Image did not start with SOI. Probably not an LJPEG");

  struct {
    bool DRI = false;
    bool DHT = false;
    bool SOF = false;
    bool SOS = false;
  } FoundMarkers;

  JpegMarker m;
  do {
    m = getNextMarker(true);

    if (m == JpegMarker::EOI)
      break;

    ByteStream data(input.getStream(input.peekU16()));
    data.skipBytes(2); // headerLength

    switch (m) {
    case JpegMarker::DHT:
      if (FoundMarkers.SOS)
        ThrowRDE("Found second DHT marker after SOS");
      // there can be more than one DHT markers.
      // FIXME: do we really want to reparse and use the last one?
      parseDHT(data);
      FoundMarkers.DHT = true;
      break;
    case JpegMarker::SOF3:
      if (FoundMarkers.SOS)
        ThrowRDE("Found second SOF marker after SOS");
      if (FoundMarkers.SOF)
        ThrowRDE("Found second SOF marker");
      // SOF is not required to be after DHT
      parseSOF(data, &frame);
      FoundMarkers.SOF = true;
      break;
    case JpegMarker::SOS:
      if (FoundMarkers.SOS)
        ThrowRDE("Found second SOS marker");
      if (!FoundMarkers.DHT)
        ThrowRDE("Did not find DHT marker before SOS.");
      if (!FoundMarkers.SOF)
        ThrowRDE("Did not find SOF marker before SOS.");
      parseSOS(data);
      FoundMarkers.SOS = true;
      break;
    case JpegMarker::DQT:
      ThrowRDE("Not a valid RAW file.");
    case JpegMarker::DRI:
      if (FoundMarkers.DRI)
        ThrowRDE("Found second DRI marker");
      parseDRI(data);
      FoundMarkers.DRI = true;
      break;
    default: // Just let it skip to next marker
      break;
    }
  } while (m != JpegMarker::EOI);

  if (!FoundMarkers.SOS)
    ThrowRDE("Did not find SOS marker.");
}

void AbstractLJpegDecoder::parseSOF(ByteStream sofInput, SOFInfo* sof) {
  sof->prec = sofInput.getByte();
  sof->h = sofInput.getU16();
  sof->w = sofInput.getU16();
  sof->cps = sofInput.getByte();

  if (sof->prec < 2 || sof->prec > 16)
    ThrowRDE("Invalid precision (%u).", sof->prec);

  if (sof->h == 0 || sof->w == 0)
    ThrowRDE("Frame width or height set to zero");

  if (sof->cps > 4 || sof->cps < 1)
    ThrowRDE("Only from 1 to 4 components are supported.");

  if (sof->cps < mRaw->getCpp()) {
    ThrowRDE("Component count should be no less than sample count (%u vs %u).",
             sof->cps, mRaw->getCpp());
  }

  if (sof->cps > static_cast<uint32_t>(mRaw->dim.x)) {
    ThrowRDE("Component count should be no greater than row length (%u vs %u).",
             sof->cps, mRaw->dim.x);
  }

  if (sofInput.getRemainSize() != 3 * sof->cps)
    ThrowRDE("Header size mismatch.");

  for (uint32_t i = 0; i < sof->cps; i++) {
    sof->compInfo[i].componentId = sofInput.getByte();

    uint32_t subs = sofInput.getByte();
    frame.compInfo[i].superV = subs & 0xf;
    frame.compInfo[i].superH = subs >> 4;

    if (frame.compInfo[i].superV < 1 || frame.compInfo[i].superV > 4)
      ThrowRDE("Horizontal sampling factor is invalid.");

    if (frame.compInfo[i].superH < 1 || frame.compInfo[i].superH > 4)
      ThrowRDE("Horizontal sampling factor is invalid.");

    uint32_t Tq = sofInput.getByte();
    if (Tq != 0)
      ThrowRDE("Quantized components not supported.");
  }

  if (static_cast<int>(sof->compInfo[0].superH) !=
          mRaw->metadata.subsampling.x ||
      static_cast<int>(sof->compInfo[0].superV) != mRaw->metadata.subsampling.y)
    ThrowRDE("LJpeg's subsampling does not match image's subsampling.");

  sof->initialized = true;
}

void AbstractLJpegDecoder::parseSOS(ByteStream sos) {
  invariant(frame.initialized);

  if (sos.getRemainSize() != 1 + 2 * frame.cps + 3)
    ThrowRDE("Invalid SOS header length.");

  if (uint32_t soscps = sos.getByte(); frame.cps != soscps)
    ThrowRDE("Component number mismatch.");

  for (uint32_t i = 0; i < frame.cps; i++) {
    uint32_t cs = sos.getByte();
    uint32_t td = sos.getByte() >> 4;

    if (td >= huff.size() || !huff[td])
      ThrowRDE("Invalid Huffman table selection.");

    int ciIndex = -1;
    for (uint32_t j = 0; j < frame.cps; ++j) {
      if (frame.compInfo[j].componentId == cs)
        ciIndex = j;
    }

    if (ciIndex == -1)
      ThrowRDE("Invalid Component Selector");

    frame.compInfo[ciIndex].dcTblNo = td;
  }

  // Get predictor, see table H.1 from the JPEG spec
  predictorMode = sos.getByte();
  // The spec says predictoreMode is in [0..7], but Hasselblad uses '8'.
  if (predictorMode > 8)
    ThrowRDE("Invalid predictor mode.");

  // Se + Ah Not used in LJPEG
  if (sos.getByte() != 0)
    ThrowRDE("Se/Ah not zero.");

  Pt = sos.getByte(); // Point Transform
  if (Pt > 15)
    ThrowRDE("Invalid Point transform.");

  decodeScan();
}

void AbstractLJpegDecoder::parseDHT(ByteStream dht) {
  while (dht.getRemainSize() > 0) {
    uint32_t b = dht.getByte();

    if (uint32_t htClass = b >> 4; htClass != 0)
      ThrowRDE("Unsupported Table class.");

    uint32_t htIndex = b & 0xf;
    if (htIndex >= huff.size())
      ThrowRDE("Invalid huffman table destination id.");

    if (huff[htIndex] != nullptr)
      ThrowRDE("Duplicate table definition");

    // Temporary table, used during parsing LJpeg.
    HuffmanCode<BaselineCodeTag> hc;

    // copy 16 bytes from input stream to number of codes per length table
    uint32_t nCodes = hc.setNCodesPerLength(dht.getBuffer(16));

    // spec says 16 different codes is max but Hasselblad violates that -> 17
    if (nCodes > 17)
      ThrowRDE("Invalid DHT table.");

    // copy nCodes bytes from input stream to code values table
    const auto codesBuf = dht.getBuffer(nCodes);
    hc.setCodeValues(
        Array1DRef<const uint8_t>(codesBuf.begin(), codesBuf.getSize()));

    // see if we already have a PrefixCodeDecoder with the same codes
    assert(PrefixCodeDecoderStore.size() == huffmanCodeStore.size());
    for (unsigned index = 0; index != PrefixCodeDecoderStore.size(); ++index) {
      if (*huffmanCodeStore[index] == hc)
        huff[htIndex] = PrefixCodeDecoderStore[index].get();
    }

    if (!huff[htIndex]) {
      huffmanCodeStore.emplace_back(std::make_unique<decltype(hc)>(hc));
      // setup new hc and put it into the store
      auto dHT = std::make_unique<PrefixCodeDecoder<>>(std::move(hc));
      dHT->setup(fullDecodeHT, fixDng16Bug);
      huff[htIndex] = dHT.get();
      PrefixCodeDecoderStore.emplace_back(std::move(dHT));
    }
  }
}

void AbstractLJpegDecoder::parseDRI(ByteStream dri) {
  if (dri.getRemainSize() != 2)
    ThrowRDE("Invalid DRI header length.");
  if (uint16_t Ri = dri.getU16(); Ri != 0)
    ThrowRDE("Non-zero restart interval not supported.");
}

JpegMarker AbstractLJpegDecoder::getNextMarker(bool allowskip) {
  auto peekMarker = [&]() -> std::optional<JpegMarker> {
    uint8_t c0 = input.peekByte(0);
    uint8_t c1 = input.peekByte(1);

    if (c0 == 0xFF && c1 != 0 && c1 != 0xFF)
      return static_cast<JpegMarker>(c1);
    return {};
  };

  while (input.getRemainSize() >= 2) {
    if (std::optional<JpegMarker> m = peekMarker()) {
      input.skipBytes(2); // Skip the bytes we've just consumed.
      return *m;
    }
    // Marker not found. Might there be leading padding bytes?
    if (!allowskip)
      break; // Nope, give up.
    // Advance by a single(!) byte and try again.
    input.skipBytes(1);
  }

  ThrowRDE("(Noskip) Expected marker not found. Probably corrupt file.");
}

} // namespace rawspeed
