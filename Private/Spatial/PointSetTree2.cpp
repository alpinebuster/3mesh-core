// Copyright Gradientspace Corp. All Rights Reserved.
#include "Spatial/PointSetTree2.h"
#include "Mesh/MeshTypes.h"

using namespace GS;


template<typename RealType>
struct TSourcePoint
{
	Vector2<RealType> Point;
	int32_t PointID;
};

template<typename RealType>
struct TPointGroup
{
	AxisBox2<RealType> Bounds;
	int32_t StartIndex = -1;
	int32_t EndIndex = -1;		// inclusive
	Index2i Children = Index2i(-1, -1);
	int32_t Count() const { return EndIndex - StartIndex + 1; }
	bool IsLeaf() const { return Children.A == -1; }
};


template<typename RealType>
static int select_split_axis_longest_pt(const AxisBox2<RealType>& Box)
{
	RealType W = Box.WidthX();
	RealType H = Box.HeightY();
	return (W >= H) ? 0 : 1;
}


template<typename RealType>
static void split_group_trivial_pt(
	unsafe_vector<TSourcePoint<RealType>>& AllPoints,
	const TPointGroup<RealType>& Group,
	TPointGroup<RealType>& LeftChild,
	TPointGroup<RealType>& RightChild)
{
	gs_debug_assert(Group.Count() > 1);

	LeftChild.StartIndex = Group.StartIndex;
	LeftChild.EndIndex = Group.StartIndex + (Group.Count() / 2);
	LeftChild.Bounds = AxisBox2<RealType>::Empty();
	for (int32_t k = LeftChild.StartIndex; k <= LeftChild.EndIndex; ++k)
		LeftChild.Bounds.Contain(AllPoints[k].Point);
	LeftChild.Children = Index2i(-1, -1);

	RightChild.StartIndex = LeftChild.EndIndex + 1;
	RightChild.EndIndex = Group.EndIndex;
	RightChild.Bounds = AxisBox2<RealType>::Empty();
	for (int32_t k = RightChild.StartIndex; k <= RightChild.EndIndex; ++k)
		RightChild.Bounds.Contain(AllPoints[k].Point);
	RightChild.Children = Index2i(-1, -1);
}


template<typename RealType>
static bool split_group_by_midpoint_pt(
	unsafe_vector<TSourcePoint<RealType>>& AllPoints,
	const TPointGroup<RealType>& Group,
	int32_t split_axis,
	RealType split_axis_origin,
	TPointGroup<RealType>& LeftChild,
	TPointGroup<RealType>& RightChild)
{
	gs_debug_assert(Group.Count() > 1);

	int32_t l = Group.StartIndex;
	int32_t r = Group.EndIndex;

	// walk from left to right, finding elements to swap
	while (l < r)
	{
		// if current left is <= splitpoint, step forward
		RealType lc = AllPoints[l].Point[split_axis];
		if (lc <= split_axis_origin) {
			l++;
			continue;
		}

		// ok at this point l is a point on the wrong side of the split

		while (r > l)
		{
			// if current right is > splitpoint, step to next
			RealType rc = AllPoints[r].Point[split_axis];
			if (rc > split_axis_origin) {
				r--;
				continue;
			} else
				break;	// ok now point r is on the wrong side too, or we hit r == l
		}

		if (r == l)		// hit the midpoint, must be sorted
			break;

		GS::SwapTemp(AllPoints[l], AllPoints[r]);
		l++;
		r--;
	}

	if (l != r) {	// this can happen if we did a final swap right at midpoint, then l == r+1. just swap back
		GS::SwapTemp(l, r);
	}
	else {			// l == r, need to make sure midpoint is on the right side
		Vector2<RealType> C = AllPoints[l].Point;
		if (C[split_axis] > split_axis_origin)
			l--;
	}

	// if all points ended up on the same side, fall back to trivial midpoint split
	if (l == Group.StartIndex || r == Group.EndIndex)
	{
		split_group_trivial_pt(AllPoints, Group, LeftChild, RightChild);
		return true;
	}

	LeftChild.Bounds = AxisBox2<RealType>::Empty();
	for (int k = Group.StartIndex; k <= l; ++k) {
		LeftChild.Bounds.Contain(AllPoints[k].Point);
	}
	LeftChild.StartIndex = Group.StartIndex;
	LeftChild.EndIndex = l;
	LeftChild.Children = Index2i(-1, -1);

	RightChild.Bounds = AxisBox2<RealType>::Empty();
	for (int k = l + 1; k <= Group.EndIndex; ++k) {
		RightChild.Bounds.Contain(AllPoints[k].Point);
	}
	RightChild.StartIndex = l + 1;
	RightChild.EndIndex = Group.EndIndex;
	RightChild.Children = Index2i(-1, -1);

	return true;
}


