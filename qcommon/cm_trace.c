/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cmodel_trace.c

#include "qcommon.h"
#include "cm_local.h"

cplane_t		box_planes[6];
cbrushside_t	box_brushsides[6];
cbrush_t		box_brush[1];
cbrush_t		*box_markbrushes[1];
cmodel_t		box_cmodel[1];

/*
===================
CM_InitBoxHull

Set up the planes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull (void)
{
	int			i;
	int			side;
	cplane_t	*p;
	cbrushside_t	*s;

	box_brush->numsides = 6;
	box_brush->brushsides = box_brushsides;
	box_brush->contents = CONTENTS_BODY;

	box_markbrushes[0] = box_brush;

	box_cmodel->nummarkfaces = 0;
	box_cmodel->markfaces = NULL;
	box_cmodel->markbrushes = box_markbrushes;
	box_cmodel->nummarkbrushes = 1;

	for (i=0 ; i<6 ; i++)
	{
		side = i&1;

		// brush sides
		s = box_brushsides + i;
		s->plane = box_planes + i;
		s->surfFlags = 0;

		// planes
		p = &box_planes[i];
		VectorClear (p->normal);

		if ( i&1 ) {
			p->type = PLANE_NONAXIAL;
			p->normal[i>>1] = -1;
			p->signbits = (1<<(i>>1));
		} else {
			p->type = i>>1;
			p->normal[i>>1] = 1;
			p->signbits = 0;
		}
	}
}


/*
===================
CM_ModelForBBox

To keep everything totally uniform, bounding boxes are turned into inline models
===================
*/
cmodel_t *CM_ModelForBBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = -mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = -mins[2];

	VectorCopy (mins, box_cmodel->mins);
	VectorCopy (maxs, box_cmodel->maxs);

	return box_cmodel;
}


/*
==================
CM_PointLeafnum
==================
*/
int CM_PointLeafnum( vec3_t p )
{
	int		num = 0;
	cnode_t	*node;

	if( !numplanes )
		return 0;		// sound may call this without map loaded

	do {
		node = map_nodes + num;
		num = node->children[PlaneDiff (p, node->plane) < 0];
	} while( num >= 0 );

	return -1 - num;
}

/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
int		leaf_count, leaf_maxcount;
int		*leaf_list;
float	*leaf_mins, *leaf_maxs;
int		leaf_topnode;

void CM_BoxLeafnums_r( int nodenum )
{
	int		s;
	cnode_t	*node;

	while( nodenum >= 0 ) {
		node = &map_nodes[nodenum];
		s = BOX_ON_PLANE_SIDE( leaf_mins, leaf_maxs, node->plane ) - 1;

		if( s < 2 ) {
			nodenum = node->children[s];
			continue;
		}

		// go down both sides
		if( leaf_topnode == -1 )
			leaf_topnode = nodenum;
		CM_BoxLeafnums_r( node->children[0] );
		nodenum = node->children[1];
	}

	if( leaf_count < leaf_maxcount )
		leaf_list[leaf_count++] = -1 - nodenum;
}

int	CM_BoxLeafnums( vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode )
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r( 0 );

	if( topnode )
		*topnode = leaf_topnode;
	return leaf_count;
}


/*
==================
CM_BrushContents
==================
*/
inline int CM_BrushContents( cbrush_t *brush, vec3_t p )
{
	int				i;
	cbrushside_t	*brushside;

	for( i = 0, brushside = brush->brushsides; i < brush->numsides; i++, brushside++ )
		if( PlaneDiff( p, brushside->plane ) > 0 )
			return 0;

	return brush->contents;
}

/*
==================
CM_PatchContents
==================
*/
inline int CM_PatchContents( cface_t *patch, vec3_t p )
{
	int			i, c;
	cfacet_t	*facet;

	for( i = 0, facet = patch->facets; i < patch->numfacets; i++, patch++ )
		if( (c = CM_BrushContents( &facet->brush, p )) )
			return c;

	return 0;
}

