/*
 * Sub-path Ordering functions for embroidery stitch LPE (Implementation)
 *
 * Copyright (C) 2016 Michael Soegtrop
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/lpe-embrodery-stitch-ordering.h"

#include <numeric>

namespace Inkscape {
namespace LivePathEffect {
namespace LPEEmbroderyStitchOrdering {

using namespace Geom;

// ==================== Debug Trace Macros ====================

// ATTENTION: both level and area macros must be enabled for tracing

// These macros are for enabling certain levels of tracing
#define DebugTrace1(list) // g_warning list
#define DebugTrace2(list) // g_warning list

// These macros are for enabling certain areas of tracing
#define DebugTraceGrouping(list) // list
#define DebugTraceTSP(list)      // list

// Combinations of above
#define DebugTrace1TSP(list) DebugTraceTSP( DebugTrace1(list) )
#define DebugTrace2TSP(list) DebugTraceTSP( DebugTrace2(list) )

// ==================== Template Utilities ====================

// Delete all objects pointed to by a vector and clear the vector

template< typename T > void delete_and_clear(std::vector<T> &vector)
{
    for (typename std::vector<T>::iterator it = vector.begin(); it != vector.end(); ++it) {
        delete *it;
    }
    vector.clear();
}

// Assert that there are no duplicates in a vector

template< typename T > void assert_unique(std::vector<T> &vector)
{
    typename std::vector<T> copy = vector;
    std::sort(copy.begin(), copy.end());
    assert(std::unique(copy.begin(), copy.end()) == copy.end());
}

// remove element(s) by value

template< typename T > void remove_by_value(std::vector<T> *vector, const T &value)
{
    vector->erase(std::remove(vector->begin(), vector->end(), value), vector->end());
}

// fill a vector with increasing elements (similar to C++11 iota)
// iota is included in some C++ libraries, not in other (it is always included for C++11)
// To avoid issues, use our own name (not iota)

template<class OutputIterator, class Counter>
void fill_increasing(OutputIterator begin, OutputIterator end, Counter counter)
{
    while (begin != end) {
        *begin++ = counter++;
    }
}

// check if an iteratable sequence contains an element

template<class InputIterator, class Element>
bool contains(InputIterator begin, InputIterator end, const Element &elem)
{
    while (begin != end) {
        if (*begin == elem) {
            return true;
        }
        ++begin;
    }
    return false;
}

// Check if a vector contains an element

template<class Element>
bool contains(std::vector<Element> const &vector, const Element &elem)
{
    return contains(vector.begin(), vector.end(), elem);
}

// ==================== Multi-dimensional iterator functions ====================

// Below are 3 simple template functions to do triangle/pyramid iteration (without diagonal).
// Here is a sample of iterating over 5 elements in 3 dimensions:
//
// 0 1 2
// 0 1 3
// 0 1 4
// 0 2 3
// 0 2 4
// 1 2 4
// 1 3 4
// 2 3 4
// end end end
//
// If the number of elements is less then the number of dimensions, the number of dimensions is reduced automatically.
//
// I thought about creating an iterator class for this, but it doesn't match that well, so I used functions on iterator vectors.

// Initialize a vector of iterators

template<class Iterator>
void triangleit_begin(std::vector<Iterator> &iterators, Iterator const &begin, Iterator const &end, size_t n)
{
    iterators.clear();
    // limit number of dimensions to number of elements
    size_t n1 = end - begin;
    n = std::min(n, n1);
    if (n) {
        iterators.push_back(begin);
        for (int i = 1; i < n; i++) {
            iterators.push_back(iterators.back() + 1);
        }
    }
}

// Increment a vector of iterators

template<class Iterator>
void triangleit_incr(std::vector<Iterator> &iterators, Iterator const &end)
{
    size_t n = iterators.size();

    for (int i = 0; i < n; i++) {
        iterators[n - 1 - i]++;
        // Each dimension ends at end-i, so that there are elements left for the i higher dimensions
        if (iterators[n - 1 - i] != end - i) {
            // Assign increasing numbers to the higher dimension
            for (int j = n - i; j < n; j++) {
                iterators[j] = iterators[j - 1] + 1;
            }
            return;
        }
    }
}

// Check if a vector of iterators is at the end

template<class Iterator>
bool triangleit_test(std::vector<Iterator> &iterators, Iterator const &end)
{
    if (iterators.empty()) {
        return false;
    } else {
        return iterators.back() != end;
    }
}

// ==================== Trivial Ordering Functions ====================

// Sub-path reordering: do nothing - keep original order

void OrderingOriginal(std::vector<OrderingInfo> &infos)
{
}

// Sub-path reordering: reverse every other sub path

void OrderingZigZag(std::vector<OrderingInfo> &infos, bool revfirst)
{
    for (std::vector<OrderingInfo>::iterator it = infos.begin(); it != infos.end(); ++it) {
        it->reverse = (it->index & 1) == (revfirst ? 0 : 1);
    }
}

// Sub-path reordering: continue with the neartest start or end point of yet unused sub paths

void OrderingClosest(std::vector<OrderingInfo> &infos, bool revfirst)
{
    std::vector<OrderingInfo> result;
    result.reserve(infos.size());

    result.push_back(infos[0]);
    result.back().reverse = revfirst;
    Point p = result.back().GetEndRev();

    infos[0].used = true;


    for (unsigned int iRnd = 1; iRnd < infos.size(); iRnd++) {
        // find closest point to p
        unsigned iBest = 0;
        bool revBest = false;
        Coord distBest = infinity();

        for (std::vector<OrderingInfo>::iterator it = infos.begin(); it != infos.end(); ++it) {
            it->index = it - infos.begin();
            it->reverse = (it->index & 1) != 0;

            if (!it->used) {
                Coord dist = distance(p, it->GetBegOrig());
                if (dist < distBest) {
                    distBest = dist;
                    iBest = it - infos.begin();
                    revBest = false;
                }

                dist = distance(p, it->GetEndOrig());
                if (dist < distBest) {
                    distBest = dist;
                    iBest = it - infos.begin();
                    revBest = true;
                }
            }
        }

        result.push_back(infos[iBest]);
        result.back().reverse = revBest;
        p = result.back().GetEndRev();
        infos[iBest].used = true;
    }

    infos = result;
}

// ==================== Traveling Salesman k-opt Ordering Function and Utilities ====================

// A Few notes on this:
// - This is a relatively simple Lin-type k-opt algorithm, but the grouping optimizations done make it already quite complex.
// - The main Ordering Function is OrderingAdvanced
// - Lines which start at the end of another line are connected and treated as one (struct OrderingInfoEx)
// - Groups of zig-zag OrderingInfoEx are grouped (struct OrderingGroup) if both ends of the segment mutually agree with a next neighbor.
//   These groups are treated as a unit in the TSP algorithm.
//   The only option is to reverse the first segment, so that a group has 4 end points, 2 of which are used externally.
// - Run a k-opt (k=2..5) Lin like Traveling Salesman Problem algorithm on the groups as a unit and the remaining edges.
//   See https://en.wikipedia.org/wiki/Travelling_salesman_problem#Iterative_improvement
//   The algorithm uses a greedy nearest neighbor as start configuration and does not use repeated random starts.
// - The algorithm searches an open tour (rather than a closed one), so the longest segment in the closed path is ignored.
// - TODO: it might be faster to use k=3 with a few random starting patterns instead of k=5
// - TODO: it is surely wiser to implement e.g. Lin-Kenrighan TSP, but the simple k-opt works ok.
// - TODO(EASY): add a jump distance, above which threads are removed and make the length of this jump distance constant and large,
//   so that mostly the number of jumps is optimized

// Find 2 nearest points to given point

void OrderingPoint::FindNearest2(const std::vector<OrderingInfoEx *> &infos)
{
    // This implementation is not terribly elegant (unSTLish).
    // But for the first 2 elements using e.g. partial_sort is not simpler.

    Coord dist0 = infinity();
    Coord dist1 = infinity();
    nearest[0] = 0;
    nearest[1] = 0;

    for (std::vector<OrderingInfoEx *>::const_iterator it = infos.begin(); it != infos.end(); ++it) {
        Coord dist = distance(point, (*it)->beg.point);
        if (dist < dist1) {
            if (&(*it)->beg != this && &(*it)->end != this) {
                if (dist < dist0) {
                    nearest[1] = nearest[0];
                    nearest[0] = &(*it)->beg;
                    dist1 = dist0;
                    dist0 = dist;
                } else {
                    nearest[1] = &(*it)->beg;
                    dist1 = dist;
                }
            }
        }

        dist = distance(point, (*it)->end.point);
        if (dist < dist1) {
            if (&(*it)->beg != this && &(*it)->end != this) {
                if (dist < dist0) {
                    nearest[1] = nearest[0];
                    nearest[0] = &(*it)->end;
                    dist1 = dist0;
                    dist0 = dist;
                } else {
                    nearest[1] = &(*it)->end;
                    dist1 = dist;
                }
            }
        }
    }
}

// Check if "this" is among the nearest of its nearest

void OrderingPoint::EnforceMutual(void)
{
    if (nearest[0] && !(this == nearest[0]->nearest[0] || this == nearest[0]->nearest[1])) {
        nearest[0] = 0;
    }

    if (nearest[1] && !(this == nearest[1]->nearest[0] || this == nearest[1]->nearest[1])) {
        nearest[1] = 0;
    }

    if (nearest[1] && !nearest[0]) {
        nearest[0] = nearest[1];
        nearest[1] = 0;
    }
}

// Check if the subpath indices of this and other are the same, otherwise zero both nearest

void OrderingPoint::EnforceSymmetric(const OrderingPoint &other)
{
    if (nearest[0] && !(
                (other.nearest[0] && nearest[0]->infoex == other.nearest[0]->infoex) ||
                (other.nearest[1] && nearest[0]->infoex == other.nearest[1]->infoex)
            )) {
        nearest[0] = 0;
    }

    if (nearest[1] && !(
                (other.nearest[0] && nearest[1]->infoex == other.nearest[0]->infoex) ||
                (other.nearest[1] && nearest[1]->infoex == other.nearest[1]->infoex)
            )) {
        nearest[1] = 0;
    }

    if (nearest[1] && !nearest[0]) {
        nearest[0] = nearest[1];
        nearest[1] = 0;
    }
}

void OrderingPoint::Dump(void)
{
    Coord dist0 = nearest[0] ? distance(point, nearest[0]->point) : -1.0;
    Coord dist1 = nearest[1] ? distance(point, nearest[1]->point) : -1.0;
    int idx0 = nearest[0] ? nearest[0]->infoex->idx : -1;
    int idx1 = nearest[1] ? nearest[1]->infoex->idx : -1;
    DebugTrace2(("I=%d X=%.5lf Y=%.5lf d1=%.3lf d2=%.3lf i1=%d i2=%d", infoex->idx, point.x(), 297.0 - point.y(), dist0, dist1, idx0, idx1));
}


// If this element can be grouped (has neighbours) but is not yet grouped, create a new group

void OrderingInfoEx::MakeGroup(std::vector<OrderingInfoEx *> &infos, std::vector<OrderingGroup *> *groups)
{
    if (grouped || !beg.HasNearest() || !end.HasNearest()) {
        return;
    }

    groups->push_back(new OrderingGroup(groups->size()));

    // Add neighbors recursively
    AddToGroup(infos, groups->back());
}

// Add this and all connected elements to the group

void OrderingInfoEx::AddToGroup(std::vector<OrderingInfoEx *> &infos, OrderingGroup *group)
{
    if (grouped || !beg.HasNearest() || !end.HasNearest()) {
        return;
    }

    group->items.push_back(this);
    grouped = true;
    // Note: beg and end neighbors have been checked to be symmetric
    if (beg.nearest[0]) {
        beg.nearest[0]->infoex->AddToGroup(infos, group);
    }
    if (beg.nearest[1]) {
        beg.nearest[1]->infoex->AddToGroup(infos, group);
    }
    if (end.nearest[0]) {
        end.nearest[0]->infoex->AddToGroup(infos, group);
    }
    if (end.nearest[1]) {
        end.nearest[1]->infoex->AddToGroup(infos, group);
    }
}

// Constructor

OrderingGroupNeighbor::OrderingGroupNeighbor(OrderingGroupPoint *me, OrderingGroupPoint *other) :
    point(other),
    distance(Geom::distance(me->point, other->point))
{
}

// Comparison function for sorting by distance

bool OrderingGroupNeighbor::Compare(const OrderingGroupNeighbor &a, const OrderingGroupNeighbor &b)
{
    return a.distance < b.distance;
}

// Find the nearest unused neighbor point

OrderingGroupNeighbor *OrderingGroupPoint::FindNearestUnused(void)
{
    for (std::vector<OrderingGroupNeighbor>::iterator it = nearest.begin(); it != nearest.end(); ++it) {
        if (!it->point->used) {
            DebugTrace1TSP(("Nearest: group %d, size %d, point %d, nghb %d, xFrom %.4lf, yFrom %.4lf, xTo %.4lf, yTo %.4lf, dist %.4lf",
                            it->point->group->index, it->point->group->items.size(), it->point->indexInGroup, it - nearest.begin(),
                            point.x(), 297 - point.y(),
                            it->point->point.x(), 297 - it->point->point.y(),
                            it->distance));
            return &*it;
        }
    }

    // it shouldn't happen that we can't find any point at all
    assert(0);
    return 0;
}

// Return the other end in the group of the point

OrderingGroupPoint *OrderingGroupPoint::GetOtherEndGroup(void)
{
    return group->endpoints[ indexInGroup ^ 1 ];
}

// Return the alternate point (if one exists), 0 otherwise

OrderingGroupPoint *OrderingGroupPoint::GetAltPointGroup(void)
{
    if (group->nEndPoints < 4) {
        return 0;
    }

    OrderingGroupPoint *alt = group->endpoints[ indexInGroup ^ 2 ];
    return alt->used ? 0 : alt;
}


// Sets the rev flags in the group assuming that the group starts with this point

void OrderingGroupPoint::SetRevInGroup(void)
{
    // If this is not a front point, the item list needs to be reversed
    group->revItemList = !front;

    // If this is not a begin point, the items need to be reversed
    group->revItems = !begin;
}

// Mark an end point as used and also mark the two other alternating points as used
// Returns the used point

void OrderingGroupPoint::UsePoint(void)
{
    group->UsePoint(indexInGroup);
}

// Mark an end point as unused and possibly also mark the two other alternating points as unused
// Returns the used point

void OrderingGroupPoint::UnusePoint(void)
{
    group->UnusePoint(indexInGroup);
}

// Return the other end in the connection
OrderingGroupPoint *OrderingGroupPoint::GetOtherEndConnection(void)
{
    assert(connection);
    assert(connection->points[ indexInConnection ] == this);
    assert(connection->points[ indexInConnection ^ 1 ]);

    return connection->points[ indexInConnection ^ 1 ];
}


// Set the end points of a group from the items

void OrderingGroup::SetEndpoints(void)
{
    assert(items.size() >= 1);

    if (items.size() == 1) {
        // A simple line:
        //
        // b0-front--e1

        nEndPoints = 2;
        endpoints[0] = new OrderingGroupPoint(items.front()->beg.point, this, 0, true,  true);
        endpoints[1] = new OrderingGroupPoint(items.front()->end.point, this, 1, false, true);
    } else {
        // If the number of elements is even, the group is
        // either from items.front().beg to items.back().beg
        // or from     items.front().end to items.back().end:
        // Below: b=beg, e=end, numbers are end point indices
        //
        // b0-front--e     b0-front--e2
        //           |     |
        // b---------e     b---------e
        // |                         |
        // b---------e     b---------e
        //           |     |
        // b1-back---e     b1-back---e3
        //
        //
        // if the number of elements is odd, it is crossed:
        //
        // b0-front--e     b--front--e2
        //           |     |
        // b---------e     b---------e
        // |                         |
        // b--back---e1    b3-back---e
        //
        // TODO: this is not true with the following kind of pattern
        //
        // b--front--e
        // b---------e
        //           b--------e
        //           b--back--e
        //
        // Here only one connection is possible, from front.end to back.beg
        //
        // TODO: also this is not true if segment direction is alternating
        //
        // TOTO: => Just see where you end up from front().begin and front().end
        //
        // the numbering is such that either end points 0 and 1 are used or 2 and 3.
        int cross = items.size() & 1 ? 2 : 0;
        nEndPoints = 4;

        endpoints[0      ] = new OrderingGroupPoint(items.front()->beg.point, this, 0,       true,  true);
        endpoints[1 ^ cross] = new OrderingGroupPoint(items.back() ->beg.point, this, 1 ^ cross, true,  false);
        endpoints[2      ] = new OrderingGroupPoint(items.front()->end.point, this, 2,       false, true);
        endpoints[3 ^ cross] = new OrderingGroupPoint(items.back() ->end.point, this, 3 ^ cross, false, false);
    }
}

// Add all points from another group as neighbors

void OrderingGroup::AddNeighbors(OrderingGroup *nghb)
{
    for (int iThis = 0; iThis < nEndPoints; iThis++) {
        for (int iNghb = 0; iNghb < nghb->nEndPoints; iNghb++) {
            endpoints[iThis]->nearest.push_back(OrderingGroupNeighbor(endpoints[iThis], nghb->endpoints[iNghb]));
        }
    }
}

// Mark an end point as used and also mark the two other alternating points as used
// Returns the used point

OrderingGroupPoint *OrderingGroup::UsePoint(int index)
{
    assert(index < nEndPoints);
    assert(!endpoints[index]->used);
    endpoints[index]->used = true;
    if (nEndPoints == 4) {
        int offs = index < 2 ? 2 : 0;
        endpoints[0 + offs]->used = true;
        endpoints[1 + offs]->used = true;
    }

    return endpoints[index];
}

// Mark an end point as unused and possibly also mark the two other alternating points as unused
// Returns the used point

void OrderingGroup::UnusePoint(int index)
{
    assert(index < nEndPoints);
    assert(endpoints[index]->used);
    endpoints[index]->used = false;

    if (nEndPoints == 4 && !endpoints[index ^ 1]->used) {
        int offs = index < 2 ? 2 : 0;
        endpoints[0 + offs]->used = false;
        endpoints[1 + offs]->used = false;
    }
}

// Add an end point
void OrderingSegment::AddPoint(OrderingGroupPoint *point)
{
    assert(point);
    assert(nEndPoints < 4);
    endpoints[ nEndPoints++ ] = point;

    // If both ends of a group are added and the group has 4 points, add the other two as well
    if (nEndPoints == 2 && endpoints[0]->group == endpoints[1]->group) {
        OrderingGroup *group = endpoints[0]->group;
        if (group->nEndPoints == 4) {
            for (int i = 0; i < 4; i++) {
                endpoints[i] = group->endpoints[i];
            }
            nEndPoints = 4;
        }
    }
}

// Get begin point (taking swap and end bit into account
OrderingGroupPoint *OrderingSegment::GetBeginPoint(unsigned int iSwap, unsigned int iEnd)
{
    int iPoint = ((iEnd >> endbit) & 1) + (((iSwap >> swapbit) & 1) << 1);
    assert(iPoint < nEndPoints);
    return endpoints[iPoint];
}

// Get end point (taking swap and end bit into account
OrderingGroupPoint *OrderingSegment::GetEndPoint(unsigned int iSwap, unsigned int iEnd)
{
    int iPoint = (((iEnd >> endbit) & 1) ^ 1) + (((iSwap >> swapbit) & 1) << 1);
    assert(iPoint < nEndPoints);
    return endpoints[iPoint];
}


// Find the next unused point in list
std::vector<OrderingGroupPoint *>::iterator FindUnusedAndUse(std::vector<OrderingGroupPoint *> *unusedPoints, std::vector<OrderingGroupPoint *>::iterator const from)
{
    for (std::vector<OrderingGroupPoint *>::iterator it = from; it != unusedPoints->end(); ++it) {
        if (!(*it)->used) {
            (*it)->UsePoint();
            return it;
        }
    }
    return unusedPoints->end();
}

// Find the shortest reconnect between the given points

bool FindShortestReconnect(std::vector<OrderingSegment> &segments, std::vector<OrderingGroupConnection *> &connections, std::vector<OrderingGroupConnection *> &allconnections, OrderingGroupConnection **longestConnect, Coord *total, Coord olddist)
{
    // Find the longest connection outside of the active set
    // The longest segment is then the longest of this longest outside segment and all inside segments
    OrderingGroupConnection *longestOutside = 0;

    if (contains(connections, *longestConnect)) {
        // The longest connection is inside the active set, so we need to search for the longest outside
        Coord length = 0.0;
        for (std::vector<OrderingGroupConnection *>::iterator it = allconnections.begin(); it != allconnections.end(); it++) {
            if ((*it)->Distance() > length) {
                if (!contains(connections, *it)) {
                    longestOutside = *it;
                    length = (*it)->Distance();
                }
            }
        }
    } else {
        longestOutside = *longestConnect;
    }

    // length of longestConnect outside
    Coord longestOutsideLength = longestOutside ? longestOutside->Distance() : 0.0;

    // We measure length without the longest, so subtract the longest length from the old distance
    olddist -= (*longestConnect)->Distance();

    // Assign a swap bit and end bit to each active connection
    int nEndBits = 0;
    int nSwapBits = 0;
    for (std::vector<OrderingSegment>::iterator it = segments.begin(); it != segments.end(); it++) {
        it->endbit = nEndBits++;
        if (it->nEndPoints == 4) {
            it->swapbit = nSwapBits++;
        } else {
            // bit 32 should always be 0
            it->swapbit = 31;
        }
    }

    unsigned int swapMask = (1U << nSwapBits) - 1;
    unsigned int endMask = (1U << nEndBits) - 1;

    // Create a permutation vector
    std::vector<int> permutation(segments.size());
    fill_increasing(permutation.begin(), permutation.end(), 0);

    // best improvement
    bool improved = false;
    Coord distBest = olddist;
    std::vector<int> permutationBest;
    unsigned int iSwapBest;
    unsigned int iEndBest;
    int nTrials = 0;

    // Loop over the permutations
    do {
        // Loop over the swap bits
        unsigned int iSwap = 0;
        do {
            // Loop over the end bits
            unsigned int iEnd = 0;
            do {
                // Length of all active connections
                Coord lengthTotal = 0;
                // Length of longest connection (active or inactive)
                Coord lengthLongest = longestOutsideLength;

                // Close the loop with the end point of the last segment
                OrderingGroupPoint *prevend = segments[permutation.back()].GetEndPoint(iSwap, iEnd);
                for (std::vector<int>::iterator it = permutation.begin(); it != permutation.end(); it++) {
                    OrderingGroupPoint *thisbeg = segments[*it].GetBeginPoint(iSwap, iEnd);
                    Coord length = Geom::distance(thisbeg->point, prevend->point);
                    lengthTotal += length;
                    if (length > lengthLongest) {
                        lengthLongest = length;
                    }
                    prevend = segments[*it].GetEndPoint(iSwap, iEnd);
                }
                lengthTotal -= lengthLongest;

                // If there is an improvement, remember the best selection
                if (lengthTotal + 1e-6 < distBest) {
                    improved = true;
                    distBest = lengthTotal;
                    permutationBest = permutation;
                    iSwapBest = iSwap;
                    iEndBest = iEnd;

                    // Just debug printing
                    OrderingGroupPoint *prevend = segments[permutation.back()].GetEndPoint(iSwap, iEnd);
                    for (std::vector<int>::iterator it = permutation.begin(); it != permutation.end(); it++) {
                        OrderingGroupPoint *thisbeg = segments[*it].GetBeginPoint(iSwap, iEnd);
                        DebugTrace2TSP(("IMP 0F=%d %d %.6lf", thisbeg->group->index, thisbeg->indexInGroup, Geom::distance(thisbeg->point, prevend->point)));
                        DebugTrace2TSP(("IMP 0T=%d %d %.6lf", prevend->group->index, prevend->indexInGroup, Geom::distance(thisbeg->point, prevend->point)));
                        prevend = segments[*it].GetEndPoint(iSwap, iEnd);
                    }
                }

                nTrials++;

                // bit 0 is always 0, because the first segment is kept fixed
                iEnd += 2;
            } while (iEnd & endMask);
            iSwap++;
        } while (iSwap & swapMask);
        // first segment is kept fixed
    } while (std::next_permutation(permutation.begin() + 1, permutation.end()));

    if (improved) {
        DebugTrace2TSP(("Improvement %lf->%lf in %d", olddist, distBest, nTrials));
        // change the connections

        for (std::vector<OrderingGroupConnection *>::iterator it = connections.begin(); it != connections.end(); ++it) {
            DebugTrace2TSP(("WAS 0F=%d %d %.6lf", (*it)->points[0]->group->index, (*it)->points[0]->indexInGroup, (*it)->Distance()));
            DebugTrace2TSP(("WAS 0T=%d %d %.6lf", (*it)->points[1]->group->index, (*it)->points[1]->indexInGroup, (*it)->Distance()));
        }
        DebugTrace2TSP(("OLDDIST %.6lf delta %.6lf", olddist, olddist - (*longestConnect)->Distance()));
        DebugTrace2TSP(("LONG =%d %d %.6lf", (*longestConnect)->points[0]->group->index, (*longestConnect)->points[0]->indexInGroup, (*longestConnect)->Distance()));
        DebugTrace2TSP(("LONG =%d %d %.6lf", (*longestConnect)->points[1]->group->index, (*longestConnect)->points[1]->indexInGroup, (*longestConnect)->Distance()));

        int perm = permutationBest.back();

        for (std::vector<OrderingGroupConnection *>::iterator it = connections.begin(); it != connections.end(); ++it) {
            (*it)->Connect(1, segments[ perm ].GetEndPoint(iSwapBest, iEndBest));
            perm = permutationBest[ it - connections.begin() ];
            (*it)->Connect(0, segments[ perm ].GetBeginPoint(iSwapBest, iEndBest));

        }

        for (std::vector<OrderingGroupConnection *>::iterator it = connections.begin(); it != connections.end(); ++it) {
            DebugTrace2TSP(("IS 0F=%d %d %.6lf", (*it)->points[0]->group->index, (*it)->points[0]->indexInGroup, (*it)->Distance()));
            DebugTrace2TSP(("IS 0T=%d %d %.6lf", (*it)->points[1]->group->index, (*it)->points[1]->indexInGroup, (*it)->Distance()));
        }

        (*longestConnect) = longestOutside;
        for (std::vector<OrderingGroupConnection *>::iterator it = connections.begin(); it != connections.end(); ++it) {
            if ((*it)->Distance() > (*longestConnect)->Distance()) {
                *longestConnect = *it;
            }
        }
        DebugTrace2TSP(("LONG =%d %d %.6lf", (*longestConnect)->points[0]->group->index, (*longestConnect)->points[0]->indexInGroup, (*longestConnect)->Distance()));
        DebugTrace2TSP(("LONG =%d %d %.6lf", (*longestConnect)->points[1]->group->index, (*longestConnect)->points[1]->indexInGroup, (*longestConnect)->Distance()));
    }

    return improved;
}

// Check if connections form a tour
void AssertIsTour(std::vector<OrderingGroup *> &groups, std::vector<OrderingGroupConnection *> &connections, OrderingGroupConnection *longestConnection)
{
    for (std::vector<OrderingGroupConnection *>::iterator it = connections.begin(); it != connections.end(); it++) {
        for (int i = 0; i < 2; i++) {
            OrderingGroupPoint *pnt = (*it)->points[i];
            assert(pnt->connection == *it);
            assert(pnt->connection->points[pnt->indexInConnection] == pnt);
            assert(pnt->group->endpoints[pnt->indexInGroup] == pnt);
        }
    }

    Coord length1 = 0;
    Coord longest1 = 0;
    OrderingGroupPoint *current = connections.front()->points[0];

    for (unsigned int n = 0; n < connections.size(); n++) {
        DebugTrace2TSP(("Tour test 1 %p g=%d/%d c=%d/%d %p %p %.6lf %.3lf %.3lf %d %d %d", current, current->group->index, current->indexInGroup, current->connection->index, current->indexInConnection, current->connection->points[0], current->connection->points[1], current->connection->Distance(), current->point.x(), 297 - current->point.y(), current->begin, current->front, current->group->items.size()));
        Coord length = current->connection->Distance();
        length1 += length;
        longest1 = std::max(length, longest1);
        current = current->GetOtherEndConnection();

        DebugTrace2TSP(("Tour test 2 %p g=%d/%d c=%d/%d %p %p %.6lf %.3lf %.3lf %d %d %d", current, current->group->index, current->indexInGroup, current->connection->index, current->indexInConnection, current->connection->points[0], current->connection->points[1], current->connection->Distance(), current->point.x(), 297 - current->point.y(), current->begin, current->front, current->group->items.size()));
        current = current->GetOtherEndGroup();
    }
    DebugTrace2TSP(("Tour test 3 %p g=%d/%d c=%d/%d %p %p", current, current->group->index, current->indexInGroup, current->connection->index, current->indexInConnection, current->connection->points[0], current->connection->points[1]));
    assert(current == connections.front()->points[0]);

    // The other direction
    Coord length2 = 0;
    Coord longest2 = 0;
    current = connections.front()->points[0];
    for (unsigned int n = 0; n < connections.size(); n++) {
        current = current->GetOtherEndGroup();
        Coord length = current->connection->Distance();
        length2 += length;
        longest2 = std::max(length, longest2);
        current = current->GetOtherEndConnection();
    }
    assert(current == connections.front()->points[0]);

    DebugTrace1TSP(("Tour length %.6lf(%.6lf) longest %.6lf(%.6lf) remaining %.6lf(%.6lf)", length1, length2, longest1, longest2, length1 - longest1, length2 - longest2));
}

// Bring a tour into linear order after a modification

/* I would like to avoid this.
 * It is no problem to travel a tour with changing directions using the GetOtherEnd functions,
 * but it is difficult to know the segments, that is which endpoint of a connection is connected to which by the unmodified pieces of the tour.
 * In the end it is probably better to implement the Lin-Kernighan algorithm which avoids this problem by creating connected changes.  */