template<typename RealType>
void GS::PointSetTree2<RealType>::Build(int32_t MaxPointID, FunctionRef<bool(int PointID, Vector2<RealType>& Point)> GetPointFunc, int32_t CountHint)
{
	static_assert(sizeof(ChildIndex) == sizeof(int32_t));

	using SourcePoint = TSourcePoint<RealType>;
	using PointGroup = TPointGroup<RealType>;

	BoxType CombinedBox = BoxType::Empty();
	unsafe_vector<SourcePoint> PointsList;
	PointsList.reserve((CountHint <= 0) ? MaxPointID : CountHint);
	for (int k = 0; k < MaxPointID; ++k) {
		SourcePoint NewPoint;
		if (GetPointFunc(k, NewPoint.Point)) {
			NewPoint.PointID = k;
			PointsList.add_ref(NewPoint);
			CombinedBox.Contain(NewPoint.Point);
		}
	}
	int32_t NumPoints = (int32_t)PointsList.size();


	int NumChildrenInLeaf = 4;

	// trivial case - all points fit in a single leaf
	if (NumPoints <= NumChildrenInLeaf) {
		LeafPointLists.reserve(NumPoints);
		for (int j = 0; j < NumPoints; j++) {
			SourcePoint& SrcPt = PointsList[j];
			[[maybe_unused]] int new_index = (int)LeafPointLists.add_ref(SourcePoint2{ SrcPt.Point, SrcPt.PointID });
		}
		this->RootBounds = CombinedBox;
		this->RootIndex.Index = 0;
		this->RootIndex.LeafCount = NumPoints;
		return;
	}


	unsafe_vector<PointGroup> GroupTree;
	GroupTree.reserve(NumPoints / 2);
	PointGroup RootGroup{ CombinedBox, 0, NumPoints - 1, Index2i(-1,-1) };
	GroupTree.add_ref(RootGroup);

	unsafe_vector<int32_t> SplitJobs;
	SplitJobs.reserve(NumPoints / 8);
	SplitJobs.add(0);

	int NextIndex = -1;
	int NumInternalNodes = 0, NumLeafNodes = 0, TotalLeafItemCount = 0;
	while (SplitJobs.pop_back(NextIndex))
	{
		PointGroup CurGroup = GroupTree[NextIndex];
		if (CurGroup.Count() > NumChildrenInLeaf)
		{
			NumInternalNodes++;

			int split_axis = select_split_axis_longest_pt(CurGroup.Bounds);
			RealType split_axis_origin = CurGroup.Bounds.Center()[split_axis];
			PointGroup LeftChild, RightChild;
			split_group_by_midpoint_pt(PointsList, CurGroup, split_axis, split_axis_origin, LeftChild, RightChild);

			// update current group in tree (note: add_ref may resize, so don't hold references)
			CurGroup.Children.A = (int)GroupTree.add_ref(LeftChild);
			CurGroup.Children.B = (int)GroupTree.add_ref(RightChild);
			GroupTree[NextIndex] = CurGroup;

			SplitJobs.add(CurGroup.Children.A);
			SplitJobs.add(CurGroup.Children.B);
		}
		else
		{
			NumLeafNodes++;
			TotalLeafItemCount += CurGroup.Count();
		}
	}
	int NumTreeNodes = (int)GroupTree.size();
	gs_debug_assert(NumLeafNodes > 0 && TotalLeafItemCount == NumPoints);

	// reassign linear indices to internal nodes (reusing StartIndex slot, which we no longer need for them)
	int LinearIndex = 0;
	for (int i = 0; i < NumTreeNodes; ++i) {
		if (!GroupTree[i].IsLeaf())
			GroupTree[i].StartIndex = LinearIndex++;
	}
	gs_debug_assert(LinearIndex == NumInternalNodes);

	// pack leaf points contiguously, recording each leaf group's start/count in StartIndex/EndIndex
	LeafPointLists.reserve(TotalLeafItemCount);
	for (int i = 0; i < NumTreeNodes; ++i) {
		PointGroup& Group = GroupTree[i];
		if (!Group.IsLeaf())
			continue;

		int start_index = (int)LeafPointLists.size(), count = Group.Count();
		for (int j = Group.StartIndex; j <= Group.EndIndex; j++)
		{
			SourcePoint& SrcPt = PointsList[j];
			[[maybe_unused]] int new_index = (int)LeafPointLists.add_ref(SourcePoint2{ SrcPt.Point, SrcPt.PointID });
		}
		Group.StartIndex = start_index; Group.EndIndex = count;
	}
	gs_debug_assert(LeafPointLists.size() == (size_t)NumPoints);

	this->RootIndex = ChildIndex{ 0,0 };
	this->RootBounds = CombinedBox;
	NodeTree.reserve(NumInternalNodes);

	for (int i = 0; i < NumTreeNodes; ++i)
	{
		const PointGroup& Group = GroupTree[i];
		if (Group.IsLeaf())
			continue;

		InteriorNode NewInteriorNode;

		const PointGroup& LeftChildGroup = GroupTree[Group.Children.A];
		const PointGroup& RightChildGroup = GroupTree[Group.Children.B];

		NewInteriorNode.LeftBounds = LeftChildGroup.Bounds;
		NewInteriorNode.LeftChild.Index = LeftChildGroup.StartIndex;
		NewInteriorNode.LeftChild.LeafCount = LeftChildGroup.IsLeaf() ? LeftChildGroup.EndIndex : 0;

		NewInteriorNode.RightBounds = RightChildGroup.Bounds;
		NewInteriorNode.RightChild.Index = RightChildGroup.StartIndex;
		NewInteriorNode.RightChild.LeafCount = RightChildGroup.IsLeaf() ? RightChildGroup.EndIndex : 0;

		int NewIndex = (int)NodeTree.add_ref(NewInteriorNode);
		gs_debug_assert(NewIndex == Group.StartIndex);
	}
}


