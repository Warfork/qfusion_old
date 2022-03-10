
#include "gl_local.h"

typedef struct clipsurf_s
{
	struct clipsurf_s *next, *prev;
	int state;
	int visible;
	int solid;
	int removed;
	void (*callback)(void *nativedata, void *nativedata2);
	void *nativedata;
	void *nativedata2;
	float wstepx, wstepy, w00;
	// wcurrent is a cached copy of w00 + wstepy * y,
	// updated each time the surface is added to the stack,
	// for quicker comparisons.
	float wcurrent;
}
clipsurf_t;

typedef struct clipedge_s
{
	float x, realx, realxstep;
	struct clipedge_s *next, *prev, *nextremove;
	clipsurf_t *psurf;
	int leading;
	int pad;
}
clipedge_t;

clipsurf_t *pavailsurf, *clipsurfs, *clipsurfsend;
clipedge_t *pavailedge, *clipedges, *clipedgesend, *newedges, **removeedges;

clipsurf_t surfstack;
clipedge_t edgehead, edgetail;
clipedge_t maxedge = {2000000000.0f};

cvar_t *r_clipwidth;
cvar_t *r_clipheight;
cvar_t *r_clipedges;
cvar_t *r_clipsurfaces;

int clipwidth = 0, clipheight = 0;
int maxclipsurfs = 0, maxclipedges = 0;
int needededges, neededsurfs;

#if CLIPTEST
typedef struct
{
	float w; // inverse depth (1/z)
}
clippixel_t;

clippixel_t *clipbuffer;
#endif

float r_clip_viewmatrix[3][3], r_clip_viewmulx, r_clip_viewmuly, r_clip_viewcenterx, r_clip_viewcentery;
cplane_t r_clip_viewplane[5];

void R_Clip_MakeViewMatrix(void)
{
	float pixelaspect, screenaspect, horizontalfieldofview, verticalfieldofview;

	pixelaspect = (float) clipheight / (float) clipwidth * 320 / 240.0;
	horizontalfieldofview = 2.0 * tan (r_newrefdef.fov_x/360*M_PI);
	screenaspect = clipwidth * pixelaspect / clipheight;
	verticalfieldofview = horizontalfieldofview / screenaspect;

	r_clip_viewcenterx = clipwidth * 0.5 - 0.5;
	r_clip_viewcentery = clipheight * 0.5 - 0.5;
	r_clip_viewmulx = clipwidth / horizontalfieldofview;
	r_clip_viewmuly = r_clip_viewmulx * pixelaspect;

	// this constructs a transposed rotation matrix for the view (transposed matrices do the opposite of their normal behavior)
	VectorCopy ( vright, r_clip_viewmatrix[0] );
	VectorNegate ( vup, r_clip_viewmatrix[1] );
	VectorCopy ( vpn, r_clip_viewmatrix[2] );
	VectorCopy ( vpn, r_clip_viewplane[0].normal );

	r_clip_viewplane[0].dist = DotProduct(r_origin, vpn);

	memcpy( &r_clip_viewplane[1], &frustum[0], sizeof(cplane_t) );
	memcpy( &r_clip_viewplane[2], &frustum[1], sizeof(cplane_t) );
	memcpy( &r_clip_viewplane[3], &frustum[2], sizeof(cplane_t) );
	memcpy( &r_clip_viewplane[4], &frustum[3], sizeof(cplane_t) );
}

