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
// world.c -- world query functions

#include "server.h"

/*
===============================================================================

ENTITY AREA CHECKING

FIXME: this use of "area" is different from the bsp file use
===============================================================================
*/

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((qbyte *)l - (int)&(((t *)0)->m)))

#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,r.area)

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

areanode_t	sv_areanodes[AREA_NODES];
int			sv_numareanodes;

float	*area_mins, *area_maxs;
edict_t	**area_list;
int		area_count, area_maxcount;
int		area_type;

struct cmodel_s *SV_CollisionModelForEntity (edict_t *ent);


// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/*
===============
SV_CreateAreaNode

Builds a uniformly subdivided tree for the given world size
===============
*/
areanode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);
	
	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;
	
	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);	
	VectorCopy (mins, mins2);	
	VectorCopy (maxs, maxs1);	
	VectorCopy (maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	
	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	vec3_t mins, maxs;
	struct cmodel_s *cmodel;

	memset (sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;

	cmodel = CM_InlineModel (0);
	CM_InlineModelBounds (cmodel, mins, maxs);
	SV_CreateAreaNode (0, mins, maxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict (edict_t *ent)
{
	if (!ent->r.area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->r.area);
	ent->r.area.prev = ent->r.area.next = NULL;
}


/*
===============
SV_LinkEdict

===============
*/
#define MAX_TOTAL_ENT_LEAFS		128
void SV_LinkEdict (edict_t *ent)
{
	areanode_t	*node;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int			num_leafs;
	int			i, j, k;
	int			area;
	int			topnode;

	if (ent->r.area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position
		
	if (ent == sv.edicts)
		return;		// don't add the world

	if (!ent->r.inuse)
		return;

	// set the size
	VectorSubtract (ent->r.maxs, ent->r.mins, ent->r.size);
	
	// encode the size into the entity_state for client prediction
	if (ent->r.solid == SOLID_BBOX)
	{	// assume that x/y are equal and symetric
		i = ent->r.maxs[0]/8;
		clamp ( i, 1, 31 );

		// z is not symetric
		j = (-ent->r.mins[2])/8;
		clamp ( j, 1, 31 );

		// and z maxs can be negative...
		k = (ent->r.maxs[2]+32)/8;
		clamp ( k, 1, 63 );

		ent->s.solid = (k<<10) | (j<<5) | i;
	}
	else if (ent->r.solid == SOLID_BSP)
	{
		ent->s.solid = SOLID_BMODEL;		// a solid_bbox will never create this value
	}
	else
		ent->s.solid = 0;

	// set the abs box
	if (ent->r.solid == SOLID_BSP && 
		(ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2]) )
	{	// expand for rotation
		float		radius;

		radius = RadiusFromBounds ( ent->r.mins, ent->r.maxs );

		for (i=0 ; i<3 ; i++)
		{
			ent->r.absmin[i] = ent->s.origin[i] - radius;
			ent->r.absmax[i] = ent->s.origin[i] + radius;
		}
	}
	else
	{	// normal
		VectorAdd (ent->s.origin, ent->r.mins, ent->r.absmin);	
		VectorAdd (ent->s.origin, ent->r.maxs, ent->r.absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->r.absmin[0] -= 1;
	ent->r.absmin[1] -= 1;
	ent->r.absmin[2] -= 1;
	ent->r.absmax[0] += 1;
	ent->r.absmax[1] += 1;
	ent->r.absmax[2] += 1;

// link to PVS leafs
	ent->r.num_clusters = 0;
	ent->r.areanum = 0;
	ent->r.areanum2 = 0;

	// get all leafs, including solids
	num_leafs = CM_BoxLeafnums (ent->r.absmin, ent->r.absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// set areas
	for (i=0 ; i<num_leafs ; i++)
	{
		clusters[i] = CM_LeafCluster (leafs[i]);
		area = CM_LeafArea (leafs[i]);
		if (area)
		{	// doors may legally straggle two areas,
			// but nothing should ever need more than that
			if (ent->r.areanum && ent->r.areanum != area)
			{
				if (ent->r.areanum2 && ent->r.areanum2 != area && sv.state == ss_loading)
					Com_DPrintf ("Object touching 3 areas at %f %f %f\n",
					ent->r.absmin[0], ent->r.absmin[1], ent->r.absmin[2]);
				ent->r.areanum2 = area;
			}
			else
				ent->r.areanum = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS)
	{	// assume we missed some leafs, and mark by headnode
		ent->r.num_clusters = -1;
		ent->r.headnode = topnode;
	}
	else
	{
		ent->r.num_clusters = 0;
		for (i=0 ; i<num_leafs ; i++)
		{
			if (clusters[i] == -1)
				continue;		// not a visible leaf
			for (j=0 ; j<i ; j++)
				if (clusters[j] == clusters[i])
					break;
			if (j == i)
			{
				if (ent->r.num_clusters == MAX_ENT_CLUSTERS)
				{	// assume we missed some leafs, and mark by headnode
					ent->r.num_clusters = -1;
					ent->r.headnode = topnode;
					break;
				}

				ent->r.clusternums[ent->r.num_clusters++] = clusters[i];
			}
		}
	}

	// if first time, make sure old_origin is valid
	if (!ent->r.linkcount)
	{
		VectorCopy (ent->s.origin, ent->s.old_origin);
	}
	ent->r.linkcount++;

	if (ent->r.solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->r.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->r.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}
	
	// link it in	
	if (ent->r.solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->r.area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->r.area, &node->solid_edicts);
}


/*
====================
SV_AreaEdicts_r

====================
*/
void SV_AreaEdicts_r (areanode_t *node)
{
	link_t		*l, *next, *start;
	edict_t		*check;
	int			count;

	count = 0;

	// touch linked edicts
	if (area_type == AREA_SOLID)
		start = &node->solid_edicts;
	else
		start = &node->trigger_edicts;

	for (l=start->next  ; l != start ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->r.solid == SOLID_NOT)
			continue;		// deactivated
		if ( !BoundsIntersect (check->r.absmin, check->r.absmax, area_mins, area_maxs) )
			continue;		// not touching

		if (area_count == area_maxcount)
		{
			Com_Printf ("SV_AreaEdicts: MAXCOUNT\n");
			return;
		}

		area_list[area_count] = check;
		area_count++;
	}
	
	if (node->axis == -1)
		return;		// terminal node

	// recurse down both sides
	if (area_maxs[node->axis] > node->dist)
		SV_AreaEdicts_r (node->children[0]);
	if (area_mins[node->axis] < node->dist)
		SV_AreaEdicts_r (node->children[1]);
}

/*
================
SV_SetAreaPortalState

Finds an areaportal leaf entity is connected with, 
and also finds two leafs from different areas connected
with the same entity.
================
*/
void SV_SetAreaPortalState ( edict_t *ent, qboolean open )
{
	// entity must touch at least two areas
	if ( !ent->r.areanum || !ent->r.areanum2 ) {
		return;
	}

	// change areaportal's state
	CM_SetAreaPortalState ( ent->s.number, ent->r.areanum, ent->r.areanum2, open );
}

/*
================
SV_AreaEdicts
================
*/
int SV_AreaEdicts (vec3_t mins, vec3_t maxs, edict_t **list,
	int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	SV_AreaEdicts_r (sv_areanodes);

	return area_count;
}


//===========================================================================

/*
=============
SV_PointContents
=============
*/
int SV_PointContents (vec3_t p)
{
	edict_t		*touch[MAX_EDICTS], *hit;
	int			i, num;
	int			contents, c2;
	struct cmodel_s	*cmodel;
	float		*angles;

	// get base contents from world
	contents = CM_PointContents (p, NULL);

	// or in contents from all the other entities
	num = SV_AreaEdicts (p, p, touch, MAX_EDICTS, AREA_SOLID);

	for (i=0 ; i<num ; i++)
	{
		hit = touch[i];

		// might intersect, so do an exact clip
		cmodel = SV_CollisionModelForEntity (hit);

		if (hit->r.solid != SOLID_BSP)
			angles = vec3_origin;	// boxes don't rotate
		else
			angles = hit->s.angles;

		c2 = CM_TransformedPointContents (p, cmodel, hit->s.origin, hit->s.angles);
		contents |= c2;
	}

	return contents;
}



typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		*trace;
	edict_t		*passedict;
	int			contentmask;
} moveclip_t;



/*
================
SV_CollisionModelForEntity

Returns a collision model that can be used for testing or clipping an
object of mins/maxs size.
================
*/
struct cmodel_s *SV_CollisionModelForEntity (edict_t *ent)
{
	struct cmodel_s	*model;

	if (ent->r.solid == SOLID_BSP)
	{	// explicit hulls in the BSP model
		model = CM_InlineModel (ent->s.modelindex);
		if (!model)
			Com_Error (ERR_FATAL, "MOVETYPE_PUSH with a non bsp model");

		return model;
	}

	// create a temp hull from bounding box sizes
	return CM_ModelForBBox (ent->r.mins, ent->r.maxs);
}


//===========================================================================

/*
====================
SV_ClipMoveToEntities

====================
*/
void SV_ClipMoveToEntities ( moveclip_t *clip )
{
	int			i, num;
	edict_t		*touchlist[MAX_EDICTS], *touch;
	trace_t		trace;
	struct cmodel_s	*cmodel;
	float		*angles;

	num = SV_AreaEdicts (clip->boxmins, clip->boxmaxs, touchlist
		, MAX_EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i=0 ; i<num ; i++)
	{
		touch = touchlist[i];
		if (touch->r.solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (clip->passedict)
		{
		 	if (touch->r.owner == clip->passedict)
				continue;	// don't clip against own missiles
			if (clip->passedict->r.owner == touch)
				continue;	// don't clip against owner
		}

		if ( !(clip->contentmask & CONTENTS_CORPSE)
		&& (touch->s.effects & EF_CORPSE) )
				continue;

		// might intersect, so do an exact clip
		cmodel = SV_CollisionModelForEntity (touch);
		angles = touch->s.angles;
		if (touch->r.solid != SOLID_BSP)
			angles = vec3_origin;	// boxes don't rotate

		if (touch->r.svflags & SVF_MONSTER)
			CM_TransformedBoxTrace (&trace, clip->start, clip->end,
				clip->mins2, clip->maxs2, cmodel, clip->contentmask,
				touch->s.origin, angles);
		else
			CM_TransformedBoxTrace (&trace, clip->start, clip->end,
				clip->mins, clip->maxs, cmodel, clip->contentmask,
				touch->s.origin, angles);

		if (trace.allsolid || trace.fraction < clip->trace->fraction)
		{
			trace.ent = touch->s.number;
			*(clip->trace) = trace;
		}
		else if (trace.startsolid)
			clip->trace->startsolid = qtrue;
		if (clip->trace->allsolid)
			return;
	}
}


/*
==================
SV_TraceBounds
==================
*/
void SV_TraceBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.

Passedict and edicts owned by passedict are explicitly not checked.

==================
*/
void SV_Trace (trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask)
{
	moveclip_t	clip;

	if (!tr)
		return;

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	// clip to world
	CM_BoxTrace (tr, start, end, mins, maxs, NULL, contentmask);
	tr->ent = 0;
	if (tr->fraction == 0)
		return;			// blocked by the world

	memset (&clip, 0, sizeof (moveclip_t));
	clip.trace = tr;
	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passedict = passedict;

	VectorCopy (mins, clip.mins2);
	VectorCopy (maxs, clip.maxs2);
	
	// create the bounding box of the entire move
	SV_TraceBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

	// clip to other solid entities
	SV_ClipMoveToEntities ( &clip );
}