template<typename RealType>
DistanceResult2<RealType> GS::PointSetTree2<RealType>::NearestPointQuery(
	Vector2<RealType> QueryPoint,
	DistanceQueryOptions<RealType> Options) const
{
	struct StackBox
	{
		ChildIndex Index;
		AxisBox2<RealType> Box;
	};

	TInlineSmallList<StackBox, 32> stack;

	DistanceResult2<RealType> MinDistResult;

	if (LeafPointLists.size() == 0)
		return MinDistResult;

	RealType MaxDistSqr = Options.MaxDistance * Options.MaxDistance;
	if (RootBounds.DistanceSquared(QueryPoint) > MaxDistSqr)
		return MinDistResult;

	RealType MinDistSqr = MaxDistSqr;

	stack.push_back({ RootIndex, RootBounds });
	StackBox nextBox;
	while (stack.pop_back(nextBox))
	{
		// re-test box: a smaller min-distance may have been found while this node was on the stack
		if (nextBox.Box.DistanceSquared(QueryPoint) > MinDistSqr)
			continue;

		ChildIndex next = nextBox.Index;

		if (next.LeafCount > 0) {
			for (uint32_t j = 0; j < next.LeafCount; ++j) {
				const SourcePoint2& SrcPt = LeafPointLists[next.Index + j];
				RealType DistSqr = SrcPt.Point.DistanceSquared(QueryPoint);
				if (DistSqr < MinDistSqr) {
					MinDistSqr = DistSqr;
					MinDistResult.ElementID = SrcPt.PointID;
					MinDistResult.DistanceSqr = DistSqr;
					MinDistResult.Point = SrcPt.Point;
				}
			}
		}
		else
		{
			const InteriorNode& Node = NodeTree[next.Index];
			StackBox Children[2] = { {Node.LeftChild,Node.LeftBounds}, {Node.RightChild,Node.RightBounds} };
			RealType ChildDists[2] = { Node.LeftBounds.DistanceSquared(QueryPoint), Node.RightBounds.DistanceSquared(QueryPoint) };
			if (ChildDists[0] > ChildDists[1]) {
				GS::SwapTemp(ChildDists[0], ChildDists[1]);
				GS::SwapTemp(Children[0], Children[1]);
			}
			if (ChildDists[0] < MinDistSqr) {
				stack.push_back(Children[0]);
				if (ChildDists[1] < MinDistSqr)
					stack.push_back(Children[1]);
			}
		}
	}

	return MinDistResult;
}


