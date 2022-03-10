/*
Copyright (C) 2002-2003 Victor Luchits

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
// huff.c
#include "qcommon.h"

//#define HUFF_DEBUG

typedef struct huffNode_s
{
	unsigned pos, count;
	struct huffNode_s *children[2];
} huffNode_t;

typedef struct huffTab_s
{
	unsigned len;
	unsigned bits;
} huffTab_t;

typedef struct huffTree_s
{
	huffNode_t *headNode;
	huffTab_t  lookupTable[256];
} huffTree_t;

static huffTree_t huffTree;
static unsigned huffCounts[256] =
{
	 66067,11275, 7275, 2794,13169, 2863, 2503, 1476
	,  985, 1473, 1003, 3366, 3305, 3119, 1496,  529
	, 6183, 1280, 1591, 1226, 1318, 1194, 1200, 1147
	, 1184, 1228, 1160, 1180, 1160,  450, 1363,  687
	, 1756,  292, 1142,  400,  253,  276,  656,  247
	,  546,  376,  372,  540,  314,  529,  616,  879
	,  531,  650,  614,  611,  440,  483,  381,  751
	,  457,  372,  273,  251,  362,  224,  357,  367
	, 5078,  384,  473,  327,  577,  251,  256,  261
	,  294,  237,  241,  230,  275,  268,  211,  271
	,  345,  350,  326,  230,  247,  207,  311,  261
	,  262,  232,  262,  192,  300,  234,  394,  386
	,  330,  854,  372,  946,  702,  790,  343,  492
	,  732,  627,  233,  266,  539,  581, 2371, 2634
	,  554,  235,  723, 1229, 2289,  468,  409,  536
	,  251,  268,  183,  204,  222,  204,  200,  194
	, 2992,  287, 1072,  208,  702,  311, 1312,  424
	,  382,  228,  220,  231,  338,  190,  255,  544
	,  333,  188,  234,  408,  221,  256,  211,  541
	,  310,  383,  277,  602,  636,  242,  249,  781
	,  496,  328,  310,  216,  272,  180,  202,  690
	,  275,  206,  348,  235,  238,  208,  280,  311
	,  231,  270,  427,  275,  214,  190,  202,  298
	,  366,  244,  274,  444,  262,  269,  344,  452
	, 1210,  701,  306,  182, 1139,  205,  357,  300
	,  353,  188,  347,  225,  307,  239,  509,  365
	,  569,  241,  216,  250,  288,  245,  191,  186
	,  809,  178,  227,  244,  540,  598,  396,  328
	,  802,  850,  498,  467,  405,  373,  363,  404
	,  431,  368,  392,  392,  551,  345,  310,  374
	,  486,  455,  399,  490,  594,  673,  683,  611
	,  909,  804,  893, 1246, 3032, 3928, 5402,32286
};

#ifdef HUFF_DEBUG
unsigned debugHuffCounts[256];

/*
=============
Huff_ResetCounts
=============
*/
static void Huff_ResetCounts( void ) {
	memset ( debugHuffCounts, 0, sizeof(debugHuffCounts) );
}

/*
=============
Huff_AddCounts
=============
*/
void Huff_AddCounts( unsigned char *data, unsigned len )
{
	int i;

	for( i = 0; i < len; i++ )
		debugHuffCounts[data[i]]++;
}

/*
=============
Huff_DumpCounts_f
=============
*/
void Huff_DumpCounts_f( void )
{
	int i, j;

	Com_Printf( " %5i", debugHuffCounts[0] );
	for( j = 1; j < 8; j++ )
		Com_Printf ( ",%5i", debugHuffCounts[j] );
	Com_Printf ( "\n" );

	for( i = 8; i < 256; i += 8 ) {
		for( j = 0; j < 8; j++ )
			Com_Printf ( ",%5i", debugHuffCounts[i+j] );
		Com_Printf ( "\n" );
	}

	Huff_ResetCounts ();
}
#endif

/*
=============
Huff_BuildTable
=============
*/
static void Huff_BuildTable( huffNode_t *node, huffTab_t *table, unsigned len, unsigned bits )
{
	if( !node )
		Com_Error ( ERR_FATAL, "Huff_BuildTable: no node" );

	if( node->children[0] ) {
		if( !node->children[1] )
			Com_Error( ERR_FATAL, "Huff_BuildTable: bad node" );
		if( len >= 32 )
			Com_Error( ERR_FATAL, "Huff_BuildTable: bad len" );

		bits <<= 1;
		Huff_BuildTable( node->children[0], table, len + 1, bits );
		Huff_BuildTable( node->children[1], table, len + 1, bits | 1 );
		return;
	}

	table[node->pos].len = len;
	table[node->pos].bits = bits;
}