/*
==================
CM_PointContents
==================
*/
int CM_PointContents( vec3_t p, cmodel_t *cmodel )
{
	int				i, superContents, contents;
	int				nummarkfaces, nummarkbrushes;
	cface_t			*patch, **markface;
	cbrush_t		*brush, **markbrush;

	if( !numnodes )		// map not loaded
		return 0;

	c_pointcontents++;	// optimize counter

	if( !cmodel || cmodel == map_cmodels ) {
		cleaf_t	*leaf;

		leaf = &map_leafs[CM_PointLeafnum( p )];
		superContents = leaf->contents;

		markbrush = leaf->markbrushes;
		nummarkbrushes = leaf->nummarkbrushes;

		markface = leaf->markfaces;
		nummarkfaces = leaf->nummarkfaces;
	} else {
		superContents = ~0;

		markbrush = cmodel->markbrushes;
		nummarkbrushes = cmodel->nummarkbrushes;

		markface = cmodel->markfaces;
		nummarkfaces = cmodel->nummarkfaces;
	}

	contents = superContents;

	for( i = 0; i < nummarkbrushes; i++ ) {
		brush = markbrush[i];

		// check if brush adds something to contents
		if( contents & brush->contents ) {
			if( !(contents &= ~CM_BrushContents( brush, p )) )
				return superContents;
		}
	}

	if( cm_noCurves->integer || !nummarkfaces )
		return ~contents & superContents;

	for( i = 0; i < nummarkfaces; i++ ) {
		patch = markface[i];

		// check if patch adds something to contents
		if( patch->numfacets && (contents & patch->contents) ) {
			if( BoundsIntersect( p, p, patch->mins, patch->maxs ) ) {
				if( !(contents &= ~CM_PatchContents( patch, p )) )
					return superContents;
			}
		}
	}

	return ~contents & superContents;
}

/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int	CM_TransformedPointContents( vec3_t p, cmodel_t *cmodel, vec3_t origin, vec3_t angles )
{
	vec3_t		p_l;
	vec3_t		temp;
	vec3_t		axis[3];

	if( !numnodes )	// map not loaded
		return 0;

	// subtract origin offset
	VectorSubtract( p, origin, p_l );

	// rotate start and end into the models frame of reference
	if( cmodel && (cmodel != box_cmodel) && (angles[0] || angles[1] || angles[2]) ) {
		AnglesToAxis( angles, axis );
		VectorCopy( p_l, temp );
		Matrix_TransformVector( axis, temp, p_l );
	}

	return CM_PointContents( p_l, cmodel );
}


/*
===============================================================================

BOX TRACING

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(1.0f / 32.0f)

vec3_t		trace_start, trace_end;
vec3_t		trace_mins, trace_maxs;
vec3_t		trace_startmins, trace_endmins;
vec3_t		trace_startmaxs, trace_endmaxs;
vec3_t		trace_absmins, trace_absmaxs;
vec3_t		trace_extents;

trace_t		*trace_trace;
int			trace_contents;
qboolean	trace_ispoint;		// optimized case

/*
================
CM_ClipBoxToBrush
================
*/
void CM_ClipBoxToBrush( cbrush_t *brush )
{
	int				i;
	cplane_t		*p, *clipplane;
	float			enterfrac, leavefrac;
	float			d1, d2, f;
	qboolean		getout, startout;
	cbrushside_t	*side, *leadside;

	if( !brush->numsides )
		return;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	c_brush_traces++;

	getout = qfalse;
	startout = qfalse;
	leadside = NULL;
	side = brush->brushsides;

	for( i = 0; i < brush->numsides; i++, side++ ) {
		p = side->plane;

		// push the plane out apropriately for mins/maxs
		if( p->type < 3 ) {
			d1 = trace_startmins[p->type] - p->dist;
			d2 = trace_endmins[p->type] - p->dist;
		} else {
			switch( p->signbits ) {
				case 0:
			d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] - p->dist;
			d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmins[2] - p->dist;
					break;
				case 1:
			d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] - p->dist;
			d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmins[2] - p->dist;
					break;
				case 2:
			d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] - p->dist;
			d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmins[2] - p->dist;
					break;
				case 3:
			d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] - p->dist;
			d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmins[2] - p->dist;
					break;
				case 4:
			d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
			d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
					break;
				case 5:
			d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
			d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
					break;
				case 6:
			d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
			d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
					break;
				case 7:
			d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
			d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
					break;
				default:
					d1 = d2 = 0;	// shut up compiler
					assert( 0 );
					break;
			}
		}

		if( d2 > 0 )
			getout = qtrue;	// endpoint is not in solid
		if( d1 > 0 )
			startout = qtrue;

		// if completely in front of face, no intersection
		if( d1 > 0 && d2 >= d1 )
			return;
		if( d1 <= 0 && d2 <= 0 )
			continue;

		// crosses face
		f = d1 - d2;
		if( f > 0 ) {			// enter
			f = (d1 - DIST_EPSILON) / f;
			if( f > enterfrac ) {
				enterfrac = f;
				clipplane = p;
				leadside = side;
			}
		} else if( f < 0 ) {	// leave
			f = (d1 + DIST_EPSILON) / f;
			if ( f < leavefrac )
				leavefrac = f;
		}
	}

	if( !startout ) {
		// original point was inside brush
		trace_trace->startsolid = qtrue;
		if( !getout )
			trace_trace->allsolid = qtrue;
		return;
	}

	if( enterfrac - (1.0f / 1024.0f) <= leavefrac ) {
		if( enterfrac > -1 && enterfrac < trace_trace->fraction ) {
			if( enterfrac < 0 )
				enterfrac = 0;
			trace_trace->fraction = enterfrac;
			trace_trace->plane = *clipplane;
			trace_trace->surfFlags = leadside->surfFlags;
			trace_trace->contents = brush->contents;
		}
	}
}

