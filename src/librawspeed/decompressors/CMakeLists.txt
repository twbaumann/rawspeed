FILE(GLOB SOURCES
  "AbstractDecompressor.h"
  "AbstractDngDecompressor.cpp"
  "AbstractDngDecompressor.h"
  "AbstractLJpegDecoder.cpp"
  "AbstractLJpegDecoder.h"
  "AbstractPrefixCode.h"
  "AbstractPrefixCodeDecoder.h"
  "AbstractSamsungDecompressor.h"
  "BinaryPrefixTree.h"
  "Cr2Decompressor.cpp"
  "Cr2Decompressor.h"
  "Cr2LJpegDecoder.cpp"
  "Cr2LJpegDecoder.h"
  "CrwDecompressor.cpp"
  "CrwDecompressor.h"
  "DeflateDecompressor.cpp"
  "DeflateDecompressor.h"
  "DummyPrefixCodeDecoder.h"
  "FujiDecompressor.cpp"
  "FujiDecompressor.h"
  "HasselbladDecompressor.cpp"
  "HasselbladDecompressor.h"
  "HasselbladLJpegDecoder.cpp"
  "HasselbladLJpegDecoder.h"
  "HuffmanCode.h"
  "JpegDecompressor.cpp"
  "JpegDecompressor.h"
  "KodakDecompressor.cpp"
  "KodakDecompressor.h"
  "LJpegDecoder.cpp"
  "LJpegDecoder.h"
  "LJpegDecompressor.cpp"
  "LJpegDecompressor.h"
  "NikonDecompressor.cpp"
  "NikonDecompressor.h"
  "OlympusDecompressor.cpp"
  "OlympusDecompressor.h"
  "PanasonicV4Decompressor.cpp"
  "PanasonicV4Decompressor.h"
  "PanasonicV5Decompressor.cpp"
  "PanasonicV5Decompressor.h"
  "PanasonicV6Decompressor.cpp"
  "PanasonicV6Decompressor.h"
  "PanasonicV7Decompressor.cpp"
  "PanasonicV7Decompressor.h"
  "PentaxDecompressor.cpp"
  "PentaxDecompressor.h"
  "PhaseOneDecompressor.cpp"
  "PhaseOneDecompressor.h"
  "PrefixCode.h"
  "PrefixCodeDecoder.h"
  "PrefixCodeLUTDecoder.h"
  "PrefixCodeLookupDecoder.h"
  "PrefixCodeTreeDecoder.h"
  "PrefixCodeVectorDecoder.h"
  "SamsungV0Decompressor.cpp"
  "SamsungV0Decompressor.h"
  "SamsungV1Decompressor.cpp"
  "SamsungV1Decompressor.h"
  "SamsungV2Decompressor.cpp"
  "SamsungV2Decompressor.h"
  "SonyArw1Decompressor.cpp"
  "SonyArw1Decompressor.h"
  "SonyArw2Decompressor.cpp"
  "SonyArw2Decompressor.h"
  "UncompressedDecompressor.cpp"
  "UncompressedDecompressor.h"
  "VC5Decompressor.cpp"
  "VC5Decompressor.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
