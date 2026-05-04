// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspacePlatform.h"
#include "Core/unsafe_vector.h"
#include "Math/GSIntVector2.h"
#include "Math/GSVector4.h"
#include "Color/GSIntColor4.h"

namespace GS
{

// Image Pixel Formats (note: not actually used yet)
enum class EGSPixelFormat : uint8_t
{
	Invalid = 0,
	G8      = 1,    // 8-bit grayscale
	BGRA8   = 2,    // 8-bit BGRA (unreal FColor)
	RGBA8	= 3,    // 8-bit RGBA
	G16     = 4,    // 16-bit grayscale
	RGBA16  = 5,    // 16-bit per channel
	RGBA16F = 6,    // half-float per channel
	RGBA32F = 7,    // float per channel
	G16F    = 8,    // half-float grayscale
	G32F    = 9,    // float grayscale
};

// Result of initializing an image buffer with a requested EGSPixelFormat
enum class EImageInitResult : uint8_t
{
	Ok                = 0,    // requested Format matches buffer storage exactly
	Converted         = 1,    // requested Format is compatible but interpretation differs (e.g. G8 -> RGBA8 expanded, BGRA channel swap, dropped alpha)
	InvalidConversion = 2,    // requested Format cannot be stored in this buffer (e.g. RGBA in 1-channel, mismatched bit depth)
	InvalidDimensions = 3,    // Width or Height was zero (or negative)
};


template<typename RealPixelType, typename RealChannelType, int NumChannels>
class TRealImageBuffer
{
public:
	Vector2i Dimensions;
	unsafe_vector<RealPixelType> Pixels;

	EGSPixelFormat Format = EGSPixelFormat::RGBA32F;

	TRealImageBuffer() = default;
	TRealImageBuffer(const TRealImageBuffer&) = default;
	TRealImageBuffer(TRealImageBuffer&&) noexcept = default;
	TRealImageBuffer& operator=(const TRealImageBuffer&) = default;
	TRealImageBuffer& operator=(TRealImageBuffer&&) noexcept = default;

	// todo: need to be in .cpp file
	~TRealImageBuffer();

	EImageInitResult Initialize(int Width, int Height, EGSPixelFormat InFormat);

	template<typename GetColorFuncT>  /*RealPixelType GetColorFuncT(int64 LinearIndex)*/
	EImageInitResult InitializeFuncLinear(
		int Width, int Height,
		EGSPixelFormat InFormat,
		GetColorFuncT GetColorFunc)
	{
		EImageInitResult Result = Initialize(Width, Height, InFormat);
		if (Result == EImageInitResult::InvalidConversion || Result == EImageInitResult::InvalidDimensions)
			return Result;
		int64_t TotalPixels = (int64_t)Width * (int64_t)Height;
		for (int64_t k = 0; k < TotalPixels; ++k)
			Pixels[k] = GetColorFunc(k);
		return Result;
	}

	template<typename GetColorFuncT>  /*RealPixelType GetColorFuncT(int X, int Y)*/
	EImageInitResult InitializeFuncCoord(
		int Width, int Height,
		EGSPixelFormat InFormat,
		GetColorFuncT GetColorFunc)
	{
		EImageInitResult Result = Initialize(Width, Height, InFormat);
		if (Result == EImageInitResult::InvalidConversion || Result == EImageInitResult::InvalidDimensions)
			return Result;
		int64_t LinearIndex = 0;
		for (int y = 0; y < Height; ++y)
			for (int x = 0; x < Width; ++x)
				Pixels[LinearIndex++] = GetColorFunc(x, y);
		return Result;
	}

	bool bIsInitialized() const { return Dimensions.X > 0 && Dimensions.Y > 0; }
	int Width() const { return Dimensions.X; }
	int Height() const { return Dimensions.Y; }
	int64_t NumPixels() const { return Pixels.size(); }

	int64_t ToLinearIndex(Vector2i PixelCoords) const {
		return (int64_t)(PixelCoords.Y * Dimensions.X) + (int64_t)PixelCoords.X;
	}
	Vector2i ToPixelCoords(int64_t LinearIndex) const {
		int64_t y = (LinearIndex / Dimensions.X);
		int64_t x = LinearIndex - (y * Dimensions.X);
		return Vector2i((int)x, (int)y);
	}

	const RealPixelType& GetPixel(int64_t LinearIndex) const {
		return Pixels[LinearIndex];
	}
	const RealPixelType& GetPixel(const Vector2i& PixelCoords) const {
		int64_t LinearIndex = (int64_t)(PixelCoords.Y * Dimensions.X) + (int64_t)PixelCoords.X;
		return Pixels[LinearIndex];
	}