/*
================
CM_TestBoxInBrush
================
*/
void CM_TestBoxInBrush( cbrush_t *brush )
{
	int				i;
	cplane_t		*p;
	cbrushside_t	*side;

	if( !brush->numsides )
		return;

	side = brush->brushsides;
	for( i = 0; i < brush->numsides; i++, side++ ) {
		p = side->plane;

		// push the plane out apropriately for mins/maxs
		// if completely in front of face, no intersection
		if( p->type < 3 ) {
			if( trace_startmins[p->type] > p->dist )
				return;
		} else {
			switch( p->signbits ) {
				case 0:
			if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] > p->dist )
				return;
					break;
				case 1:
			if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] > p->dist )
				return;
					break;
				case 2:
			if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] > p->dist )
				return;
					break;
				case 3:
			if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] > p->dist )
				return;
					break;
				case 4:
			if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
				return;
					break;
				case 5:
			if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
				return;
					break;
				case 6:
			if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
				return;
					break;
				case 7:
			if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
				return;
					break;
				default:
					assert( 0 );
					return;
			}
		}
	}

	// inside this brush
	trace_trace->startsolid = trace_trace->allsolid = qtrue;
	trace_trace->fraction = 0;
	trace_trace->contents = brush->contents;
}

/*
================
CM_ClipBox
================
*/
void CM_ClipBox( cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces, int nummarkfaces )
{
	int			i, j;
	cbrush_t	*b;
	cface_t		*patch;
	cfacet_t	*facet;

	// trace line against all brushes
	for( i = 0; i < nummarkbrushes; i++ ) {
		b = markbrushes[i];
		if( b->checkcount == checkcount )
			continue;	// already checked this brush
		b->checkcount = checkcount;
		if( !(b->contents & trace_contents) )
			continue;
		CM_ClipBoxToBrush( b );
		if( !trace_trace->fraction )
			return;
	}

	if( cm_noCurves->integer || !nummarkfaces )
		return;

	// trace line against all patches
	for( i = 0; i < nummarkfaces; i++ ) {
		patch = markfaces[i];
		if( patch->checkcount == checkcount )
			continue;	// already checked this patch
		patch->checkcount = checkcount;
		if( !patch->numfacets || !(patch->contents & trace_contents) )
			continue;
		if( !BoundsIntersect( patch->mins, patch->maxs, trace_absmins, trace_absmaxs ) )
			continue;
		facet = patch->facets;
		for( j = 0; j < patch->numfacets; j++, facet++ ) {
			if( !BoundsIntersect( facet->mins, facet->maxs, trace_absmins, trace_absmaxs ) )
				continue;
			CM_ClipBoxToBrush( &facet->brush );
			if( !trace_trace->fraction )
				return;
		}
	}
}

