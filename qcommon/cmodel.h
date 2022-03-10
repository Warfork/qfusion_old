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

extern	int c_traces, c_brush_traces;
extern	int	c_pointcontents;

extern cvar_t	*cm_noAreas;
extern cvar_t	*cm_noCurves;

struct cmodel_s	*CM_LoadMap (char *name, qboolean clientload, unsigned *checksum);
struct cmodel_s	*CM_InlineModel (int num);	// 1, 2, etc
qboolean CM_ClientLoad (void);

int			CM_NumClusters (void);
int			CM_NumInlineModels (void);
char		*CM_EntityString (void);

// creates a clipping hull for an arbitrary bounding box
struct cmodel_s	*CM_ModelForBBox (vec3_t mins, vec3_t maxs);
void		CM_InlineModelBounds (struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs);


// returns an ORed contents mask
int			CM_PointContents (vec3_t p, struct cmodel_s *cmodel);
int			CM_TransformedPointContents (vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles);

void		CM_BoxTrace (trace_t *tr, vec3_t start, vec3_t end,
					  vec3_t mins, vec3_t maxs,
					  struct cmodel_s *cmodel, int brushmask);
void		CM_TransformedBoxTrace (trace_t *tr, vec3_t start, vec3_t end,
					  vec3_t mins, vec3_t maxs,
		   			  struct cmodel_s *cmodel, int brushmask,
		    		  vec3_t origin, vec3_t angles);

qbyte		*CM_ClusterPVS (int cluster);
qbyte		*CM_ClusterPHS (int cluster);
int			CM_ClusterSize (void);

int			CM_PointLeafnum (vec3_t p);

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int			CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list,
						int listsize, int *topnode);

int			CM_LeafCluster (int leafnum);
int			CM_LeafArea (int leafnum);

void		CM_SetAreaPortalState (int portalnum, int area, int otherarea, qboolean open);
qboolean	CM_AreasConnected (int area1, int area2);

int			CM_WriteAreaBits (qbyte *buffer, int area);
void		CM_MergeAreaBits (qbyte *buffer, int area);
qboolean	CM_HeadnodeVisible (int headnode, qbyte *visbits);

void		CM_WritePortalState (FILE *f);
void		CM_ReadPortalState (FILE *f);