void LinearizeTour(std::vector<OrderingGroupConnection *> &connections)
{
    OrderingGroupPoint *current = connections.front()->points[0];

    for (unsigned int iNew = 0; iNew < connections.size(); iNew++) {
        // swap the connection at location n with the current connection
        OrderingGroupConnection *connection = current->connection;
        unsigned int iOld = connection->index;
        assert(connections[iOld] == connection);

        connections[iOld] = connections[iNew];
        connections[iNew] = connection;
        connections[iOld]->index = iOld;
        connections[iNew]->index = iNew;

        // swap the points of a connection
        assert(current == connection->points[0] || current == connection->points[1]);
        if (current != connection->points[0]) {
            connection->points[1] = connection->points[0];
            connection->points[0] = current;
            connection->points[1]->indexInConnection = 1;
            connection->points[0]->indexInConnection = 0;
        }

        current = current->GetOtherEndConnection();
        current = current->GetOtherEndGroup();
    }
}

// Use some Traveling Salesman Problem (TSP) like heuristics to bring several groups into a
// order with as short as possible interconnection paths

void OrderGroups(std::vector<OrderingGroup *> *groups, const int nDims)
{
    // There is no point in ordering just one group
    if (groups->size() <= 1) {
        return;
    }

    // Initialize the endpoints for all groups
    for (std::vector<OrderingGroup *>::iterator it = groups->begin(); it != groups->end(); ++it) {
        (*it)->SetEndpoints();
    }

    // Find the neighboring points for all end points of all groups and sort by distance
    for (std::vector<OrderingGroup *>::iterator itThis = groups->begin(); itThis != groups->end(); ++itThis) {
        for (int i = 0; i < (*itThis)->nEndPoints; i++) {
            // This can be up to 2x too large, but still better than incrementing the size
            (*itThis)->endpoints[i]->nearest.reserve(4 * groups->size());
        }

        for (std::vector<OrderingGroup *>::iterator itNghb = groups->begin(); itNghb != groups->end(); ++itNghb) {
            if (itThis != itNghb) {
                (*itThis)->AddNeighbors(*itNghb);
            }
        }

        for (int i = 0; i < (*itThis)->nEndPoints; i++) {
            std::sort((*itThis)->endpoints[i]->nearest.begin(), (*itThis)->endpoints[i]->nearest.end(), OrderingGroupNeighbor::Compare);
        }
    }

    // =========== Step 1: Create a simple nearest neighbor chain ===========

    // Vector of connection points
    std::vector<OrderingGroupConnection *> connections;
    connections.reserve(groups->size());
    // Total Jump Distance
    Coord total = 0.0;

    // Start with the first group and connect always with nearest unused point
    OrderingGroupPoint *crnt = groups->front()->endpoints[0];

    // The longest connection is ignored (we don't want cycles)
    OrderingGroupConnection *longestConnect = 0;

    for (unsigned int nConnected = 0; nConnected < groups->size(); nConnected++) {
        // Mark both end points of the current segment as used
        crnt->UsePoint();
        crnt = crnt->GetOtherEndGroup();
        crnt->UsePoint();

        // if this is the last segment, Mark start point of first segment as unused,
        // so that the end can connect to it
        if (nConnected == groups->size() - 1) {
            groups->front()->endpoints[0]->UnusePoint();
        }

        // connect to next segment
        OrderingGroupNeighbor *nghb = crnt->FindNearestUnused();
        connections.push_back(new OrderingGroupConnection(crnt, nghb->point, connections.size()));
        total += nghb->distance;
        crnt = nghb->point;

        if (!longestConnect || nghb->distance > longestConnect->Distance()) {
            longestConnect = connections.back();
        }
    }

    DebugTrace1TSP(("Total jump distance %.3lf (closed)", total));
    DebugTrace1TSP(("Total jump distance %.3lf (open)", total - longestConnect->Distance()));

    AssertIsTour(*groups, connections, longestConnect);

    // =========== Step 2: Choose nDims segments to clear and reconnect ===========

    bool improvement;
    int nRuns = 0;
    int nTrials = 0;
    int nImprovements = 0;

    do {
        improvement = false;
        nRuns ++;
        std::vector< std::vector<OrderingGroupConnection *>::iterator > iterators;

        for (
            triangleit_begin(iterators, connections.begin(), connections.end(), nDims);
            triangleit_test(iterators, connections.end());
            triangleit_incr(iterators, connections.end())
        ) {
            nTrials ++;

            Coord dist = 0;

            std::vector<OrderingSegment> segments(iterators.size());
            std::vector<OrderingGroupConnection *> changedconnections;
            changedconnections.reserve(3);
            OrderingGroupConnection *prev = *iterators.back();


            for (size_t i = 0; i < iterators.size(); i++) {
                dist += (*iterators[i])->Distance();
                segments[i].AddPoint(prev->points[1]);
                segments[i].AddPoint((*iterators[i])->points[0]);
                prev = *iterators[i];
                changedconnections.push_back(*iterators[i]);
            }

            if (FindShortestReconnect(segments, changedconnections, connections, &longestConnect, &total, dist)) {
                nImprovements ++;

                AssertIsTour(*groups, connections, longestConnect);
                LinearizeTour(connections);
                AssertIsTour(*groups, connections, longestConnect);
                improvement = true;
            }
        }
    } while (improvement && nRuns < 10);

    DebugTrace1TSP(("Finished after %d rounds, %d trials, %d improvements", nRuns, nTrials, nImprovements));

    // =========== Step N: Create vector of groups from vector of connection points ===========

    std::vector<OrderingGroup *> result;
    result.reserve(groups->size());

    // Go through the groups starting with the longest connection (which is this way left out)
    {
        OrderingGroupPoint *current = longestConnect->points[1];

        for (unsigned int n = 0; n < connections.size(); n++) {
            result.push_back(current->group);
            current->SetRevInGroup();
            current = current->GetOtherEndGroup();
            current = current->GetOtherEndConnection();
        }
    }

    assert(result.size() == groups->size());
    assert_unique(result);

    delete_and_clear(connections);

    *groups = result;
}

