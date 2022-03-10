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
#include "g_local.h"

// Note that the pmenu entries are duplicated
// this is so that a static set of pmenu entries can be used
// for multiple clients and changed without interference
pmenuhnd_t *PMenu_Open(edict_t *ent, pmenu_t *entries, int cur, int num, void *arg)
{
	pmenuhnd_t *hnd;
	pmenu_t *p;
	int i;

	if (!ent->r.client)
		return NULL;

	if (ent->r.client->menu) {
		G_Printf ("warning, ent already has a menu\n");
		PMenu_Close(ent);
	}

	hnd = G_Malloc(sizeof(*hnd));

	hnd->arg = arg;
	hnd->entries = G_Malloc(sizeof(pmenu_t) * num);
	memcpy (hnd->entries, entries, sizeof(pmenu_t) * num);

	// duplicate the strings
	for (i = 0; i < num; i++)
	{
		if (entries[i].text && entries[i].text[0])
			hnd->entries[i].text = G_CopyString (entries[i].text);
	}

	hnd->num = num;

	if (cur < 0 || !entries[cur].SelectFunc) {
		for (i = 0, p = entries; i < num; i++, p++)
			if (p->SelectFunc)
				break;
	} else
		i = cur;

	if (i >= num)
		hnd->cur = -1;
	else
		hnd->cur = i;

	ent->r.client->showscores = qtrue;
	ent->r.client->inmenu = qtrue;
	ent->r.client->menu = hnd;

	trap_Layout (ent, PMenu_Do_Update (ent));

	return hnd;
}

void PMenu_Close(edict_t *ent)
{
	int i;
	pmenuhnd_t *hnd;

	if (!ent->r.client->menu)
		return;

	hnd = ent->r.client->menu;
	for (i = 0; i < hnd->num; i++)
	{
		if (hnd->entries[i].text)
			G_Free (hnd->entries[i].text);
	}

	G_Free (hnd->entries);
	G_Free (hnd);

	ent->r.client->menu = NULL;
	ent->r.client->showscores = qfalse;
}

// only use on pmenu's that have been called with PMenu_Open
void PMenu_UpdateEntry(pmenu_t *entry, const char *text, int align, SelectFunc_t SelectFunc)
{
	G_Free (entry->text);

	entry->text = G_CopyString ((char *)text);
	entry->align = align;
	entry->SelectFunc = SelectFunc;
}

char *PMenu_Do_Update(edict_t *ent)
{
	static char string[MAX_STRING_CHARS];
	int i;
	pmenu_t *p;
	int x;
	pmenuhnd_t *hnd;
	char *t;
	qboolean alt = qfalse;

	if (!ent->r.client->menu) {
		G_Printf ("warning:  ent has no menu\n");
		return NULL;
	}

	hnd = ent->r.client->menu;

	strcpy (string, "size 256 192 xv 32 yv 8 picn inventory ");

	for (i = 0, p = hnd->entries; i < hnd->num; i++, p++) {
		if (!p->text || !*(p->text))
			continue; // blank line

		t = p->text;
		if (*t == '*') {
			alt = qtrue;
			t++;
		}

		sprintf (string + strlen(string), "yv %d ", 32 + i * 16);
		if (p->align == PMENU_ALIGN_CENTER)
			x = 196/2 - strlen(t)*4 + 64;
		else if (p->align == PMENU_ALIGN_RIGHT)
			x = 64 + (196 - strlen(t)*8);
		else
			x = 64;

		sprintf(string + strlen(string), "xv %d ",
			x - ((hnd->cur == i) ? 8 : 0));

		if (hnd->cur == i)
			sprintf(string + strlen(string), "string \"%s\x0d%s\" ", S_COLOR_YELLOW, t);
		else if (alt)
			sprintf(string + strlen(string), "string \"%s%s\" ", S_COLOR_YELLOW, t);
		else
			sprintf(string + strlen(string), "string \"%s\" ", t);
		alt = qfalse;
	}

	return string;
}

void PMenu_Update(edict_t *ent)
{
	if (!ent->r.client->menu) {
		G_Printf ("warning:  ent has no menu\n");
		return;
	}

	if (level.time - ent->r.client->menutime >= 1.0) {
		// been a second or more since last update, update now
		PMenu_Do_Update (ent);
		ent->r.client->menutime = level.time;
		ent->r.client->menudirty = qfalse;
	}

	ent->r.client->menutime = level.time + 0.2;
	ent->r.client->menudirty = qtrue;
}

void PMenu_Next(edict_t *ent)
{
	pmenuhnd_t *hnd;
	int i;
	pmenu_t *p;

	if (!ent->r.client->menu) {
		G_Printf ("warning:  ent has no menu\n");
		return;
	}

	hnd = ent->r.client->menu;

	if (hnd->cur < 0)
		return; // no selectable entries

	i = hnd->cur;
	p = hnd->entries + hnd->cur;
	do {
		i++, p++;
		if (i == hnd->num)
			i = 0, p = hnd->entries;
		if (p->SelectFunc)
			break;
	} while (i != hnd->cur);

	hnd->cur = i;

	PMenu_Update(ent);
}

void PMenu_Prev(edict_t *ent)
{
	pmenuhnd_t *hnd;
	int i;
	pmenu_t *p;

	if (!ent->r.client->menu) {
		G_Printf ("warning:  ent has no menu\n");
		return;
	}

	hnd = ent->r.client->menu;

	if (hnd->cur < 0)
		return; // no selectable entries

	i = hnd->cur;
	p = hnd->entries + hnd->cur;
	do {
		if (i == 0) {
			i = hnd->num - 1;
			p = hnd->entries + i;
		} else
			i--, p--;
		if (p->SelectFunc)
			break;
	} while (i != hnd->cur);

	hnd->cur = i;

	PMenu_Update(ent);
}

void PMenu_Select(edict_t *ent)
{
	pmenuhnd_t *hnd;
	pmenu_t *p;

	if (!ent->r.client->menu) {
		G_Printf ("warning:  ent has no menu\n");
		return;
	}

	hnd = ent->r.client->menu;

	if (hnd->cur < 0)
		return; // no selectable entries

	p = hnd->entries + hnd->cur;

	if (p->SelectFunc)
		p->SelectFunc(ent, hnd);
}
