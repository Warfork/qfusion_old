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
#include "qcommon.h"

cvar_t	*net_compressPackets;

int LastCompMessageSize = 0;

typedef struct huffnode_s
{
	struct huffnode_s *zero;
	struct huffnode_s *one;
	unsigned char val;
	float freq;
} huffnode_t;

typedef struct
{
	unsigned int bits;
	int len;
} hufftab_t;

static huffnode_t *HuffTree = 0;
static hufftab_t HuffLookup[256];

static float HuffFreq[256] =
{
#include "huff_tab.h"
};

static unsigned char Masks[8] =
{
	0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80
};

void PutBit (unsigned char *buf,int pos,int bit)
{
	if (bit)
		buf[pos/8] |= Masks[pos%8];
	else
		buf[pos/8] &= ~Masks[pos%8];
}

void FindTab (huffnode_t *tmp, int len, unsigned int bits)
{
	if (!tmp)
		Com_Error (ERR_DROP, "No huff node");

	if (tmp->zero)
	{
		if (!tmp->one)
			Com_Error (ERR_DROP, "No one in node");

		if (len >= 32)
			Com_Error (ERR_DROP, "Compression screwed");

		FindTab (tmp->zero, len+1, bits<<1);
		FindTab (tmp->one, len+1, (bits<<1)|1);
		return;
	}

	HuffLookup[tmp->val].len = len;
	HuffLookup[tmp->val].bits = bits;
}

void BuildTree (float *freq)
{
	float min1, min2;
	int i, j, minat1, minat2;
	huffnode_t *work[256];	
	huffnode_t *tmp;

	for (i = 0; i < 256; i++)
	{
		work[i] = (huffnode_t *)Z_Malloc (sizeof(huffnode_t));
		work[i]->val = (unsigned char)i;
		work[i]->freq = freq[i];
		work[i]->zero = 0;
		work[i]->one = 0;
		HuffLookup[i].len = 0;
	}

	for (i = 0; i < 255; i++)
	{
		minat1 = -1;
		minat2 = -1;
		min1 = 1E30;
		min2 = 1E30;

		for (j = 0; j < 256; j++)
		{
			if (!work[j])
				continue;

			if (work[j]->freq < min1)
			{
				minat2 = minat1;
				min2 = min1;
				minat1 = j;
				min1 = work[j]->freq;
			}
			else if (work[j]->freq < min2)
			{
				minat2 = j;
				min2 = work[j]->freq;
			}
		}

		if (minat1 < 0)
			Sys_Error ("minatl: %f", minat1);
		if (minat2 < 0)
			Sys_Error ("minat2: %f", minat2);
		
		tmp = (huffnode_t *)Z_Malloc(sizeof(huffnode_t));
		tmp->zero = work[minat2];
		tmp->one = work[minat1];
		tmp->freq = work[minat2]->freq + work[minat1]->freq;
		tmp->val = 0xff;
		work[minat1] = tmp;
		work[minat2] = 0;
	}

	HuffTree = tmp;
	FindTab (HuffTree, 0, 0);

#if _DEBUG
	for (i = 0; i < 256; i++)
	{
		if (!HuffLookup[i].len && HuffLookup[i].len <= 32)
			Sys_Error("bad frequency table");
	}
#endif
}

void HuffDecode (unsigned char *in, unsigned char *out, int inlen, int *outlen)
{
	int bits, tbits;
	huffnode_t *tmp;	

	if ( !net_compressPackets->value ) {
		memcpy ( out, in, inlen );
		*outlen = inlen;
		return;
	}

	if (*in == 0xff)
	{
		memcpy (out, in+1, inlen-1);
		*outlen = inlen-1;
		return;
	}

	tbits = ((inlen-1) << 3) - *in;
	bits = 0;
	*outlen = 0;

	while (bits < tbits)
	{
		tmp = HuffTree;

		do
		{
			if ((in+1)[bits>>3] & Masks[bits%8])
				tmp = tmp->one;
			else
				tmp = tmp->zero;

			bits++;
		} while (tmp->zero);

		*out++ = tmp->val;
		(*outlen)++;
	}
}

void HuffEncode (unsigned char *in, unsigned char *out, int inlen, int *outlen)
{
	int i, j, bitat;
	unsigned int t;

	if ( !net_compressPackets->value ) {
		memcpy ( out, in, inlen );
		*outlen = inlen;
		return;
	}

	bitat = 0;
	for (i = 0; i < inlen; i++)
	{
		t = HuffLookup[in[i]].bits;
		bitat += HuffLookup[in[i]].len;

		for (j = 0; j < HuffLookup[in[i]].len; j++)
		{
			PutBit (out + 1, bitat - j - 1, t&1);
			t >>= 1;
		}
	}

	*outlen = 1 + ((bitat + 7) >> 3);
	*out = (((*outlen)-1)<<3) - bitat;

	if (*outlen >= inlen+1)
	{
		*out = 0xff;
		memcpy (out+1, in, inlen);
		*outlen = inlen+1;
	}

#if _DEBUG
	{
		unsigned char *buf;
		int tlen;

		buf = (unsigned char *)Q_malloc( inlen );

		HuffDecode ( out, buf, *outlen, &tlen );

		if (!tlen == inlen)
			Sys_Error ( "bogus compression" );

		for (i = 0; i < inlen; i++)
		{
			if (in[i] != buf[i])
				Sys_Error ( "bogus compression" );
		}

		Q_free(buf);
	}
#endif
}

void HuffInit (void)
{
	if ( Com_ServerState() ) {
		net_compressPackets = Cvar_Get ( "net_compresspackets", "1", CVAR_SERVERINFO );
	} else {
		net_compressPackets = Cvar_Get ( "net_compresspackets", "1", 0 );
	}

	BuildTree ( HuffFreq );
}