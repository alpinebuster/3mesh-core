// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspacePlatform.h"


namespace GS
{

struct GRADIENTSPACECORE_API GSTag
{
	uint32_t Type = 0;
	uint32_t Value = 0;

	bool operator==(const GSTag& OtherTag) const
	{
		return Type == OtherTag.Type && Value == OtherTag.Value;
	}
	static GSTag EmptyTag() { return GSTag(); }
};


#if defined(_MSC_VER)
#pragma warning(push)
// C4587/C4588: anonymous struct member's constructor/destructor no longer implicitly called.
// We initialize the Tags[] arm of the union instead, which aliases the same storage — intended.
#pragma warning(disable: 4587)
#pragma warning(disable: 4588)
#endif

struct GRADIENTSPACECORE_API GSTagSet
{
	union {
		struct {
			GSTag A;
			GSTag B;
			GSTag C;
			GSTag D;
		};
		GSTag Tags[4];
	};

	int NumTags;

	GSTagSet() : Tags{ GSTag(), GSTag(), GSTag(), GSTag() }, NumTags(0) {}

	bool AddTag(GSTag Tag, bool bUniqueType = false)
	{
		if (NumTags >= 4) return false;
		if (bUniqueType && ContainsTagOfType(Tag)) return false;
		Tags[NumTags] = Tag;
		NumTags++;
		return true;
	}


	bool ContainsTagOfType(GSTag Tag) const
	{
		for (int j = 0; j < NumTags; ++j)
			if (Tags[j].Type == Tag.Type)
				return true;
		return false;
	}

	const GSTag* FindTagOfType(uint32_t Type) const
	{
		for (int j = 0; j < NumTags; ++j)
			if (Tags[j].Type == Type)
				return &Tags[j];
		return nullptr;
	}

};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

}
