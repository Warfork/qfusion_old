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
// net_wins.h

#ifndef __NET_WINS_H__
# define __NET_WINS_H__

// the code below was initially taken from the Wine project
#include "winsock.h"

#ifdef HAS_WSIPX
# include "wsipx.h"
#else
# if __GNUC__ >=3
#  pragma GCC system_header
# endif

# ifdef __cplusplus
extern "C" {
# endif

# define NSPROTO_IPX	1000
# define NSPROTO_SPX	1256
# define NSPROTO_SPXII	1257

typedef struct sockaddr_ipx {
	short sa_family;
	char sa_netnum[4];
	char sa_nodenum[6];
	unsigned short sa_socket;
} SOCKADDR_IPX, *PSOCKADDR_IPX, *LPSOCKADDR_IPX;

# ifdef __cplusplus
}
# endif

#endif

#endif //__NET_WINS_H__