/*
================
CM_TestBox
================
*/
void CM_TestBox( cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces, int nummarkfaces )
{
	int			i, j;
	cbrush_t	*b;
	cface_t		*patch;
	cfacet_t	*facet;

	// trace line against all brushes
	for( i = 0; i < nummarkbrushes; i++ ) {
		b = markbrushes[i];
		if( b->checkcount == checkcount )
			continue;	// already checked this brush
		b->checkcount = checkcount;
		if( !(b->contents & trace_contents) )
			continue;
		CM_TestBoxInBrush( b );
		if( !trace_trace->fraction )
			return;
	}

	if( cm_noCurves->integer || !nummarkfaces )
		return;

	// trace line against all patches
	for( i = 0; i < nummarkfaces; i++ ) {
		patch = markfaces[i];
		if( patch->checkcount == checkcount )
			continue;	// already checked this patch
		patch->checkcount = checkcount;
		if( !patch->numfacets || !(patch->contents & trace_contents) )
			continue;
		if( !BoundsIntersect( patch->mins, patch->maxs, trace_absmins, trace_absmaxs ) )
			continue;
		facet = patch->facets;
		for( j = 0; j < patch->numfacets; j++, facet++ ) {
			if( !BoundsIntersect( facet->mins, facet->maxs, trace_absmins, trace_absmaxs ) )
				continue;
			CM_TestBoxInBrush( &facet->brush );
			if( !trace_trace->fraction )
				return;
		}
	}
}

/*
==================
CM_RecursiveHullCheck
==================
*/
void CM_RecursiveHullCheck( int num, float p1f, float p2f, vec3_t p1, vec3_t p2 )
{
	cnode_t		*node;
	cplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

loc0:
	if( trace_trace->fraction <= p1f )
		return;		// already hit something nearer

	// if < 0, we are in a leaf node
	if( num < 0 ) {
		cleaf_t	*leaf;

		leaf = &map_leafs[-1 - num];
		if( leaf->contents & trace_contents )
			CM_ClipBox( leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces );
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = map_nodes + num;
	plane = node->plane;

	if( plane->type < 3 ) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	} else {
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
		if( trace_ispoint )
			offset = 0;
		else
			offset = fabs( trace_extents[0] * plane->normal[0]) +
				fabs(trace_extents[1] * plane->normal[1]) +
				fabs(trace_extents[2] * plane->normal[2] );
	}

	// see which sides we need to consider
	if( t1 >= offset && t2 >= offset ) {
		num = node->children[0];
		goto loc0;
	}
	if( t1 < -offset && t2 < -offset ) {
		num = node->children[1];
		goto loc0;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if( t1 < t2 ) {
		idist = 1.0 / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac = (t1 - offset + DIST_EPSILON) * idist;
	} else if( t1 > t2 ) {
		idist = 1.0 / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON) * idist;
		frac = (t1 + offset + DIST_EPSILON) * idist;
	} else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	clamp( frac, 0, 1 );

	midf = p1f + (p2f - p1f) * frac;
	for( i = 0; i < 3; i++ )
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	CM_RecursiveHullCheck( node->children[side], p1f, midf, p1, mid );

	// go past the node
	clamp( frac2, 0, 1 );

	midf = p1f + (p2f - p1f) * frac2;
	for( i = 0; i < 3; i++ )
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);

	CM_RecursiveHullCheck( node->children[side^1], midf, p2f, mid, p2 );
}



//======================================================================