template<typename RealType>
int GS::PointSetTree2<RealType>::NearestKPointQuery(
	Vector2<RealType> QueryPoint,
	int K,
	DistanceResult2<RealType>* OutResults,
	DistanceQueryOptions<RealType> Options) const
{
	if (K <= 0 || OutResults == nullptr)
		return 0;
	if (LeafPointLists.size() == 0)
		return 0;

	RealType MaxDistSqr = Options.MaxDistance * Options.MaxDistance;
	if (RootBounds.DistanceSquared(QueryPoint) > MaxDistSqr)
		return 0;

	struct StackBox
	{
		ChildIndex Index;
		AxisBox2<RealType> Box;
	};

	TInlineSmallList<StackBox, 32> stack;

	int NumFound = 0;
	// pruning radius: while heap is not full, prune by MaxDistSqr; once full, prune by Kth-best.
	RealType KthBestDistSqr = MaxDistSqr;

	auto TryInsert = [&](int PointID, RealType DistSqr, const Vector2<RealType>& Point)
	{
		if (DistSqr > MaxDistSqr) return;
		if (NumFound == K && DistSqr >= OutResults[K - 1].DistanceSqr) return;

		// shift larger entries right; OutResults[0..NumFound-1] is sorted ascending
		int pos = (NumFound < K) ? NumFound : (K - 1);
		while (pos > 0 && OutResults[pos - 1].DistanceSqr > DistSqr) {
			OutResults[pos] = OutResults[pos - 1];
			pos--;
		}
		OutResults[pos] = DistanceResult2<RealType>();
		OutResults[pos].ElementID = PointID;
		OutResults[pos].DistanceSqr = DistSqr;
		OutResults[pos].Point = Point;

		if (NumFound < K) NumFound++;

		KthBestDistSqr = (NumFound == K) ? OutResults[K - 1].DistanceSqr : MaxDistSqr;
	};

	stack.push_back({ RootIndex, RootBounds });
	StackBox nextBox;
	while (stack.pop_back(nextBox))
	{
		if (nextBox.Box.DistanceSquared(QueryPoint) > KthBestDistSqr)
			continue;

		ChildIndex next = nextBox.Index;

		if (next.LeafCount > 0) {
			for (uint32_t j = 0; j < next.LeafCount; ++j) {
				const SourcePoint2& SrcPt = LeafPointLists[next.Index + j];
				RealType DistSqr = SrcPt.Point.DistanceSquared(QueryPoint);
				TryInsert(SrcPt.PointID, DistSqr, SrcPt.Point);
			}
		}
		else
		{
			const InteriorNode& Node = NodeTree[next.Index];
			StackBox Children[2] = { {Node.LeftChild,Node.LeftBounds}, {Node.RightChild,Node.RightBounds} };
			RealType ChildDists[2] = { Node.LeftBounds.DistanceSquared(QueryPoint), Node.RightBounds.DistanceSquared(QueryPoint) };
			if (ChildDists[0] > ChildDists[1]) {
				GS::SwapTemp(ChildDists[0], ChildDists[1]);
				GS::SwapTemp(Children[0], Children[1]);
			}
			if (ChildDists[0] < KthBestDistSqr) {
				stack.push_back(Children[0]);
				if (ChildDists[1] < KthBestDistSqr)
					stack.push_back(Children[1]);
			}
		}
	}

	return NumFound;
}




template<typename RealType>
static void check_boxes_pt(const AxisBox2<RealType>& A, const AxisBox2<RealType>& B)
{
	RealType AreaA = A.Area();
	RealType AreaB = B.Area();
	gs_debug_assert(ToleranceEqual(AreaA, AreaB));
	AxisBox2<RealType> Combined = A;
	Combined.Contain(B);
	RealType AreaC = Combined.Area();
	gs_debug_assert(ToleranceEqual(AreaA, AreaC));
}


template<typename RealType>
void GS::PointSetTree2<RealType>::Validate()
{
	if (LeafPointLists.size() == 0 && NodeTree.size() == 0)
		return;

	AxisBox2<RealType> ComputedBounds;
	if (RootIndex.LeafCount > 0)
		validate_leaf_child(RootIndex, ComputedBounds);
	else
		validate_internal_node(RootIndex, ComputedBounds);
	check_boxes_pt(RootBounds, ComputedBounds);
}


template<typename RealType>
void GS::PointSetTree2<RealType>::validate_leaf_child(ChildIndex Index, AxisBox2<RealType>& ComputedBounds)
{
	gs_debug_assert(Index.LeafCount > 0);
	ComputedBounds = AxisBox2<RealType>::Empty();
	for (uint32_t j = 0; j < Index.LeafCount; ++j) {
		ComputedBounds.Contain(LeafPointLists[Index.Index + j].Point);
	}
}


template<typename RealType>
void GS::PointSetTree2<RealType>::validate_internal_node(ChildIndex Index, AxisBox2<RealType>& ComputedBounds)
{
	gs_debug_assert(Index.LeafCount == 0);
	const InteriorNode& Node = NodeTree[Index.Index];

	AxisBox2<RealType> LeftBox, RightBox;
	if (Node.LeftChild.LeafCount > 0)
		validate_leaf_child(Node.LeftChild, LeftBox);
	else
		validate_internal_node(Node.LeftChild, LeftBox);
	check_boxes_pt(LeftBox, Node.LeftBounds);

	if (Node.RightChild.LeafCount > 0)
		validate_leaf_child(Node.RightChild, RightBox);
	else
		validate_internal_node(Node.RightChild, RightBox);
	check_boxes_pt(RightBox, Node.RightBounds);

	ComputedBounds = LeftBox;
	ComputedBounds.Contain(RightBox);
}


// explicit instantiation
template class GRADIENTSPACECORE_API GS::PointSetTree2<float>;
template class GRADIENTSPACECORE_API GS::PointSetTree2<double>;