void R_Clip_StartFrame(void)
{
	int i;
	int newwidth, newheight, newmaxedges, newmaxsurfs;

	newwidth = (int)r_clipwidth->value;
	newheight = (int)r_clipheight->value;
	newmaxedges = (int)r_clipedges->value;
	newmaxsurfs = (int)r_clipsurfaces->value;

	if ( newwidth < 80 ) {
		newwidth = 80;
	} else if ( newwidth > vid.width * 2 ) {
		newwidth = vid.width * 2;
	}

	if ( newheight < 60 ) {
		newheight = 60;
	} else if ( newheight > vid.height * 2 ) {
		newheight = vid.height * 2;
	}

	if ( newmaxedges < 128 ) {
		newmaxedges = 128;
	} else if ( newmaxedges > 262144 ) {
		newmaxedges = 262144;
	}

	if ( newmaxsurfs < 32 ) {
		newmaxsurfs = 32;
	} else if ( newmaxsurfs > 65536 ) {
		newmaxsurfs = 65536;
	}

	if ( newwidth != clipwidth || newheight != clipheight || maxclipedges != newmaxedges || maxclipsurfs != newmaxsurfs ) {
#if CLIPTEST
		if (clipbuffer)
			Z_Free( clipbuffer );
#endif
		if (clipedges)
			Z_Free( clipedges );
		if (clipsurfs)
			Z_Free( clipsurfs );
		if (newedges)
			Z_Free( newedges );
		if (removeedges)
			Z_Free( removeedges );

		clipwidth = newwidth;
		clipheight = newheight;
		maxclipedges = newmaxedges;
		maxclipsurfs = newmaxsurfs;

#if CLIPTEST
		clipbuffer = Z_Malloc( clipwidth * clipheight * sizeof(clippixel_t) );
#endif

		clipedges = Z_Malloc( maxclipedges * sizeof(clipedge_t) );
		clipsurfs = Z_Malloc( maxclipsurfs * sizeof(clipsurf_t) );
		newedges = Z_Malloc( clipheight * sizeof(clipedge_t));
		removeedges = Z_Malloc( clipheight * sizeof(clipedge_t *) );
		clipedgesend = clipedges + maxclipedges;
		clipsurfsend = clipsurfs + maxclipsurfs;
	}

#if CLIPTEST
	memset( clipbuffer, 0, clipwidth * clipheight * sizeof(clippixel_t) );
#endif

	pavailedge = clipedges;
	pavailsurf = clipsurfs;

	// Clear the lists of edges to add and remove on each scan line.
	needededges = 0;
	neededsurfs = 0;
	for (i = 0;i < clipheight;i++)
	{
		newedges[i].next = &maxedge;
		removeedges[i] = NULL;
	}

	R_Clip_MakeViewMatrix();
}

void ScanEdges (void);

void R_Clip_EndFrame(void)
{
	ScanEdges ();

	if (maxclipedges < needededges)
	{
		Com_Printf( "R_Clip: ran out of edges, increasing limit from %d to %d\n", maxclipedges, needededges );
		Cvar_SetValue( "r_clipedges", needededges);
	}

	if (maxclipsurfs < neededsurfs)
	{
		Com_Printf( "R_Clip: ran out of surfaces, increasing limit from %d to %d\n", maxclipsurfs, neededsurfs );
		Cvar_SetValue( "r_clipsurfaces", neededsurfs);
	}
}

void R_Clip_Init(void)
{
	r_clipwidth = Cvar_Get ( "r_clipwidth", "800", 0 );
	r_clipheight = Cvar_Get ( "r_clipheight", "600", 0 );
	r_clipedges = Cvar_Get ( "r_clipedges", "32768", CVAR_ARCHIVE );
	r_clipsurfaces = Cvar_Get ( "r_clipsurfaces", "8192", CVAR_ARCHIVE );
}

int R_Clip_TriangleToPlane(vec3_t point1, vec3_t point2, vec3_t point3, cplane_t *p)
{
	float y, number;
	vec3_t v1, v2;
	VectorSubtract(point1, point2, v1);
	VectorSubtract(point3, point2, v2);
	CrossProduct(v1, v2, p->normal);
	number = DotProduct(p->normal, p->normal);
	if (number >= 0.1f)
	{
		*((int *)&y) = 0x5f3759df - ((* (int *) &number) >> 1);
		y = y * (1.5f - (number * 0.5f * y * y));
		VectorScale(p->normal, y, p->normal);
		p->dist = DotProduct(point1, p->normal);
		return true;
	}
	else
		return false;
}