	void SetPixel(int64_t LinearIndex, const RealPixelType& NewLinearColor) {
		Pixels[LinearIndex] = NewLinearColor;
	}
	void SetPixel(const Vector2i& PixelCoords, const RealPixelType& NewLinearColor) {
		int64_t LinearIndex = (int64_t)(PixelCoords.Y * Dimensions.X) + (int64_t)PixelCoords.X;
		Pixels[LinearIndex] = NewLinearColor;
	}
};

// standard image types
typedef TRealImageBuffer<Vector4f, float, 4> Image4f;
typedef TRealImageBuffer<Vector3f, float, 3> Image3f;
typedef TRealImageBuffer<float, float, 1> Image1f;

// explicit instantiation
extern template class TRealImageBuffer<Vector4f, float, 4>;
extern template class TRealImageBuffer<Vector3f, float, 3>;
extern template class TRealImageBuffer<float, float, 1>;


template<typename BytePixelType, typename ByteChannelType, int NumChannels>
class TByteImageBuffer
{
public:
	Vector2i Dimensions;
	unsafe_vector<BytePixelType> Pixels;

	EGSPixelFormat Format = EGSPixelFormat::RGBA8;
	bool bIsSRGB = false;

	TByteImageBuffer() = default;
	TByteImageBuffer(const TByteImageBuffer&) = default;
	TByteImageBuffer(TByteImageBuffer&&) noexcept = default;
	TByteImageBuffer& operator=(const TByteImageBuffer&) = default;
	TByteImageBuffer& operator=(TByteImageBuffer&&) noexcept = default;

	// todo: need to be in .cpp file
	~TByteImageBuffer();

	EImageInitResult Initialize(int Width, int Height, EGSPixelFormat InFormat, bool InIsSRGB);

	template<typename GetColorFuncT>  /*BytePixelType GetColorFuncT(int64 LinearIndex)*/
	EImageInitResult InitializeFuncLinear(
		int Width, int Height,
		EGSPixelFormat InFormat, bool InIsSRGB,
		GetColorFuncT GetColorFunc)
	{
		EImageInitResult Result = Initialize(Width, Height, InFormat, InIsSRGB);
		if (Result == EImageInitResult::InvalidConversion || Result == EImageInitResult::InvalidDimensions)
			return Result;
		int64_t TotalPixels = (int64_t)Width * (int64_t)Height;
		for (int64_t k = 0; k < TotalPixels; ++k)
			Pixels[k] = GetColorFunc(k);
		return Result;
	}

	template<typename GetColorFuncT>  /*BytePixelType GetColorFuncT(int X, int Y)*/
	EImageInitResult InitializeFuncCoord(
		int Width, int Height,
		EGSPixelFormat InFormat, bool InIsSRGB,
		GetColorFuncT GetColorFunc)
	{
		EImageInitResult Result = Initialize(Width, Height, InFormat, InIsSRGB);
		if (Result == EImageInitResult::InvalidConversion || Result == EImageInitResult::InvalidDimensions)
			return Result;
		int64_t LinearIndex = 0;
		for (int y = 0; y < Height; ++y)
			for (int x = 0; x < Width; ++x)
				Pixels[LinearIndex++] = GetColorFunc(x, y);
		return Result;
	}

	bool bIsInitialized() const { return Dimensions.X > 0 && Dimensions.Y > 0; }
	int Width() const { return Dimensions.X; }
	int Height() const { return Dimensions.Y; }
	int64_t NumPixels() const { return Pixels.size(); }

	int64_t ToLinearIndex(Vector2i PixelCoords) const {
		return (int64_t)(PixelCoords.Y * Dimensions.X) + (int64_t)PixelCoords.X;
	}
	Vector2i ToPixelCoords(int64_t LinearIndex) const {
		int64_t y = (LinearIndex / Dimensions.X);
		int64_t x = LinearIndex - (y * Dimensions.X);
		return Vector2i((int)x, (int)y);
	}

	const BytePixelType& GetPixel(int64_t LinearIndex) const {
		return Pixels[LinearIndex];
	}
	const BytePixelType& GetPixel(const Vector2i& PixelCoords) const {
		int64_t LinearIndex = (int64_t)(PixelCoords.Y * Dimensions.X) + (int64_t)PixelCoords.X;
		return Pixels[LinearIndex];
	}


	void SetPixel(int64_t LinearIndex, const BytePixelType& NewColor) {
		Pixels[LinearIndex] = NewColor;
	}
	void SetPixel(const Vector2i& PixelCoords, const BytePixelType& NewColor) {
		int64_t LinearIndex = (int64_t)(PixelCoords.Y * Dimensions.X) + (int64_t)PixelCoords.X;
		Pixels[LinearIndex] = NewColor;
	}
};

// standard byte image types
typedef TByteImageBuffer<Color4b, uint8_t, 4> Image4b;
typedef TByteImageBuffer<uint8_t, uint8_t, 1> Image1b;

// explicit instantiation
extern template class TByteImageBuffer<Color4b, uint8_t, 4>;
extern template class TByteImageBuffer<uint8_t, uint8_t, 1>;



}