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

#include "decompressors/Cr2Decompressor.h"     // for Cr2Decompressor
#include "decompressors/Cr2DecompressorImpl.h" // IWYU pragma: keep
#include "decompressors/PrefixCodeDecoder.h"   // for PrefixCodeDecoder

namespace rawspeed {

template class Cr2Decompressor<PrefixCodeDecoder<>>;

} // namespace rawspeed