int R_Clip_ClipPolygonToPlane(float *in, float *out, int inpoints, int stride, cplane_t *plane)
{
	int i, outpoints, prevside, side;
	float *prevpoint, prevdist, dist, dot;

	// begin with the last point, then enter the loop with the first point as current
	prevpoint = (float *) ((byte *)in + stride * (inpoints - 1));
	prevdist = DotProduct(prevpoint, plane->normal) - plane->dist;
	prevside = prevdist >= 0 ? SIDE_FRONT : SIDE_BACK;
	i = 0;
	outpoints = 0;
	goto begin;
	for (;i < inpoints;i++)
	{
		prevpoint = in;
		prevdist = dist;
		prevside = side;
		(byte *)in += stride;

begin:
		dist = DotProduct(in, plane->normal) - plane->dist;
		side = dist >= 0 ? SIDE_FRONT : SIDE_BACK;

		if (prevside == SIDE_FRONT)
		{
			VectorCopy(prevpoint, out);
			out += 3;
			outpoints++;
			if (side == SIDE_FRONT)
				continue;
		}
		else if (side == SIDE_BACK)
			continue;

		// generate a split point
		dot = prevdist / (prevdist - dist);
		out[0] = prevpoint[0] + dot * (in[0] - prevpoint[0]);
		out[1] = prevpoint[1] + dot * (in[1] - prevpoint[1]);
		out[2] = prevpoint[2] + dot * (in[2] - prevpoint[2]);
		out += 3;
		outpoints++;
	}

	return outpoints;
}

float points[256*3];
float tempverts[256][3];
float tempverts2[256][3];
float screenverts[256][3];
qboolean pointsset;

// LordHavoc: this code is based primarily on the ddjzsort code

