// Copyright Gradientspace Corp. All Rights Reserved.
#include "Image/GSImage.h"

#include <type_traits>

using namespace GS;

namespace {

struct FormatProps
{
	int Channels = 0;     // 0 == invalid
	int BitDepth = 0;     // 8, 16, 32
	bool bIsFloat = false;
	bool bIsBGR = false;  // alternate channel order vs RGBA
};

static FormatProps GetFormatProps(EGSPixelFormat F)
{
	switch (F)
	{
		case EGSPixelFormat::G8:      return { 1,  8, false, false };
		case EGSPixelFormat::BGRA8:   return { 4,  8, false, true  };
		case EGSPixelFormat::RGBA8:   return { 4,  8, false, false };
		case EGSPixelFormat::G16:     return { 1, 16, false, false };
		case EGSPixelFormat::RGBA16:  return { 4, 16, false, false };
		case EGSPixelFormat::RGBA16F: return { 4, 16, true,  false };
		case EGSPixelFormat::RGBA32F: return { 4, 32, true,  false };
		case EGSPixelFormat::G16F:    return { 1, 16, true,  false };
		case EGSPixelFormat::G32F:    return { 1, 32, true,  false };
		default:                      return {};
	}
}

static EImageInitResult ClassifyFormat(EGSPixelFormat Format, int TargetChannels, int TargetBitDepth, bool TargetIsFloat)
{
	FormatProps P = GetFormatProps(Format);
	if (P.Channels == 0)
		return EImageInitResult::InvalidConversion;
	// underlying channel storage (bit depth + int/float) must match
	if (P.BitDepth != TargetBitDepth || P.bIsFloat != TargetIsFloat)
		return EImageInitResult::InvalidConversion;
	if (P.Channels == TargetChannels)
		return P.bIsBGR ? EImageInitResult::Converted : EImageInitResult::Ok;
	// single-channel source can be expanded into 3- or 4-channel target (G -> RGB[A])
	if (P.Channels == 1 && TargetChannels >= 3)
		return EImageInitResult::Converted;
	// 4-channel source can drop alpha into 3-channel target (RGBA -> RGB)
	if (P.Channels == 4 && TargetChannels == 3)
		return EImageInitResult::Converted;
	return EImageInitResult::InvalidConversion;
}

} // anonymous namespace


template<typename RealPixelType, typename RealChannelType, int NumChannels>
GS::TRealImageBuffer<RealPixelType, RealChannelType, NumChannels>::~TRealImageBuffer()
{
	Pixels.clear(true);
}

template<typename RealPixelType, typename RealChannelType, int NumChannels>
EImageInitResult GS::TRealImageBuffer<RealPixelType, RealChannelType, NumChannels>::Initialize(int Width, int Height, EGSPixelFormat InFormat)
{
	if (Width <= 0 || Height <= 0)
		return EImageInitResult::InvalidDimensions;

	constexpr int TargetBitDepth = (int)(sizeof(RealChannelType) * 8);
	constexpr bool TargetIsFloat = std::is_floating_point<RealChannelType>::value;
	EImageInitResult Result = ClassifyFormat(InFormat, NumChannels, TargetBitDepth, TargetIsFloat);
	if (Result == EImageInitResult::InvalidConversion)
		return Result;

	Dimensions = Vector2i(Width, Height);
	Pixels.resize((int64_t)Width * (int64_t)Height);
	Format = InFormat;
	return Result;
}

// explicit instantiation
template class GRADIENTSPACECORE_API GS::TRealImageBuffer<Vector4f, float, 4>;
template class GRADIENTSPACECORE_API GS::TRealImageBuffer<Vector3f, float, 3>;
template class GRADIENTSPACECORE_API GS::TRealImageBuffer<float, float, 1>;


template<typename BytePixelType, typename ByteChannelType, int NumChannels>
GS::TByteImageBuffer<BytePixelType, ByteChannelType, NumChannels>::~TByteImageBuffer()
{
	Pixels.clear(true);
}

template<typename BytePixelType, typename ByteChannelType, int NumChannels>
EImageInitResult GS::TByteImageBuffer<BytePixelType, ByteChannelType, NumChannels>::Initialize(int Width, int Height, EGSPixelFormat InFormat, bool InIsSRGB)
{
	if (Width <= 0 || Height <= 0)
		return EImageInitResult::InvalidDimensions;

	constexpr int TargetBitDepth = (int)(sizeof(ByteChannelType) * 8);
	constexpr bool TargetIsFloat = std::is_floating_point<ByteChannelType>::value;
	EImageInitResult Result = ClassifyFormat(InFormat, NumChannels, TargetBitDepth, TargetIsFloat);
	if (Result == EImageInitResult::InvalidConversion)
		return Result;

	Dimensions = Vector2i(Width, Height);
	Pixels.resize((int64_t)Width * (int64_t)Height);
	Format = InFormat;
	bIsSRGB = InIsSRGB;
	return Result;
}

// explicit instantiation
template class GRADIENTSPACECORE_API GS::TByteImageBuffer<Color4b, uint8_t, 4>;
template class GRADIENTSPACECORE_API GS::TByteImageBuffer<uint8_t, uint8_t, 1>;
