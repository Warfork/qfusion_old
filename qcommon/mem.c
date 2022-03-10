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
// Z_zone.c

#include "qcommon.h"

cvar_t *developerMemory;

mempool_t *poolChain = NULL;

// used for temporary memory allocations around the engine, not for longterm
// storage, if anything in this pool stays allocated during gameplay, it is
// considered a leak
mempool_t *tempMemPool;

// only for zone
mempool_t *zoneMemPool;

void *_Mem_Alloc ( mempool_t *pool, int size, int musthave, int canthave, const char *filename, int fileline )
{
#if MEMCLUMPING
	int i, j, k, needed, endbit, largest;
	memclump_t *clump, **clumpChainPointer;
#endif
	memheader_t *mem;

	if ( size <= 0 ) {
		return NULL;
	}
	if ( pool == NULL ) {
		Sys_Error ( "Mem_Alloc: pool == NULL (alloc at %s:%i)", filename, fileline );
	}
	if ( musthave && ((pool->flags & musthave) != musthave) ) {
		Sys_Error ( "Mem_Alloc: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	}
	if ( canthave && (pool->flags & canthave) ) {
		Sys_Error ( "Mem_Alloc: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	}

	if ( developerMemory && developerMemory->value ) {
		Com_DPrintf ( "Mem_Alloc: pool %s, file %s:%i, size %i bytes\n", pool->name, filename, fileline, size );
	}

	pool->totalsize += size;

#if MEMCLUMPING
	if ( size < 4096 ) {
		// clumping
		needed = (sizeof(memheader_t) + size + sizeof(int) + (MEMUNIT - 1)) / MEMUNIT;
		endbit = MEMBITS - needed;

		for ( clumpChainPointer = &pool->clumpchain; *clumpChainPointer; clumpChainPointer = &(*clumpChainPointer)->chain ) {
			clump = *clumpChainPointer;

			if ( clump->sentinel1 != MEMCLUMP_SENTINEL ) {
				Sys_Error("Mem_Alloc: trashed clump sentinel 1 (alloc at %s:%d)", filename, fileline);
			}
			if ( clump->sentinel2 != MEMCLUMP_SENTINEL ) {
				Sys_Error("Mem_Alloc: trashed clump sentinel 2 (alloc at %s:%d)", filename, fileline);
			}

			if ( clump->largestavailable >= needed ) {
				largest = 0;

				for ( i = 0; i < endbit; i++ ) {
					if ( clump->bits[i >> 5] & (1 << (i & 31)) ) {
						continue;
					}

					k = i + needed;
					for ( j = i; i < k; i++ ) {
						if ( clump->bits[i >> 5] & (1 << (i & 31)) ) {
							goto loopcontinue;
						}
					}

					goto choseclump;

loopcontinue:;
					if ( largest < j - i ) {
						largest = j - i;
					}
				}

				// since clump falsely advertised enough space (nothing wrong
				// with that), update largest count to avoid wasting time in
				// later allocations
				clump->largestavailable = largest;
			}
		}

		pool->realsize += sizeof(memclump_t);

		clump = malloc(sizeof(memclump_t));
		if ( clump == NULL ) {
			Sys_Error ( "Mem_Alloc: out of memory (alloc at %s:%i)", filename, fileline );
		}

		memset ( clump, 0, sizeof(memclump_t) );
		clump->sentinel1 = MEMCLUMP_SENTINEL;
		clump->sentinel2 = MEMCLUMP_SENTINEL;
		clump->chain = NULL;
		clump->blocksinuse = 0;
		clump->largestavailable = MEMBITS - needed;
		*clumpChainPointer = clump;

		j = 0;

choseclump:
		mem = (memheader_t *)((qbyte *) clump->block + j * MEMUNIT);
		mem->clump = clump;
		clump->blocksinuse += needed;

		for ( i = j + needed; j < i; j++ ) {
			clump->bits[j >> 5] |= (1 << (j & 31));
		}
	} else {
		// big allocations are not clumped
#endif
		pool->realsize += sizeof(memheader_t) + size + sizeof(int);

		mem = malloc(sizeof(memheader_t) + size + sizeof(int));
		if ( mem == NULL ) {
			Sys_Error("Mem_Alloc: out of memory (alloc at %s:%i)", filename, fileline);
		}

#if MEMCLUMPING
		mem->clump = NULL;
	}
#endif

	mem->filename = filename;
	mem->fileline = fileline;
	mem->size = size;
	mem->pool = pool;
	mem->sentinel1 = MEMHEADER_SENTINEL1;

	// we have to use only a single byte for this sentinel, because it may not be aligned, and some platforms can't use unaligned accesses
	*((qbyte *) mem + sizeof(memheader_t) + mem->size) = MEMHEADER_SENTINEL2;

	// append to head of list
	mem->next = pool->chain;
	mem->prev = NULL;
	pool->chain = mem;
	if ( mem->next ) {
		mem->next->prev = mem;
	}

	memset((void *)((qbyte *) mem + sizeof(memheader_t)), 0, mem->size);

	return (void *)((qbyte *) mem + sizeof(memheader_t));
}

// FIXME: rewrite this?
void *_Mem_Realloc ( void *data, int size, const char *filename, int fileline )
{
	void *newdata;
	memheader_t *mem;

	if ( data == NULL ) {
		Sys_Error ( "Mem_Realloc: data == NULL (called at %s:%i)", filename, fileline );
	}
	if ( size <= 0 ) {
		Mem_Free ( data );
		return NULL;
	}

	mem = (memheader_t *)((qbyte *) data - sizeof(memheader_t));
	if ( size <= mem->size ) {
		return data;
	}

	newdata = Mem_Alloc ( mem->pool, size );
	memcpy ( newdata, data, mem->size );
	Mem_Free ( data );

	return newdata;
}

void _Mem_Free ( void *data, int musthave, int canthave, const char *filename, int fileline ) 
{
#if MEMCLUMPING
	int i, firstblock, endblock;
	memclump_t *clump, **clumpChainPointer;
#endif
	memheader_t *mem;
	mempool_t *pool;

	if ( data == NULL ) {
		Sys_Error ( "Mem_Free: data == NULL (called at %s:%i)", filename, fileline );
	}

	mem = (memheader_t *)((qbyte *) data - sizeof(memheader_t));
	if ( mem->sentinel1 != MEMHEADER_SENTINEL1 ) {
		Sys_Error ( "Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline );
	}

	if ( *((qbyte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2 ) {
		Sys_Error ( "Mem_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline );
	}

	pool = mem->pool;
	if ( musthave && ((pool->flags & musthave) != musthave) ) {
		Sys_Error ( "Mem_Free: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	}
	if ( canthave && (pool->flags & canthave) ) {
		Sys_Error ( "Mem_Free: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	}

	if ( developerMemory && developerMemory->value ) {
		Com_DPrintf ( "Mem_Free: pool %s, alloc %s:%i, free %s:%i, size %i bytes\n", pool->name, mem->filename, mem->fileline, filename, fileline, mem->size );
	}

	// unlink memheader from doubly linked list
	if ( (mem->prev ? mem->prev->next != mem : pool->chain != mem) || (mem->next && mem->next->prev != mem) ) {
		Sys_Error("Mem_Free: not allocated or double freed (free at %s:%i)", filename, fileline);
	}

	if (mem->prev)
		mem->prev->next = mem->next;
	else
		pool->chain = mem->next;

	if (mem->next)
		mem->next->prev = mem->prev;

	// memheader has been unlinked, do the actual free now
	pool->totalsize -= mem->size;

#if MEMCLUMPING
	if ( (clump = mem->clump) ) {
		if ( clump->sentinel1 != MEMCLUMP_SENTINEL ) {
			Sys_Error ( "Mem_Free: trashed clump sentinel 1 (free at %s:%i)", filename, fileline );
		}
		if ( clump->sentinel2 != MEMCLUMP_SENTINEL ) {
			Sys_Error ( "Mem_Free: trashed clump sentinel 2 (free at %s:%i)", filename, fileline );
		}

		firstblock = ((qbyte *) mem - (qbyte *) clump->block);
		if ( firstblock & (MEMUNIT - 1) ) {
			Sys_Error("Mem_Free: address not valid in clump (free at %s:%i)", filename, fileline);
		}

		firstblock /= MEMUNIT;
		endblock = firstblock + ((sizeof(memheader_t) + mem->size + sizeof(int) + (MEMUNIT - 1)) / MEMUNIT);
		clump->blocksinuse -= endblock - firstblock;

		// could use &, but we know the bit is set
		for ( i = firstblock;i < endblock;i++ ) {
			clump->bits[i >> 5] -= (1 << (i & 31));
		}

		if ( clump->blocksinuse <= 0 ) {		// unlink from chain
			for ( clumpChainPointer = &pool->clumpchain; *clumpChainPointer; clumpChainPointer = &(*clumpChainPointer)->chain ) {
				if ( *clumpChainPointer == clump ) {
					*clumpChainPointer = clump->chain;
					break;
				}
			}

			pool->realsize -= sizeof(memclump_t);
			memset ( clump, 0xBF, sizeof(memclump_t) );
			free ( clump );
		} else {								// clump still has some allocations
			// force re-check of largest available space on next alloc
			clump->largestavailable = MEMBITS - clump->blocksinuse;
		}
	} else {
#endif
		pool->realsize -= sizeof(memheader_t) + mem->size + sizeof(int);
		memset ( mem, 0xBF, sizeof(memheader_t) + mem->size + sizeof(int) );
		free ( mem );
#if MEMCLUMPING
	}
#endif
}

mempool_t *_Mem_AllocPool ( mempool_t *parent, const char *name, int flags, const char *filename, int fileline )
{
	mempool_t *pool;

	if ( parent && (parent->flags & MEMPOOL_TEMPORARY) ) {
		Sys_Error ( "Mem_AllocPool: nested temporary pools are not allowed (allocpool at %s:%i)", filename, fileline );
	}
	if ( flags & MEMPOOL_TEMPORARY ) {
		Sys_Error ( "Mem_AllocPool: tried to allocate temporary pool, use Mem_AllocTempPool instead (allocpool at %s:%i)", filename, fileline );
	}

	pool = malloc ( sizeof(mempool_t) );
	if ( pool == NULL ) {
		Sys_Error ( "Mem_AllocPool: out of memory (allocpool at %s:%i)", filename, fileline );
	}

	memset ( pool, 0, sizeof(mempool_t) );
	pool->sentinel1 = MEMHEADER_SENTINEL1;
	pool->sentinel2 = MEMHEADER_SENTINEL1;
	pool->filename = filename;
	pool->fileline = fileline;
	pool->flags = flags;
	pool->chain = NULL;
	pool->parent = parent;
	pool->child = NULL;
	pool->totalsize = 0;
	pool->realsize = sizeof(mempool_t);
	strncpy ( pool->name, name, sizeof(pool->name)-1 );

	if ( parent ) {
		pool->next = parent->child;
		parent->child = pool;
	} else {
		pool->next = poolChain;
		poolChain = pool;
	}

	return pool;
}

mempool_t *_Mem_AllocTempPool ( const char *name, const char *filename, int fileline )
{
	mempool_t *pool;

	pool = _Mem_AllocPool ( NULL, name, 0, filename, fileline );
	pool->flags = MEMPOOL_TEMPORARY;

	return pool;
}

void _Mem_FreePool ( mempool_t **pool, int musthave, int canthave, const char *filename, int fileline )
{
	mempool_t **chainAddress, **next;

	if ( !(*pool) ) {
		return;
	}
	if ( musthave && (((*pool)->flags & musthave) != musthave) ) {
		Sys_Error ( "Mem_FreePool: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	}
	if ( canthave && ((*pool)->flags & canthave) ) {
		Sys_Error ( "Mem_FreePool: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	}

	// recurse into children
	// note that children will be freed no matter if their flags 
	// do not match musthave\canthave pair
	if ( (*pool)->child ) {
		for ( chainAddress = &(*pool)->child; *chainAddress; chainAddress = next ) {
			next = &((*chainAddress)->next);
			_Mem_FreePool ( chainAddress, 0, 0, filename, fileline );
		}
	}
	
	if ( (*pool)->sentinel1 != MEMHEADER_SENTINEL1 ) {
		Sys_Error ( "Mem_FreePool: trashed pool sentinel 1 (allocpool at %s:%i, freepool at %s:%i)", (*pool)->filename, (*pool)->fileline, filename, fileline );
	}
	if ( (*pool)->sentinel2 != MEMHEADER_SENTINEL1 ) {
		Sys_Error ( "Mem_FreePool: trashed pool sentinel 2 (allocpool at %s:%i, freepool at %s:%i)", (*pool)->filename, (*pool)->fileline, filename, fileline );
	}

	// unlink pool from chain
	if ( (*pool)->parent ) {
		for ( chainAddress = &(*pool)->parent->child; *chainAddress && *chainAddress != *pool; chainAddress = &((*chainAddress)->next) ) { }
	} else {
		for ( chainAddress = &poolChain; *chainAddress && *chainAddress != *pool; chainAddress = &((*chainAddress)->next) ) { }
	}

	if ( *chainAddress != *pool ) {
		Sys_Error ( "Mem_FreePool: pool already free (freepool at %s:%i)", filename, fileline );
	}

	*chainAddress = (*pool)->next;

	while ( (*pool)->chain ) {		// free memory owned by the pool
		Mem_Free ( (void *)((qbyte *)(*pool)->chain + sizeof(memheader_t)) );
	}

	// free the pool itself
	memset ( *pool, 0xBF, sizeof(mempool_t) );
	free ( *pool );
	*pool = NULL;
}

void _Mem_EmptyPool ( mempool_t *pool, int musthave, int canthave, const char *filename, int fileline )
{
	mempool_t *child, *next;

	if ( pool == NULL ) {
		Sys_Error ( "Mem_EmptyPool: pool == NULL (emptypool at %s:%i)", filename, fileline );
	}
	if ( musthave && ((pool->flags & musthave) != musthave) ) {
		Sys_Error ( "Mem_EmptyPool: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	}
	if ( canthave && (pool->flags & canthave) ) {
		Sys_Error ( "Mem_EmptyPool: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	}

	// recurse into children
	if ( pool->child ) {
		for ( child = pool->child; child; child = next ) {
			next = child->next;
			_Mem_EmptyPool ( child, 0, 0, filename, fileline );
		}
	}

	if ( pool->sentinel1 != MEMHEADER_SENTINEL1 ) {
		Sys_Error ( "Mem_EmptyPool: trashed pool sentinel 1 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline );
	}
	if ( pool->sentinel2 != MEMHEADER_SENTINEL1 ) {
		Sys_Error ( "Mem_EmptyPool: trashed pool sentinel 2 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline );
	}

	while ( pool->chain ) {			// free memory owned by the pool
		Mem_Free((void *)((qbyte *) pool->chain + sizeof(memheader_t)));
	}
}

void _Mem_CheckSentinels ( void *data, const char *filename, int fileline )
{
	memheader_t *mem;

	if ( data == NULL ) {
		Sys_Error ( "Mem_CheckSentinels: data == NULL (sentinel check at %s:%i)", filename, fileline );
	}

	mem = (memheader_t *)((qbyte *) data - sizeof(memheader_t));
	if ( mem->sentinel1 != MEMHEADER_SENTINEL1 ) {
		Sys_Error ( "Mem_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline );
	}
	if ( *((qbyte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2 ) {
		Sys_Error ( "Mem_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline );
	}
}

#if MEMCLUMPING
static void _Mem_CheckClumpSentinels ( memclump_t *clump, const char *filename, int fileline )
{
	// this isn't really very useful
	if ( clump->sentinel1 != MEMCLUMP_SENTINEL ) {
		Sys_Error ( "Mem_CheckClumpSentinels: trashed sentinel 1 (sentinel check at %s:%i)", filename, fileline );
	}
	if ( clump->sentinel2 != MEMCLUMP_SENTINEL ) {
		Sys_Error ( "Mem_CheckClumpSentinels: trashed sentinel 2 (sentinel check at %s:%i)", filename, fileline );
	}
}
#endif

void _Mem_CheckSentinelsPool ( mempool_t *pool, const char *filename, int fileline )
{
	memheader_t *mem;
#if MEMCLUMPING
	memclump_t *clump;
#endif
	mempool_t *child;

	// recurse into children
	if ( pool->child ) {
		for ( child = pool->child; child; child = child->next ) {
			_Mem_CheckSentinelsPool ( child, filename, fileline );
		}
	}

	if ( pool->sentinel1 != MEMHEADER_SENTINEL1 ) {
		Sys_Error ( "_Mem_CheckSentinelsPool: trashed pool sentinel 1 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline );
	}
	if ( pool->sentinel2 != MEMHEADER_SENTINEL1 ) {
		Sys_Error ( "_Mem_CheckSentinelsPool: trashed pool sentinel 2 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline );
	}

	for ( mem = pool->chain; mem; mem = mem->next) {
		_Mem_CheckSentinels((void *)((qbyte *) mem + sizeof(memheader_t)), filename, fileline);
	}

#if MEMCLUMPING
	for ( clump = pool->clumpchain; clump; clump = clump->chain ) {		
		_Mem_CheckClumpSentinels ( clump, filename, fileline );
	}
#endif
}

void _Mem_CheckSentinelsGlobal ( const char *filename, int fileline )
{
	mempool_t *pool;

	for ( pool = poolChain; pool; pool = pool->next ) {
		_Mem_CheckSentinelsPool ( pool, filename, fileline );
	}
}

void Mem_CountPoolStats ( mempool_t *pool, int *count, int *size, int *realsize )
{
	mempool_t *child;

	// recurse into children
	if ( pool->child ) {
		for ( child = pool->child; child; child = child->next ) {
			Mem_CountPoolStats ( child, count, size, realsize );
		}
	}

	if ( count ) {
		(*count)++;
	}
	if ( size ) {
		(*size) += pool->totalsize;
	}
	if ( realsize ) {
		(*realsize) += pool->realsize;
	}
}

void Mem_PrintStats (void)
{
	int count, size;
	int total, totalsize;
	mempool_t *pool;
	memheader_t *mem;

	Mem_CheckSentinelsGlobal ();

	for ( total = 0, totalsize = 0, pool = poolChain; pool; pool = pool->next ) {
		count = 0; size = 0;
		Mem_CountPoolStats ( pool, &count, &size, NULL );
		total += count; totalsize += size;
	}

	Com_Printf ( "%i memory pools, totalling %i bytes (%.3fMB)\n", total, totalsize, totalsize / 1048576.0 );

	// temporary pools are not nested
	for ( pool = poolChain; pool; pool = pool->next ) {
		if ( (pool->flags & MEMPOOL_TEMPORARY) && pool->chain ) {
			Com_Printf ( "%i bytes (%.3fMB) of temporary memory still allocated (Leak!)\n", pool->totalsize, pool->totalsize / 1048576.0 );
			Com_Printf ( "listing temporary memory allocations for %s:\n", pool->name );

			for ( mem = tempMemPool->chain; mem; mem = mem->next) {
				Com_Printf ( "%10i bytes allocated at %s:%i\n", mem->size, mem->filename, mem->fileline );
			}
		}
	}
}

void Mem_PrintPoolStats ( mempool_t *pool, int listchildren, int listallocations )
{
	mempool_t *child;
	memheader_t *mem;
	int totalsize = 0, realsize = 0;

	Mem_CountPoolStats ( pool, NULL, &totalsize, &realsize );

	if ( pool->parent ) {
		if ( pool->lastchecksize != 0 && totalsize != pool->lastchecksize ) {
			Com_Printf ( "%6ik (%6ik actual) %s:%s (%i byte change)\n", (totalsize + 1023) / 1024, (realsize + 1023) / 1024, pool->parent->name, pool->name, totalsize - pool->lastchecksize );
		} else {
			Com_Printf ( "%6ik (%6ik actual) %s:%s\n", (totalsize + 1023) / 1024, (realsize + 1023) / 1024, pool->parent->name, pool->name );
		}
	} else {
		if ( pool->lastchecksize != 0 && totalsize != pool->lastchecksize ) {
			Com_Printf ( "%6ik (%6ik actual) %s (%i byte change)\n", (totalsize + 1023) / 1024, (realsize + 1023) / 1024, pool->name, totalsize - pool->lastchecksize );
		} else {
			Com_Printf ( "%6ik (%6ik actual) %s\n", (totalsize + 1023) / 1024, (realsize + 1023) / 1024, pool->name );
		}
	}

	pool->lastchecksize = totalsize;

	if ( listallocations ) {
		for ( mem = pool->chain; mem; mem = mem->next ) {
			Com_Printf("%10i bytes allocated at %s:%i\n", mem->size, mem->filename, mem->fileline);
		}
	}

	if ( listchildren ) {
		if ( pool->child ) {
			for ( child = pool->child; child; child = child->next ) {
				Mem_PrintPoolStats ( child, listchildren, listallocations );
			}
		}
	}
}

void Mem_PrintList ( int listchildren, int listallocations )
{
	mempool_t *pool;

	Mem_CheckSentinelsGlobal ();

	Com_Printf ( "memory pool list:\n" "size    name\n" );

	for ( pool = poolChain; pool; pool = pool->next ) {
		Mem_PrintPoolStats ( pool, listchildren, listallocations );
	}
}

void MemList_f (void)
{
	switch ( Cmd_Argc () ) {
		case 1:
			Mem_PrintList ( qtrue, qfalse );
			Mem_PrintStats ();
			break;

		case 2:
			if ( !Q_stricmp (Cmd_Argv(1), "all") ) {
				Mem_PrintList ( qtrue, qtrue );
				Mem_PrintStats ();
				break;
			}

			// drop through
		default:
			Com_Printf ( "MemList_f: unrecognized options\nusage: memlist [all]\n" );
			break;
	}
}

void MemStats_f (void)
{
	Mem_CheckSentinelsGlobal ();
	Mem_PrintStats ();
}


/*
========================
Memory_Init
========================
*/
void Memory_Init (void)
{
	zoneMemPool = Mem_AllocPool ( NULL, "Zone" );
	tempMemPool = Mem_AllocTempPool ( "Temporary Memory" );
}

/*
========================
Memory_InitCommands
========================
*/
void Memory_InitCommands (void)
{
	developerMemory = Cvar_Get ( "developerMemory", "0", 0 );

	Cmd_AddCommand ( "memlist", MemList_f );
	Cmd_AddCommand ( "memstats", MemStats_f );
}

/*
========================
Memory_Shutdown

NOTE: Should be the last called function before shutdown!
========================
*/
void Memory_Shutdown (void)
{
	static qboolean isdown = qfalse;
	mempool_t *pool, *next;

	if ( isdown ) {
		return;
	}

	// set the cvar to NULL so nothing is printed to non-existing console
	developerMemory = NULL;

	Cmd_RemoveCommand ( "memlist" );
	Cmd_RemoveCommand ( "memstats" );

	isdown = qtrue;

	Mem_CheckSentinelsGlobal ();

	for ( pool = poolChain; pool; pool = next ){
		// do it here, because pool is to be freed
		// and the chain will be broken
		next = pool->next;

		Mem_FreePool ( &pool );
	}
}