// Clips polygon to view frustum and nearclip, transforms polygon to viewspace, perspective projects polygon to screenspace,
// and adds polygon's edges to the global edge table.
void R_Clip_AddPolygon (mvertex_t *verts, int numverts, int stride, int solid, void (*callback)(void *nativedata, void *nativedata2), void *nativedata, void *nativedata2, cplane_t *polyplane)
{
	float deltax, deltay, vx, vy, vz, fx;
	int i, j, k, nextvert, temp, topy, bottomy, height, addededges;
	clipedge_t *pedge;
	cplane_t localplane;
	float distinv;

	if ( !numverts ) {
		return;
	}
	
	if ( !pointsset ) {
		for ( i = 0; i < numverts; i++ ) {
			points[i*3+0] = verts[i].position[0];
			points[i*3+1] = verts[i].position[1];
			points[i*3+2] = verts[i].position[2];
		}
	}

	pointsset = false;

	if (polyplane == NULL)
	{
		polyplane = &localplane;
		// calculate the plane for the polygon
		if (!R_Clip_TriangleToPlane((float *) points, (float *) ((byte *)points + stride), (float *) ((byte *)points + 2 * stride), polyplane))
		{
			for (i = 0;i < numverts;i++)
				for (j = i + 1;j < numverts;j++)
					for (k = j + 1;k < numverts;k++)
						if (R_Clip_TriangleToPlane((float *) ((byte *)points + i * stride), (float *) ((byte *)points + j * stride), (float *) ((byte *)points + k * stride), polyplane))
							goto valid1;
			return; // gave up
			valid1:;
		}

		// caller hasn't checked if this polygon faces the view, so we have to check
		if (DotProduct(r_origin, polyplane->normal) < (polyplane->dist + 0.5f))
			return;
	}

	// for adaptive limits
	needededges += numverts;
	neededsurfs++;

	if (pavailsurf >= clipsurfsend)
		return;

	// clip to view frustum and nearclip
	if (numverts < 3) return;numverts = R_Clip_ClipPolygonToPlane(points      , tempverts2[0], numverts, stride, &r_clip_viewplane[0]);
	if (numverts < 3) return;numverts = R_Clip_ClipPolygonToPlane(tempverts2[0], tempverts[0], numverts, sizeof(float) * 3, &r_clip_viewplane[1]);
	if (numverts < 3) return;numverts = R_Clip_ClipPolygonToPlane(tempverts[0], tempverts2[0], numverts, sizeof(float) * 3, &r_clip_viewplane[2]);
	if (numverts < 3) return;numverts = R_Clip_ClipPolygonToPlane(tempverts2[0], tempverts[0], numverts, sizeof(float) * 3, &r_clip_viewplane[3]);
	if (numverts < 3) return;numverts = R_Clip_ClipPolygonToPlane(tempverts[0], tempverts2[0], numverts, sizeof(float) * 3, &r_clip_viewplane[4]);
	if (numverts < 3)
		return;
	if (numverts > 256)
		Sys_Error("R_Clip_AddPolygon: polygon exceeded 256 vertex buffer\n");

	// it survived the clipping, transform to viewspace and project to screenspace

	if (pavailedge + numverts > clipedgesend)
		return;

	for (i = 0;i < numverts;i++)
	{
		vx = tempverts2[i][0] - r_origin[0];
		vy = tempverts2[i][1] - r_origin[1];
		vz = tempverts2[i][2] - r_origin[2];
		screenverts[i][2] = 1.0f / (r_clip_viewmatrix[2][0] * vx + r_clip_viewmatrix[2][1] * vy + r_clip_viewmatrix[2][2] * vz);
		screenverts[i][0] = (r_clip_viewmatrix[0][0] * vx + r_clip_viewmatrix[0][1] * vy + r_clip_viewmatrix[0][2] * vz) * r_clip_viewmulx * screenverts[i][2] + r_clip_viewcenterx;
		screenverts[i][1] = (r_clip_viewmatrix[1][0] * vx + r_clip_viewmatrix[1][1] * vy + r_clip_viewmatrix[1][2] * vz) * r_clip_viewmuly * screenverts[i][2] + r_clip_viewcentery;
	}

	// calculate the plane for the polygon
	if (!R_Clip_TriangleToPlane(screenverts[0], screenverts[1], screenverts[2], &localplane))
	{
		for (i = 0;i < numverts;i++)
			for (j = i + 1;j < numverts;j++)
				for (k = j + 1;k < numverts;k++)
					if (R_Clip_TriangleToPlane(screenverts[i], screenverts[j], screenverts[k], &localplane))
						goto valid;
		return; // gave up
valid:;
	}

	// Set up the 1/z gradients from the polygon, calculating the
	// base value at screen coordinate 0,0 so we can use screen
	// coordinates directly when calculating 1/z from the gradients
	distinv = 1.0f / localplane.normal[2];
	pavailsurf->wstepx = -(localplane.normal[0] * distinv);
	pavailsurf->wstepy = -(localplane.normal[1] * distinv);
	pavailsurf->w00 = localplane.dist * distinv;

	addededges = false;

	// Add each edge in turn
	for (i = 0;i < numverts;i++)
	{
		nextvert = i + 1;
		if (nextvert >= numverts)
			nextvert = 0;

		topy = (int)ceil(screenverts[i][1]);
		bottomy = (int)ceil(screenverts[nextvert][1]);
		height = bottomy - topy;
		if (height == 0)
			continue;       // doesn't cross any scan lines
		if (height < 0)
		{
			// Leading edge
			temp = topy;
			topy = bottomy;
			bottomy = temp;
			if (topy < 0)
				topy = 0;
			if (bottomy > clipheight)
				bottomy = clipheight;
			if (topy >= bottomy)
				continue;

			pavailedge->leading = 1;

			deltax = screenverts[i][0] - screenverts[nextvert][0];
			deltay = screenverts[i][1] - screenverts[nextvert][1];

			pavailedge->realxstep = deltax / deltay;
			pavailedge->realx = screenverts[nextvert][0] + ((float)topy - screenverts[nextvert][1]) * pavailedge->realxstep;
		}
		else
		{
			// Trailing edge
			if (topy < 0)
				topy = 0;
			if (bottomy > clipheight)
				bottomy = clipheight;
			if (topy >= bottomy)
				continue;

			pavailedge->leading = 0;

			deltax = screenverts[nextvert][0] - screenverts[i][0];
			deltay = screenverts[nextvert][1] - screenverts[i][1];

			pavailedge->realxstep = deltax / deltay;
			pavailedge->realx = screenverts[i][0] + ((float)topy - screenverts[i][1]) * pavailedge->realxstep;
		}

		// Put the edge on the list to be added on top scan
		fx = pavailedge->realx;
		
		if ( fx < 0.0f ) {
			fx = 0.0f;
		} else if ( fx > clipwidth - 0.5f ) {
			fx = clipwidth - 0.5f;
		}
		pavailedge->x = fx;

		pedge = &newedges[topy];
		while (fx > pedge->next->x)
			pedge = pedge->next;
		pavailedge->next = pedge->next;
		pedge->next = pavailedge;

		// Put the edge on the list to be removed after final scan
		pavailedge->nextremove = removeedges[bottomy - 1];
		removeedges[bottomy - 1] = pavailedge;

		// Associate the edge with the surface
		pavailedge->psurf = pavailsurf;

		pavailedge++;

		addededges = true;
	}

	if (!addededges)
		return;

	// Create the surface, so we'll know how to sort and draw from the edges
	pavailsurf->next = NULL;
	pavailsurf->prev = NULL;
	pavailsurf->state = 0;
	pavailsurf->visible = false;
	pavailsurf->callback = callback;
	pavailsurf->nativedata = nativedata;
	pavailsurf->nativedata2 = nativedata2;
	pavailsurf->solid = solid;
	pavailsurf->removed = false;
	pavailsurf++;
}