/*
==================
CM_BoxTrace
==================
*/
void CM_BoxTrace( trace_t *tr, vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  cmodel_t *cmodel, int brushmask )
{
	qboolean notworld = (cmodel && (cmodel != map_cmodels));

	if( !tr )
		return;

	checkcount++;		// for multi-check avoidance
	c_traces++;			// for statistics, may be zeroed

	// fill in a default trace
	memset( tr, 0, sizeof( *tr ) );
	tr->fraction = 1;

	if( !numnodes )	// map not loaded
		return;

	trace_trace = tr;
	trace_contents = brushmask;
	VectorCopy( start, trace_start );
	VectorCopy( end, trace_end );
	VectorCopy( mins, trace_mins );
	VectorCopy( maxs, trace_maxs );

	// build a bounding box of the entire move
	ClearBounds( trace_absmins, trace_absmaxs );

	VectorAdd( start, trace_mins, trace_startmins );
	AddPointToBounds( trace_startmins, trace_absmins, trace_absmaxs );

	VectorAdd( start, trace_maxs, trace_startmaxs );
	AddPointToBounds( trace_startmaxs, trace_absmins, trace_absmaxs );

	VectorAdd( end, trace_mins, trace_endmins );
	AddPointToBounds( trace_endmins, trace_absmins, trace_absmaxs );

	VectorAdd( end, trace_maxs, trace_endmaxs );
	AddPointToBounds( trace_endmaxs, trace_absmins, trace_absmaxs );

	//
	// check for position test special case
	//
	if( VectorCompare( start, end ) ) {
		int		leafs[1024];
		int		i, numleafs;
		vec3_t	c1, c2;
		int		topnode;
		cleaf_t	*leaf;

		if( notworld ) {
			if( BoundsIntersect( cmodel->mins, cmodel->maxs, trace_absmins, trace_absmaxs ) )
				CM_TestBox( cmodel->markbrushes, cmodel->nummarkbrushes, cmodel->markfaces, cmodel->nummarkfaces );
		} else {
			for( i = 0; i < 3; i++ ) {
				c1[i] = start[i] + mins[i] - 1;
				c2[i] = start[i] + maxs[i] + 1;
			}

			numleafs = CM_BoxLeafnums( c1, c2, leafs, 1024, &topnode );
			for( i = 0; i < numleafs; i++ ) {
				leaf = &map_leafs[leafs[i]];

				if( leaf->contents & trace_contents ) {
					CM_TestBox( leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces );
					if( tr->allsolid )
						break;
				}
			}
		}

		VectorCopy( start, tr->endpos );
		return;
	}

	//
	// check for point special case
	//
	if( VectorCompare( mins, vec3_origin ) && VectorCompare( maxs, vec3_origin ) ) {
		trace_ispoint = qtrue;
		VectorClear( trace_extents );
	} else {
		trace_ispoint = qfalse;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	//
	// general sweeping through world
	//
	if( !notworld ) {
		CM_RecursiveHullCheck( 0, 0, 1, start, end );
	} else {
		if( BoundsIntersect( cmodel->mins, cmodel->maxs, trace_absmins, trace_absmaxs ) )
			CM_ClipBox( cmodel->markbrushes, cmodel->nummarkbrushes, cmodel->markfaces, cmodel->nummarkfaces );
	}

	if( tr->fraction == 1 ) {
		VectorCopy( end, tr->endpos );
	} else {
		tr->endpos[0] = start[0] + tr->fraction * (end[0] - start[0]);
		tr->endpos[1] = start[1] + tr->fraction * (end[1] - start[1]);
		tr->endpos[2] = start[2] + tr->fraction * (end[2] - start[2]);
	}
}


/*
==================
CM_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
void CM_TransformedBoxTrace( trace_t *tr, vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  cmodel_t *cmodel, int brushmask,
						  vec3_t origin, vec3_t angles )
{
	vec3_t		start_l, end_l;
	vec3_t		a, temp;
	vec3_t		axis[3];
	qboolean	rotated;

	if( !tr )
		return;

	// subtract origin offset
	VectorSubtract( start, origin, start_l );
	VectorSubtract( end, origin, end_l );

	// rotate start and end into the models frame of reference
	if( cmodel && (cmodel != box_cmodel) && (angles[0] || angles[1] || angles[2]) )
		rotated = qtrue;
	else
		rotated = qfalse;

	if( rotated ) {
		AnglesToAxis( angles, axis );

		VectorCopy( start_l, temp );
		Matrix_TransformVector( axis, temp, start_l );

		VectorCopy( end_l, temp );
		Matrix_TransformVector( axis, temp, end_l );
	}

	// sweep the box through the model
	CM_BoxTrace( tr, start_l, end_l, mins, maxs, cmodel, brushmask );

	if( rotated && tr->fraction != 1.0 ) {
		VectorNegate( angles, a );
		AnglesToAxis( a, axis );

		VectorCopy( tr->plane.normal, temp );
		Matrix_TransformVector( axis, temp, tr->plane.normal );
	}

	if( tr->fraction == 1 ) {
		VectorCopy( end, tr->endpos );
	} else {
		tr->endpos[0] = start[0] + tr->fraction * (end[0] - start[0]);
		tr->endpos[1] = start[1] + tr->fraction * (end[1] - start[1]);
		tr->endpos[2] = start[2] + tr->fraction * (end[2] - start[2]);
	}
}
