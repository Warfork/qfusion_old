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

/*

packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16	qport

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher than the last reliable sequence, but without the correct even/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted. It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

If the sequence number is -1, the packet should be handled without a netcon.

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection. This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.


The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs. The IP port should be updated
to the new value before sending out any replies.


If there is no information that needs to be transfered on a given frame,
such as during the connection stage while waiting for the client to load,
then a packet only needs to be delivered if there is something in the
unacknowledged reliable
*/

cvar_t		*showpackets;
cvar_t		*showdrop;
cvar_t		*qport;

netadr_t	net_from;

sizebuf_t	net_recieved;
qbyte		net_recieved_buffer[MAX_PACKETLEN];

sizebuf_t	net_message;
qbyte		net_message_buffer[MAX_MSGLEN];

/*
===============
Netchan_Init

===============
*/
void Netchan_Init (void)
{
	int		port;

	// pick a port value that should be nice and random
	port = Sys_Milliseconds() & 0xffff;

	showpackets = Cvar_Get ("showpackets", "0", 0);
	showdrop = Cvar_Get ("showdrop", "0", 0);
	qport = Cvar_Get ("qport", va("%i", port), CVAR_NOSET);
}

/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand (int net_socket, netadr_t adr, int length, qbyte *data)
{
	sizebuf_t	send;
	qbyte		send_buf[MAX_PACKETLEN];

// write the packet header
	SZ_Init (&send, send_buf, sizeof(send_buf));

	MSG_WriteLong (&send, -1);	// -1 sequence means out of band
	SZ_Write (&send, data, length);

// send the datagram
	NET_SendPacket (net_socket, send.cursize, send.data, adr);
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint (int net_socket, netadr_t adr, char *format, ...)
{
	va_list		argptr;
	static char		string[MAX_PACKETLEN - 4];

	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format, argptr);
	va_end (argptr);

	Netchan_OutOfBand (net_socket, adr, strlen(string), (qbyte *)string);
}


/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t adr, int qport)
{
	memset (chan, 0, sizeof(*chan));

	chan->sock = sock;
	chan->remote_address = adr;
	chan->qport = qport;
	chan->last_received = curtime;
	chan->incoming_sequence = 0;
	chan->outgoing_sequence = 1;

	SZ_Init (&chan->message, chan->message_buf, sizeof(chan->message_buf));
	chan->message.allowoverflow = qtrue;
}

/*
===============
Netchan_NeedReliable
================
*/
qboolean Netchan_NeedReliable (netchan_t *chan)
{
	qboolean	send_reliable;

// if the remote side dropped the last reliable message, resend it
	send_reliable = qfalse;

	if (chan->incoming_acknowledged > chan->last_reliable_sequence
	&& chan->incoming_reliable_acknowledged != chan->reliable_sequence)
		send_reliable = qtrue;

// if the reliable transmit buffer is empty, copy the current message out
	if (!chan->reliable_length && chan->message.cursize)
	{
		send_reliable = qtrue;
	}

	return send_reliable;
}

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
void Netchan_Transmit (netchan_t *chan, int length, qbyte *data)
{
	sizebuf_t	send;
	qbyte		send_buf[MAX_PACKETLEN], send_uncompressed;
	qbyte		bigbuf[MAX_MSGLEN], compressed[MAX_PACKETLEN-PACKET_HEADER];
	qboolean	send_reliable, send_unreliable;
	unsigned	w1, w2;
	unsigned	bigbuflen, len;

	// check for message overflow
	if (chan->message.overflowed)
	{
		chan->fatal_error = qtrue;
		Com_Printf ("%s:Outgoing message overflow\n"
			, NET_AdrToString (&chan->remote_address));
		return;
	}

	send_reliable = Netchan_NeedReliable (chan);

	if (!chan->reliable_length && chan->message.cursize)
	{
		memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
	}

	// initialize our packet buffer
	SZ_Init (&send, send_buf, sizeof(send_buf));

	// do not bother compressing anything less than MAX_PACKETLEN/3
	if ((send_reliable ? chan->reliable_length : 0) + length < MAX_PACKETLEN/3)
	{
		send_unreliable = qtrue;
		send_uncompressed = 255;
	}
	else
	{
		// zero-fill our buffer for huffman compression
		bigbuflen = 0;
		memset (bigbuf, 0, sizeof(bigbuf));

		send_uncompressed = 0;

		// copy the reliable message to the packet first
		if (send_reliable)
		{
			memcpy (bigbuf, chan->reliable_buf, chan->reliable_length);
			bigbuflen += chan->reliable_length;
		}

		// add the unreliable part if space is available
		if (sizeof(bigbuf) - bigbuflen >= length)
		{
			send_unreliable = qtrue;
			memcpy (bigbuf+bigbuflen, data, length);
			bigbuflen += length;
		}
		else
		{
			send_unreliable = qfalse;
			Com_Printf ("Netchan_Transmit: dumped unreliable\n");
		}

		// compress leaving space for header
		len = Huff_EncodeStatic (bigbuf, bigbuflen, compressed, sizeof(compressed));
		if (len > sizeof(compressed))
		{	// try sending reliable data only
			if (send_unreliable)
			{
				send_unreliable = qfalse;
				Com_Printf ("Netchan_Transmit: dumped unreliable\n");

				len = Huff_EncodeStatic (bigbuf, bigbuflen -= length, compressed, sizeof(compressed));
				if (len > sizeof(compressed))
					Com_Error (ERR_FATAL, "Netchan_Transmit: buffer overflow");
			}
			else
			{
				Com_Error (ERR_FATAL, "Netchan_Transmit: buffer overflow");
			}
		}
	}

	if (send_reliable)
		chan->last_reliable_sequence = chan->outgoing_sequence;

	// write the packet header
	w1 = ( chan->outgoing_sequence & ~(1<<31) ) | (send_reliable<<31);
	w2 = ( chan->incoming_sequence & ~(1<<31) ) | (chan->incoming_reliable_sequence<<31);

	chan->outgoing_sequence++;
	chan->last_sent = curtime;

	MSG_WriteLong (&send, w1);
	MSG_WriteLong (&send, w2);

	// send the qport if we are a client
	if (chan->sock == NS_CLIENT)
		MSG_WriteShort (&send, qport->integer);

	if (send_uncompressed)
	{
		SZ_Write (&send, &send_uncompressed, 1);
		if (send_reliable)
			SZ_Write (&send, chan->reliable_buf, chan->reliable_length);
		if (send_unreliable)
			SZ_Write (&send, data, length);
	}
	else
	{
		SZ_Write (&send, compressed, len);
	}

	// send the datagram
	NET_SendPacket (chan->sock, send.cursize, send.data, chan->remote_address);

	if (showpackets->integer)
	{
		if (send_reliable)
			Com_Printf ("send %4i : s=%i reliable=%i ack=%i rack=%i\n"
				, send.cursize
				, chan->outgoing_sequence - 1
				, chan->reliable_sequence
				, chan->incoming_sequence
				, chan->incoming_reliable_sequence);
		else
			Com_Printf ("send %4i : s=%i ack=%i rack=%i\n"
				, send.cursize
				, chan->outgoing_sequence - 1
				, chan->incoming_sequence
				, chan->incoming_reliable_sequence);
	}
}