/*
=============
Huff_BuildTree
=============
*/
static void Huff_BuildTree( huffTree_t *tree, unsigned *counts )
{
	int i, j;
	unsigned best[2], bestval[2];
	huffNode_t *nodes[256], *headNode;

	headNode = ( huffNode_t * )Mem_ZoneMalloc( sizeof( huffNode_t ) * (256 + 255) );

	for( i = 0; i < 256; i++ ) {
		nodes[i] = headNode++;
		nodes[i]->count = counts[i];
		nodes[i]->pos = ( unsigned )i;
	}

	for( i = 0; i < 255; i++ ) {
		best[0] = best[1] = 0;
		bestval[0] = bestval[1] = 99999999;

		for( j = 0; j < 256; j++ ) {
			if( !nodes[j] )
				continue;

			if( nodes[j]->count < bestval[0] ) {
				best[1] = best[0];
				bestval[1] = bestval[0];
				best[0] = j + 1;
				bestval[0] = nodes[j]->count;
			} else if ( nodes[j]->count < bestval[1] ) {
				best[1] = j + 1;
				bestval[1] = nodes[j]->count;
			}
		}

		if( !best[0] )
			Com_Error( ERR_FATAL, "Huff_BuildTree: no node 0: %i", bestval[0] );
		if( !best[1] )
			Com_Error( ERR_FATAL, "Huff_BuildTree: no node 1: %i", bestval[1] );

		headNode->children[0] = nodes[best[1]-1];
		headNode->children[1] = nodes[best[0]-1];
		headNode->count = headNode->children[0]->count + headNode->children[1]->count;
		headNode->pos = 0xff;

		nodes[best[0]-1] = headNode++;
		nodes[best[1]-1] = NULL;
	}

	tree->headNode = --headNode;
	memset( tree->lookupTable, 0, sizeof( tree->lookupTable ) );

	Huff_BuildTable( tree->headNode, tree->lookupTable, 0, 0 );
}

/*
=============
Huff_Decode
=============
*/
static unsigned Huff_Decode( huffTree_t *tree, unsigned char *in, unsigned inSize, unsigned char *out, unsigned maxOutSize )
{
	huffNode_t *node;
	unsigned bits, tbits;
	unsigned outSize;

	if( !inSize )
		return 0;

	if( *in == 0xff ) {
		memcpy( out, in + 1, inSize - 1 );
		return inSize - 1;
	}

	outSize = 0;
	tbits = ((inSize - 1) << 3) - *in++;
	for( bits = 0; bits < tbits; ) {
		node = tree->headNode;

		do {
			if( in[bits >> 3] & (1 << (bits & 7)) )
				node = node->children[1];
			else
				node = node->children[0];
			bits++;
		} while( node->children[0] );

		if( outSize == maxOutSize )
			Com_Error ( ERR_DROP, "Huff_Decode: outlen == maxOutSize" );

		*out++ = node->pos;
		outSize++;
	}

	return outSize;
}

/*
=============
Huff_Encode
=============
*/
static unsigned Huff_Encode( huffTree_t *tree, unsigned char *in, unsigned inSize, unsigned char *out, unsigned maxOutSize )
{
	int i, j;
	unsigned bits, pos, len, bitsPos;
	unsigned outSize;

	if( !inSize )
		return 0;

	bitsPos = 0;
	for( i = 0; i < inSize; i++, bitsPos += len ) {
		bits = tree->lookupTable[in[i]].bits;
		len = tree->lookupTable[in[i]].len;
		pos = bitsPos + len - 1;
		if( (pos>>3) >= maxOutSize )
			return maxOutSize + 1;

		for( j = 0; j < len; j++, pos--, bits >>= 1 ) {
			if( bits & 1 )
				out[(pos>>3)+1] |= (1 << (pos & 7));
			else
				out[(pos>>3)+1] &= ~(1 << (pos & 7));
		}
	}

	outSize = (bitsPos + 7) / 8 + 1;
	if( outSize >= inSize + 1 ) {
		if( inSize + 1 > maxOutSize )
			return inSize + 1;
		*out = 0xff;
		memcpy( out + 1, in, inSize );
		return inSize + 1;
	} else if( outSize > maxOutSize ) {
		return outSize;
	} else {
		*out = ((outSize - 1) << 3) - bitsPos;
	}

	return outSize;
}

/*
=============
Huff_DecodeStatic
=============
*/
unsigned Huff_DecodeStatic( unsigned char *in, unsigned inSize, unsigned char *out, unsigned maxOutSize ) {
	return Huff_Decode( &huffTree, in, inSize, out, maxOutSize );
}

/*
=============
Huff_EncodeStatic
=============
*/
unsigned Huff_EncodeStatic( unsigned char *in, unsigned inSize, unsigned char *out, unsigned maxOutSize ) {
	return Huff_Encode( &huffTree, in, inSize, out, maxOutSize );
}

/*
=============
Huff_Init
=============
*/
void Huff_Init( void )
{
#ifdef HUFF_DEBUG
	Cmd_AddCommand( "dumpcounts", Huff_DumpCounts_f );
#endif

	Huff_BuildTree( &huffTree, huffCounts );

#ifdef HUFF_DEBUG
	Huff_ResetCounts ();
#endif
}
