/*
 *  Filesystem interface layer (trimmed down for XMBurner AluEmu)
 *
 *  Copyright (C) 2016
 *    Sandor Zsuga (Jubatian)
 *  Uzem (the base of CUzeBox) is copyright (C)
 *    David Etherton,
 *    Eric Anderton,
 *    Alec Bourque (Uze),
 *    Filipe Rinaldi,
 *    Sandor Zsuga (Jubatian),
 *    Matt Pandina (Artcfox)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#ifndef FILESYS_H
#define FILESYS_H



#include "types.h"



/*
** The emulator uses this layer to communicate with the underlaying system's
** files. This is provided to make it possible to entirely replace the
** filesystem in need, such as to provide a self-contained package (planned
** mainly for Emscripten use)
*/


/*
** Filesystem channels. These are fixed, statically allocated to various
** parts of the emulator. Each channel can hold one file open at any time.
*/
/* Various emulator one-shot tasks (such as game loading) */
#define FILESYS_CH_EMU     0U

/* Number of filesystem channels (must be one larger than the largest entry
** of the list above) */
#define FILESYS_CH_NO      1U


/*
** Sets path string. All filesystem operations will be carried out on the
** set path. If there is a file name on the end of the path, it is removed
** from it (bare paths must terminate with a '/'). If name is non-null, the
** filename part of the path (if any) is loaded into it (up to len bytes).
*/
void  filesys_setpath(char const* path, char* name, auint len);


/*
** Opens a file. Position is at the beginning. Returns TRUE on success.
** Initially the file internally is opened for reading (so write protected
** files may also be opened), adding write access is only attempted when
** trying to write the file. If there is already a file open, it is closed
** first.
*/
boole filesys_open(auint ch, char const* name);


/*
** Read bytes from a file. The reading increases the internal position.
** Returns the number of bytes read, which may be zero if no file is open
** on the channel or it can not be read (such as because the position is at
** or beyond the end). This automatically reopens the last opened file if
** necessary, seeking to the last set position.
*/
auint filesys_read(auint ch, uint8* dest, auint len);


/*
** Flushes a channel. It internally closes any opened file, safely flushing
** them as needed.
*/
void  filesys_flush(auint ch);


/*
** Flushes all channels. Should be used before exit.
*/
void  filesys_flushall(void);


#endif