/*
=================
Netchan_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
qboolean Netchan_Process (netchan_t *chan, sizebuf_t *recieved, sizebuf_t *msg)
{
	unsigned	sequence, sequence_ack;
	unsigned	reliable_ack, reliable_message;
	int			qport, length, headerlength;
	qbyte		bigbuf[MAX_MSGLEN];

	// get sequence numbers		
	MSG_BeginReading (recieved);
	sequence = MSG_ReadLong (recieved);
	sequence_ack = MSG_ReadLong (recieved);

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1<<31);
	sequence_ack &= ~(1<<31);	

	// read the qport if we are a server
	if (chan->sock == NS_SERVER)
		qport = MSG_ReadShort (recieved);

	headerlength = recieved->readcount;
	SZ_Init (msg, msg->data, msg->maxsize);
	SZ_Write (msg, recieved->data, headerlength);

	length = Huff_DecodeStatic (recieved->data + headerlength, recieved->cursize - headerlength, bigbuf, sizeof(bigbuf));
	if (length > MAX_MSGLEN)
		Com_Error (ERR_FATAL, "Netchan_Process: buffer overflow");
	SZ_Write (msg, bigbuf, length);

	// skip header for other functions
	msg->readcount = headerlength;

	if (showpackets->integer)
	{
		if (reliable_message)
			Com_Printf ("recv %4i : s=%i reliable=%i ack=%i rack=%i\n"
				, msg->cursize
				, sequence
				, chan->incoming_reliable_sequence ^ 1
				, sequence_ack
				, reliable_ack);
		else
			Com_Printf ("recv %4i : s=%i ack=%i rack=%i\n"
				, msg->cursize
				, sequence
				, sequence_ack
				, reliable_ack);
	}

//
// discard stale or duplicated packets
//
	if (sequence <= chan->incoming_sequence)
	{
		if (showdrop->integer)
			Com_Printf ("%s:Out of order packet %i at %i\n"
				, NET_AdrToString (&chan->remote_address)
				,  sequence
				, chan->incoming_sequence);
		return qfalse;
	}

//
// dropped packets don't keep the message from being used
//
	chan->dropped = sequence - (chan->incoming_sequence+1);
	if (chan->dropped > 0)
	{
		if (showdrop->integer)
			Com_Printf ("%s:Dropped %i packets at %i\n"
			, NET_AdrToString (&chan->remote_address)
			, chan->dropped
			, sequence);
	}

//
// if the current outgoing reliable message has been acknowledged
// clear the buffer to make way for the next
//
	if (reliable_ack == chan->reliable_sequence)
		chan->reliable_length = 0;	// it has been received
	
//
// if this message contains a reliable message, bump incoming_reliable_sequence 
//
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message)
	{
		chan->incoming_reliable_sequence ^= 1;
	}

//
// the message can now be read from the current message pointer
//
	chan->last_received = curtime;

	return qtrue;
}