// Scan all the edges in the global edge table into spans.
void ScanEdges (void)
{
	int y, rescan;
	float fx, fy, w, w2, clipwidthf = clipwidth - 0.5f;
	clipedge_t *pedge, *pedge2, *ptemp;
	clipsurf_t *psurf, *psurf2;
#if CLIPTEST
	int x, x2;
	float zi;
	clippixel_t *cb;
#endif
	float cx;

	// Set up the active edge list as initially empty, containing
	// only the sentinels (which are also the background fill). Most
	// of these fields could be set up just once at start-up
	edgehead.next = &edgetail;
	edgehead.prev = NULL;
	edgehead.x = edgehead.realx = -0.9999f; // left edge of screen
	edgehead.realxstep = 0;
	edgehead.leading = 1;
	edgehead.psurf = &surfstack;

	edgetail.next = NULL; // mark end of list
	edgetail.prev = &edgehead;
	edgetail.x = edgetail.realx = clipwidth + 0.5f; // right edge of screen
	edgetail.realxstep = 0;
	edgetail.leading = 0;
	edgetail.psurf = &surfstack;

	// The background surface is the entire stack initially, and
	// is infinitely far away, so everything sorts in front of it.
	// This could be set just once at start-up
	surfstack.solid = true;
	surfstack.visible = true; // no callback
	surfstack.next = surfstack.prev = &surfstack;
	surfstack.wcurrent = surfstack.w00 = -999999.0;
	surfstack.wstepx = surfstack.wstepy = 0.0;
	surfstack.removed = false;

	// rescan causes the edges to be compared at the span level
	// it is false if the scanline will be identical to the previous
	rescan = true;
	for (y = 0;y < clipheight;y++)
	{
		fy = y;
#if CLIPTEST
		cb = clipbuffer + y * clipwidth;
#endif

		// Sort in any edges that start on this scan
		if (newedges[y].next != &maxedge)
		{
			rescan = true;
			pedge = newedges[y].next;
			pedge2 = &edgehead;
			while (pedge != &maxedge)
			{
				if (pedge->psurf->removed)
				{
					pedge = pedge->next;
					continue;
				}

				while (pedge->x > pedge2->next->x)
					pedge2 = pedge2->next;

				ptemp = pedge->next;
				pedge->next = pedge2->next;
				pedge->prev = pedge2;
				pedge2->next->prev = pedge;
				pedge2->next = pedge;

				pedge2 = pedge;
				pedge = ptemp;
			}
		}

		// Scan out the active edges into spans

		// Start out with the left background edge already inserted, and the surface stack containing only the background
		surfstack.state = 1;
		cx = 0;

		// must always rescan if rendering to wbuffer
#ifndef CLIPTEST
//		if (rescan)
#endif
		{
			for (pedge = edgehead.next;pedge;pedge = pedge->next)
			{
				edgeremoved:
				psurf = pedge->psurf;
				if (psurf->removed)
				{
					pedge2 = pedge->next;
					pedge->prev->next = pedge->next;
					pedge->next->prev = pedge->prev;
					pedge->next = pedge->prev = pedge;
					pedge = pedge2;
					if (pedge)
						goto edgeremoved;
					else
						break;
				}

				if (pedge->leading)
				{
					// It's a leading edge. Figure out where it is
					// relative to the current surfaces and insert in
					// the surface stack; if it's on top, emit the span
					// for the current top.
					// First, make sure the edges don't cross
					if (++psurf->state == 1)
					{
						fx = pedge->x;
						// Calculate the surface's 1/z value at this pixel, and cache the y depth for quick compares later
						w = (psurf->wcurrent = psurf->w00 + psurf->wstepy * fy) + psurf->wstepx * fx;

						// See if that makes it a new top surface
						psurf2 = surfstack.next;
						w2 = psurf2->wcurrent + psurf2->wstepx * fx;

						if (w >= w2)
						{
							// It's a new top surface
							// emit the span for the current top
							if (fx > cx && !psurf2->visible)
							{
								psurf2->visible = true;
								psurf2->callback(psurf2->nativedata, psurf2->nativedata2);
							}

#if CLIPTEST
							for (x = ceil(cx), x2 = ceil(fx) >= clipwidth ? clipwidth : ceil(fx), zi = psurf2->wcurrent + psurf2->wstepx * x;x < x2;x++, zi += psurf2->wstepx)
								cb[x].w = zi;
#endif

							cx = fx;

							// Add the edge to the stack
							psurf->next = psurf2;
							psurf2->prev = psurf;
							surfstack.next = psurf;
							psurf->prev = &surfstack;
						}
						else
						{
							// Not a new top; sort into the surface stack.
							// Guaranteed to terminate due to sentinel background surface
							do
							{
								psurf2 = psurf2->next;
								w2 = psurf2->wcurrent + psurf2->wstepx * fx;
							}
							while (w < w2);

							// Insert the surface into the stack
							psurf->next = psurf2;
							psurf->prev = psurf2->prev;
							psurf2->prev->next = psurf;
							psurf2->prev = psurf;
						}
					}
				}
				else
				{
					// It's a trailing edge; if this was the top surface,
					// emit the span and remove it.
					// First, make sure the edges didn't cross
					if (--psurf->state == 0)
					{
						if (surfstack.next == psurf)
						{
							fx = pedge->x;

							// It's on top, emit the span
							if (fx > cx && !psurf->visible)
							{
								psurf->visible = true;
								psurf->callback(psurf->nativedata, psurf->nativedata2);
							}

#if CLIPTEST
							fx = pedge->x;
							for (x = ceil(cx), x2 = ceil(fx) >= clipwidth ? clipwidth : ceil(fx), zi = psurf->w00 + psurf->wstepx * x + psurf->wstepy * fy;x < x2;x++, zi += psurf->wstepx)
								cb[x].w = zi;
#endif

							cx = fx;
						}

						// Remove the surface from the stack
						psurf->next->prev = psurf->prev;
						psurf->prev->next = psurf->next;
					}
				}

				// mark and remove all non-solid surfaces that are ontop
				while (!surfstack.next->solid)
				{
					psurf = surfstack.next;
					if (!psurf->visible)
					{
						psurf->visible = true;
						psurf->callback(psurf->nativedata, psurf->nativedata2);
					}
					psurf->removed = true;
					psurf->next->prev = psurf->prev;
					psurf->prev->next = psurf->next;
					// isolate the surface
					psurf->next = psurf->prev = psurf;
				}
			}
			rescan = false;
		}

		// Remove edges that are done
		pedge = removeedges[y];
		if (pedge)
		{
			while (pedge)
			{
				if (!pedge->psurf->removed)
				{
					pedge->prev->next = pedge->next;
					pedge->next->prev = pedge->prev;
					if (pedge->psurf->visible)
						rescan = true;
				}
				pedge = pedge->nextremove;
			}
		}

		// Step the remaining edges one scan line, and re-sort
		for (pedge = edgehead.next;pedge != &edgetail;)
		{
			ptemp = pedge->next;
			if (pedge->psurf->removed)
			{
				pedge->next->prev = pedge->prev;
				pedge->prev->next = pedge->next;
				pedge->next = pedge->prev = pedge;
				pedge = ptemp;
				continue;
			}

			// Step the edge
			if (pedge->realxstep)
			{
				pedge->realx += pedge->realxstep;
				pedge->x = pedge->realx;
				
				if ( pedge->x < 0.0f ) {
					pedge->x = 0.0f;
				} else if ( pedge->x > clipwidth ) {
					pedge->x = clipwidth;
				}
			}
			fx = pedge->x;

			// Move the edge back to the proper sorted location, if necessary
			while (fx < pedge->prev->x)
			{
				if (!rescan && (pedge->psurf->solid || pedge->prev->psurf->solid))
					rescan = true;
				pedge2 = pedge->prev;
				pedge2->next = pedge->next;
				pedge->next->prev = pedge2;
				pedge2->prev->next = pedge;
				pedge->prev = pedge2->prev;
				pedge->next = pedge2;
				pedge2->prev = pedge;
			}

			pedge = ptemp;
		}
	}
}

