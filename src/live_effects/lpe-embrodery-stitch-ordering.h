/*
 * Sub-path Ordering functions for embroidery stitch LPE
 *
 * Copyright (C) 2016 Michael Soegtrop
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef INKSCAPE_LPE_EMBRODERY_STITCH_ORDERING_H
#define INKSCAPE_LPE_EMBRODERY_STITCH_ORDERING_H

#include "live_effects/effect.h"

namespace Inkscape {
namespace LivePathEffect {
namespace LPEEmbroderyStitchOrdering {

// Structure keeping information on the ordering and reversing of sub paths
// Used for simple ordering functions like zig-zag

struct OrderingInfo {
    int   index;
    bool  reverse;
    bool  used;
    bool  connect;
    Geom::Point begOrig; // begin point in original orientation
    Geom::Point endOrig; // end point in original orientation

    Geom::Point GetBegOrig() const
    {
        return begOrig;
    }
    Geom::Point GetEndOrig() const
    {
        return endOrig;
    }
    Geom::Point GetBegRev() const
    {
        return reverse ? endOrig : begOrig;
    }
    Geom::Point GetEndRev() const
    {
        return reverse ? begOrig : endOrig;
    }
};

// Structure for a path end-point in OrderingInfoEx.
// This keeps information about the two nearest neighbor points.

struct OrderingInfoEx;

struct OrderingPoint {
    OrderingPoint(const Geom::Point &pointIn, OrderingInfoEx *infoexIn, bool beginIn) :
        point(pointIn),
        infoex(infoexIn),
        begin(beginIn)
    {
        nearest[0] = nearest[1] = 0;
    }

    // Check if both nearest values are valid
    bool IsNearestValid() const
    {
        return nearest[0] && nearest[1];
    }
    // Check if at least one nearest values are valid
    bool HasNearest() const
    {
        return nearest[0] || nearest[1];
    }
    // Find 2 nearest points to given point
    void FindNearest2(const std::vector<OrderingInfoEx *> &infos);
    // Check if "this" is among the nearest of its nearest
    void EnforceMutual(void);
    // Check if the subpath indices of this and other are the same, otherwise zero both nearest
    void EnforceSymmetric(const OrderingPoint &other);
    // Dump point information
    void Dump(void);

    Geom::Point point;
    OrderingInfoEx *infoex;
    bool  begin;
    const OrderingPoint *nearest[2];
};

// Structure keeping information on the ordering and reversing of sub paths
// Used for advanced ordering functions with block creation and Traveling Salesman Problem Optimization
// A OrderingInfoEx may contain several original sub-paths in case sub-paths are directly connected.
// Directly connected sub-paths happen e.g. after a slice boolean operation.

struct OrderingGroup;

struct OrderingInfoEx {
    OrderingInfoEx(const OrderingInfo &infoIn, int idxIn) :
        beg(infoIn.begOrig, this, true),
        end(infoIn.endOrig, this, false),
        idx(idxIn),
        grouped(false)
    {
        origIndices.push_back(infoIn.index);
    }

    // If this element can be grouped (has neighbours) but is not yet grouped, create a new group
    void MakeGroup(std::vector<OrderingInfoEx *> &infos, std::vector<OrderingGroup *> *groups);
    // Add this and all connected elements to the group
    void AddToGroup(std::vector<OrderingInfoEx *> &infos, OrderingGroup *group);

    int idx;
    bool grouped;      // true if this element has been grouped
    OrderingPoint beg; // begin point in original orientation
    OrderingPoint end; // end point in original orientation
    std::vector<int> origIndices; // Indices of the original OrderInfos (more than 1 if directly connected
};

// Neighbor information for OrderingGroupPoint - a distance and a OrderingGroupPoint

struct OrderingGroupPoint;

struct OrderingGroupNeighbor {
    OrderingGroupNeighbor(OrderingGroupPoint *me, OrderingGroupPoint *other);

    // Distance from owner of this neighbor info
    Geom::Coord distance;
    // Neighbor point (which in turn contains a pointer to the neighbor group
    OrderingGroupPoint *point;

    // Comparison function for sorting by distance
    static bool Compare(const OrderingGroupNeighbor &a, const OrderingGroupNeighbor &b);
};

// An end point in an OrderingGroup, with nearest neighbor information

struct OrderingGroupConnection;

struct OrderingGroupPoint {
    OrderingGroupPoint(const Geom::Point &pointIn, OrderingGroup *groupIn, int indexIn, bool beginIn, bool frontIn) :
        point(pointIn),
        group(groupIn),
        indexInGroup(indexIn),
        connection(0),
        indexInConnection(0),
        begin(beginIn),
        front(frontIn),
        used(false)
    {
    }

    // Find the nearest unused neighbor point
    OrderingGroupNeighbor *FindNearestUnused(void);
    // Return the other end in the group of the point
    OrderingGroupPoint *GetOtherEndGroup(void);
    // Return the alternate point (if one exists), 0 otherwise
    OrderingGroupPoint *GetAltPointGroup(void);
    // Sets the rev flags in the group assuming that the group starts with this point
    void SetRevInGroup(void);
    // Mark an end point as used and also mark the two other alternating points as used
    // Returns the used point
    void UsePoint(void);
    // Mark an end point as unused and possibly also mark the two other alternating points as unused
    // Returns the used point
    void UnusePoint(void);
    // Return the other end in the connection
    OrderingGroupPoint *GetOtherEndConnection(void);

    // The coordinates of the point
    Geom::Point point;
    // The group to which the point belongs
    OrderingGroup *group;
    // The end-point index within the group
    int indexInGroup;
    // The connection, which connects this point
    OrderingGroupConnection *connection;
    // The end point index in the connection
    int indexInConnection;
    // True if this is a begin point (rather than an end point)
    bool begin;
    // True if this is a front point (rather than a back point)
    bool front;
    // True if the point is used/connected to another point
    bool used;
    // The nearest neighbors, to which this group end point may connect
    std::vector<OrderingGroupNeighbor> nearest;
};

// A connection between two points/groups
struct OrderingGroupConnection {
    OrderingGroupConnection(OrderingGroupPoint *fromIn, OrderingGroupPoint *toIn, int indexIn) :
        index(indexIn)
    {
        assert(fromIn->connection == 0);
        assert(toIn->connection == 0);
        points[0] = 0;
        points[1] = 0;
        Connect(0, fromIn);
        Connect(1, toIn);
    }

    // Connect one of the connection endpoints to the given point
    void Connect(int index, OrderingGroupPoint *point)
    {
        assert(point);
        points[index] = point;
        point->connection = this;
        point->indexInConnection = index;
    }

    // Get length of connection
    Geom::Coord Distance()
    {
        return Geom::distance(points[0]->point, points[1]->point);
    }

    OrderingGroupPoint *points[2];
    // index of connection in the connections vector (just for debugging)
    int index;
};

// A group of OrderingInfoEx, which build a block in path interconnect length optimization.
// A block can have two sets of endpoints.
// If a block has 2 sets of endpoints, one can swap between the two sets.

struct OrderingGroup {
    OrderingGroup(int indexIn) :
        nEndPoints(0),
        revItemList(false),
        revItems(false),
        index(indexIn)
    {
        for (int i = 0; i < sizeof(endpoints) / sizeof(*endpoints); i++) {
            endpoints[i] = 0;
        }
    }

    ~OrderingGroup()
    {
        for (int i = 0; i < nEndPoints; i++) {
            delete endpoints[i];
        }
    }

    // Set the endpoints of a group from the items
    void SetEndpoints(void);
    // Add all points from another group as neighbors
    void AddNeighbors(OrderingGroup *nghb);
    // Mark an end point as used and also mark the two other alternating points as used
    // Returns the used point
    OrderingGroupPoint *UsePoint(int index);
    // Mark an end point as unused and possibly also mark the two other alternating points as unused
    // Returns the used point
    void UnusePoint(int index);

    // Items on the group
    std::vector<OrderingInfoEx *> items;
    // End points of the group
    OrderingGroupPoint *endpoints[4];
    // Number of endpoints used (either 2 or 4)
    int nEndPoints;
    // Index of the group (just for debugging purposes)
    int index;
    // If true, the items in the group shall be output from back to front.
    bool revItemList;
    // If false, the individual items are output alternatingly normal-reversed
    // If true, the individual items are output alternatingly reversed-normal
    bool revItems;
};

// A segment is either a OrderingGroup or a series of groups and connections.
// Usually a segment has just 2 end points.
// If a segment is just one ordering group, it has the same number of end points as the ordering group
// A main difference between a segment and a group is that the segment does not own the end points.

struct OrderingSegment {
    OrderingSegment() :
        nEndPoints(0),
        endbit(0),
        swapbit(0)
    {}

    // Add an end point
    void AddPoint(OrderingGroupPoint *point);
    // Get begin point (taking swap and end bit into account
    OrderingGroupPoint *GetBeginPoint(unsigned int iSwap, unsigned int iEnd);
    // Get end point (taking swap and end bit into account
    OrderingGroupPoint *GetEndPoint(unsigned int iSwap, unsigned int iEnd);

    // End points of the group
    OrderingGroupPoint *endpoints[4];
    // Number of endpoints used (either 2 or 4)
    int nEndPoints;
    // bit index in the end counter
    int endbit;
    // bit index in the swap counter
    int swapbit;
};


// Sub-path reordering: do nothing - keep original order
void OrderingOriginal(std::vector<OrderingInfo> &infos);

// Sub-path reordering: reverse every other sub path
void OrderingZigZag(std::vector<OrderingInfo> &infos, bool revfirst);

// Sub-path reordering: continue with the neartest start or end point of yet unused sub paths
void OrderingClosest(std::vector<OrderingInfo> &infos, bool revfirst);

// Global optimization of path length
void OrderingAdvanced(std::vector<OrderingInfo> &infos, int nDims);

} //LPEEmbroderyStitchOrdering
} //namespace LivePathEffect
} //namespace Inkscape

#endif
