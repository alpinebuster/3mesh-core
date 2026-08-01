// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspacePlatform.h"
#include "Core/FunctionRef.h"
#include "Core/unsafe_vector.h"
#include "Math/GSVector2.h"
#include "Math/GSAxisBox2.h"
#include "Math/GSIndex2.h"
#include "Spatial/SpatialResult2.h"
#include "Spatial/AxisBoxTree2.h"		// for DistanceQueryOptions

namespace GS
{

//! AABBTree for a 2D points
template<typename RealType>
class PointSetTree2
{
public:
	using BoxType = typename GS::AxisBox2<RealType>;
	using PointType = typename GS::Vector2<RealType>;

	void Build(
		int32_t MaxPointID,
		FunctionRef<bool(int, Vector2<RealType>& Point)> GetPointFunc,
		int32_t CountHint = 0);

	//! find ElementID of nearest point to QueryPoint within Options.MaxDistance.
	//! Result.IsValid() is false if no point was within MaxDistance.
	DistanceResult2<RealType> NearestPointQuery(
		Vector2<RealType> QueryPoint,
		DistanceQueryOptions<RealType> Options = DistanceQueryOptions<RealType>() ) const;

	//! find up to K nearest points to QueryPoint within Options.MaxDistance.
	//! OutResults must point to storage for at least K elements.
	//! On return, OutResults[0..N-1] are sorted ascending by DistanceSqr, where N is the returned count.
	int NearestKPointQuery(
		Vector2<RealType> QueryPoint,
		int K,
		DistanceResult2<RealType>* OutResults,
		DistanceQueryOptions<RealType> Options = DistanceQueryOptions<RealType>() ) const;

	void Validate();

public:

	struct ChildIndex {
		uint32_t LeafCount : 4;		// max 16 elements in a leaf. If > 0, Index is a leaf-node-index
		uint32_t Index : 28;		// max 268M internal or leaf nodes
	};

	struct InteriorNode {
		ChildIndex LeftChild;
		ChildIndex RightChild;
		BoxType LeftBounds;
		BoxType RightBounds;
	};

	struct SourcePoint2
	{
		Vector2<RealType> Point;
		int32_t PointID;
	};

	ChildIndex RootIndex;
	BoxType RootBounds;
	unsafe_vector<InteriorNode> NodeTree;
	unsafe_vector<SourcePoint2> LeafPointLists;


private:
	void validate_leaf_child(ChildIndex Index, AxisBox2<RealType>& ComputedBounds);
	void validate_internal_node(ChildIndex Index, AxisBox2<RealType>& ComputedBounds);
};


typedef PointSetTree2<float> PointSetTree2f;
typedef PointSetTree2<double> PointSetTree2d;


// explicit instantiation
extern template class PointSetTree2<float>;
extern template class PointSetTree2<double>;

} // end namespace GS