#define R_Clip_MinsBoxPolygon(axis, axisvalue, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, callback, nativedata, nativedata2, plane) \
{\
	if (r_origin[(axis)] < ((axisvalue) - 0.5f))\
	{\
		(plane)->dist = -axisvalue;\
		points[ 0] = x1;points[ 1] = y1;points[ 2] = z1;\
		points[ 3] = x2;points[ 4] = y2;points[ 5] = z2;\
		points[ 6] = x3;points[ 7] = y3;points[ 8] = z3;\
		points[ 9] = x4;points[10] = y4;points[11] = z4;\
		pointsset = true;\
		R_Clip_AddPolygon (NULL, 4, sizeof(float[3]), false, callback, nativedata, nativedata2, plane);\
	}\
}

#define R_Clip_MaxsBoxPolygon(axis, axisvalue, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, callback, nativedata, nativedata2, plane) \
{\
	if (r_origin[(axis)] > ((axisvalue) + 0.5f))\
	{\
		(plane)->dist = axisvalue;\
		points[ 0] = x1;points[ 1] = y1;points[ 2] = z1;\
		points[ 3] = x2;points[ 4] = y2;points[ 5] = z2;\
		points[ 6] = x3;points[ 7] = y3;points[ 8] = z3;\
		points[ 9] = x4;points[10] = y4;points[11] = z4;\
		pointsset = true;\
		R_Clip_AddPolygon (NULL, 4, sizeof(float[3]), false, callback, nativedata, nativedata2, plane);\
	}\
}