// Global optimization of path length

void OrderingAdvanced(std::vector<OrderingInfo> &infos, int nDims)
{
    if (infos.size() < 3) {
        return;
    }

    // Create extended ordering info vector and copy data from normal ordering info
    std::vector<OrderingInfoEx *> infoex;
    infoex.reserve(infos.size());

    for (std::vector<OrderingInfo>::const_iterator it = infos.begin(); it != infos.end();) {
        // Note: This assumes that the index in the OrderingInfo matches the vector index!
        infoex.push_back(new OrderingInfoEx(*it, infoex.size()));
        ++it;
        while (it != infos.end() && it->begOrig == infoex.back()->end.point) {
            infoex.back()->end.point = it->endOrig;
            infoex.back()->origIndices.push_back(it->index);
            ++it;
        }
    }

    // Find closest 2 points for each point and enforce that 2nd nearest is not further away than 1.8xthe nearest
    // If this is not the case, clear nearest and 2nd nearest point
    for (std::vector<OrderingInfoEx *>::iterator it = infoex.begin(); it != infoex.end(); ++it) {
        (*it)->beg.FindNearest2(infoex);
        (*it)->end.FindNearest2(infoex);
    }

    DebugTraceGrouping(
        DebugTrace2(("STEP1"));
    for (std::vector<OrderingInfoEx *>::iterator it = infoex.begin(); it != infoex.end(); ++it) {
    (*it)->beg.Dump();
        (*it)->end.Dump();
    }
    )

    // Make sure the nearest points are mutual
    for (std::vector<OrderingInfoEx *>::iterator it = infoex.begin(); it != infoex.end(); ++it) {
        (*it)->beg.EnforceMutual();
        (*it)->end.EnforceMutual();
    }

    DebugTraceGrouping(
        DebugTrace2(("STEP2"));
    for (std::vector<OrderingInfoEx *>::iterator it = infoex.begin(); it != infoex.end(); ++it) {
    (*it)->beg.Dump();
        (*it)->end.Dump();
    }
    )

    // Make sure the nearest points for begin and end lead to the same sub-path (same index)
    for (std::vector<OrderingInfoEx *>::iterator it = infoex.begin(); it != infoex.end(); ++it) {
        (*it)->beg.EnforceSymmetric((*it)->end);
        (*it)->end.EnforceSymmetric((*it)->beg);
    }

    DebugTraceGrouping(
        DebugTrace2(("STEP3"));
    for (std::vector<OrderingInfoEx *>::iterator it = infoex.begin(); it != infoex.end(); ++it) {
    (*it)->beg.Dump();
        (*it)->end.Dump();
    }
    )

    // The remaining nearest neighbors should be 100% non ambiguous, so group them
    std::vector<OrderingGroup *> groups;
    for (std::vector<OrderingInfoEx *>::iterator it = infoex.begin(); it != infoex.end(); ++it) {
        (*it)->MakeGroup(infoex, &groups);
    }

    // Create single groups for ungrouped lines
    std::vector<OrderingInfo> result;
    result.reserve(infos.size());
    int nUngrouped = 0;
    for (std::vector<OrderingInfoEx *>::iterator it = infoex.begin(); it != infoex.end(); ++it) {
        if (!(*it)->grouped) {
            groups.push_back(new OrderingGroup(groups.size()));
            groups.back()->items.push_back(*it);
            nUngrouped++;
        }
    }

    DebugTraceGrouping(
        DebugTrace2(("Ungrouped lines = %d", nUngrouped));
        DebugTrace2(("%d Groups found", groups.size()));
    for (std::vector<OrderingGroup *>::iterator it = groups.begin(); it != groups.end(); ++it) {
    DebugTrace2(("Group size %d", (*it)->items.size()));
    }
    )

    // Order groups, so that the connection path gets shortest
    OrderGroups(&groups, nDims);

    // Copy grouped lines to output
    for (std::vector<OrderingGroup *>::iterator itGroup = groups.begin(); itGroup != groups.end(); ++itGroup) {
        for (unsigned int iItem = 0; iItem < (*itGroup)->items.size(); iItem++) {
            unsigned int iItemRev = (*itGroup)->revItemList ? (*itGroup)->items.size() - 1 - iItem : iItem;
            OrderingInfoEx *item = (*itGroup)->items[iItemRev];

            // If revItems is false, even items shall have reverse=false
            // In this case ( ( iItem & 1 ) == 0 )== true, revItems=false, (true==false) == false
            bool reverse = ((iItem & 1) == 0) == (*itGroup)->revItems;
            if (!reverse) {
                for (std::vector<int>::iterator itOrig = item->origIndices.begin(); itOrig != item->origIndices.end(); ++itOrig) {
                    result.push_back(infos[*itOrig]);
                    result.back().reverse = false;
                }
            } else {
                for (std::vector<int>::reverse_iterator itOrig = item->origIndices.rbegin(); itOrig != item->origIndices.rend(); ++itOrig) {
                    result.push_back(infos[*itOrig]);
                    result.back().reverse = true;
                }
            }
        }
        result.back().connect = true;
    }


    delete_and_clear(groups);
    delete_and_clear(infoex);

    infos = result;
}

} // namespace LPEEmbroderyStitchOrdering
} // namespace LivePathEffect
} // namespace Inkscape