cplane_t clipboxplane[6] =
{
	{{-1,  0,  0}, 0},
	{{ 1,  0,  0}, 0},
	{{ 0, -1,  0}, 0},
	{{ 0,  1,  0}, 0},
	{{ 0,  0, -1}, 0},
	{{ 0,  0,  1}, 0},
};

void R_Clip_AddBox(float *a, float *b, void (*callback)(void *nativedata, void *nativedata2), void *nativedata, void *nativedata2)
{
	if (r_origin[0] >= (a[0] - 5.0f) && r_origin[0] < (b[0] + 5.0f)
	 && r_origin[1] >= (a[1] - 5.0f) && r_origin[1] < (b[1] + 5.0f)
	 && r_origin[2] >= (a[2] - 5.0f) && r_origin[2] < (b[2] + 5.0f))
	{
		callback(nativedata, nativedata2);
		return;
	}

	R_Clip_MinsBoxPolygon
	(
		0, a[0],
		a[0], a[1], a[2],
		a[0], b[1], a[2],
		a[0], b[1], b[2],
		a[0], a[1], b[2],
		callback, nativedata, nativedata2, &clipboxplane[0]
	);
	R_Clip_MaxsBoxPolygon
	(
		0, b[0],
		b[0], b[1], a[2],
		b[0], a[1], a[2],
		b[0], a[1], b[2],
		b[0], b[1], b[2],
		callback, nativedata, nativedata2, &clipboxplane[1]
	);
	R_Clip_MinsBoxPolygon
	(
		1, a[1],
		b[0], a[1], a[2],
		a[0], a[1], a[2],
		a[0], a[1], b[2],
		b[0], a[1], b[2],
		callback, nativedata, nativedata2, &clipboxplane[2]
	);
	R_Clip_MaxsBoxPolygon
	(
		1, b[1],
		a[0], b[1], a[2],
		b[0], b[1], a[2],
		b[0], b[1], b[2],
		a[0], b[1], b[2],
		callback, nativedata, nativedata2, &clipboxplane[3]
	);
	R_Clip_MinsBoxPolygon
	(
		2, a[2],
		a[0], a[1], a[2],
		b[0], a[1], a[2],
		b[0], b[1], a[2],
		a[0], b[1], a[2],
		callback, nativedata, nativedata2, &clipboxplane[4]
	);
	R_Clip_MaxsBoxPolygon
	(
		2, b[2],
		b[0], a[1], b[2],
		a[0], a[1], b[2],
		a[0], b[1], b[2],
		b[0], b[1], b[2],
		callback, nativedata, nativedata2, &clipboxplane[5]
	);
}

