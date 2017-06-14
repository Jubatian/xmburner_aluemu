/*
 *  Filesystem interface layer
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



#include "filesys.h"



/* Size of name in the channel state structure */
#define CH_NSIZE     1280U
/* Size of path */
#define PATH_SIZE    1024U


/* Channel state structure */
typedef struct{
 char   name[CH_NSIZE]; /* The file name opened including base path. Used for re-opening */
 FILE*  fp;       /* Opened file if any */
 boole  rd;       /* If TRUE, file is open for reading, otherwise it is closed */
 boole  wr;       /* If TRUE, file is also open for writes (rd must be TRUE too) */
 auint  pos;      /* Position within the file (virtual) */
}filesys_ch_t;


/* There are compiler problems about the uniform zero initializer ({0}) when
** they are used for more complex things. So the initializer below is a hack,
** it is meant to zero initialize everything. But it relies on FILESYS_CH_NO's
** size, so check here */
#if (FILESYS_CH_NO != 1U)
#error "Check filesys_ch's initializer! FILESYS_CH_NO changed!"
#endif


/* Base path of files */
static char filesys_path[PATH_SIZE] = {0U};

/* Channels */
static filesys_ch_t filesys_ch[FILESYS_CH_NO] = {
 { {0U}, NULL, FALSE, FALSE, 0U},
};



/*
** Combines the set path with the passed filename.
*/
static void filesys_addpath(char* dest, const char* src, auint len)
{
 auint i = 0U;
 auint j = 0U;

 if (len == 0){ return; } /* No sense to call with len set zero */

 while ( (i < (len - 1U)) &&
         (filesys_path[i] != 0) ){
  dest[i] = filesys_path[i];
  i ++;
 }

 while ( (i < (len - 1U)) &&
         (src[j] != 0) ){
  dest[i] = src[j];
  i ++;
  j ++;
 }

 dest[i] = 0;

 return;
}



/*
** Sets path string. All filesystem operations will be carried out on the
** set path. If there is a file name on the end of the path, it is removed
** from it (bare paths must terminate with a '/'). If name is non-null, the
** filename part of the path (if any) is loaded into it (up to len bytes).
*/
void  filesys_setpath(char const* path, char* name, auint len)
{
 auint i = 0U;
 auint j = 0U;

 /* Copy along with receiving string length (into 'i') */

 while ( (i < (PATH_SIZE - 1U)) &&
         (path[i] != 0) ){
  filesys_path[i] = path[i];
  i ++;
 }
 filesys_path[i] = 0;

 /* Walk back to last directory seperator (if any) */

 while (i != 0U){
  i --;
  if ( (filesys_path[i] ==  '/') ||
       (filesys_path[i] == '\\') ){
   i ++;
   break;
  }
 }
 filesys_path[i] = 0; /* Trim everything after last path separator */

 /* If returning the name was requested, do it */

 if ( (name != NULL) &&
      (len != 0U) ){
  while ( (j < (len - 1U)) &&
          (path[i] != 0) ){
   name[j] = path[i];
   i ++;
   j ++;
  }
  name[j] = 0;
 }

 return;
}



/*
** Opens a file. Position is at the beginning. Returns TRUE on success.
** Initially the file internally is opened for reading (so write protected
** files may also be opened), adding write access is only attempted when
** trying to write the file. If there is already a file open, it is closed
** first.
*/
boole filesys_open(auint ch, char const* name)
{
 /* Clean up previously open file if any */

 filesys_flush(ch);

 /* Open new file (or attempt it) */

 filesys_addpath(&(filesys_ch[ch].name[0]), name, CH_NSIZE);
 filesys_ch[ch].fp = fopen(&(filesys_ch[ch].name[0]), "rb");
 if (filesys_ch[ch].fp != NULL){
  filesys_ch[ch].rd = TRUE;
  filesys_ch[ch].wr = FALSE;
 }
 filesys_ch[ch].pos = 0U;

 return filesys_ch[ch].rd;
}



/*
** Read bytes from a file. The reading increases the internal position.
** Returns the number of bytes read, which may be zero if no file is open
** on the channel or it can not be read (such as because the position is at
** or beyond the end). This automatically reopens the last opened file if
** necessary, seeking to the last set position.
*/
auint filesys_read(auint ch, uint8* dest, auint len)
{
 auint rb;

 /* Try to open the file if necessary */

 if (!filesys_ch[ch].rd){
  filesys_ch[ch].fp = fopen(&(filesys_ch[ch].name[0]), "rb");
  if (filesys_ch[ch].fp != NULL){
   filesys_ch[ch].rd = TRUE;
   filesys_ch[ch].wr = FALSE;
   if (filesys_ch[ch].pos != 0U){
    (void)(fseek(filesys_ch[ch].fp, filesys_ch[ch].pos, SEEK_SET));
   }
  }
 }

 /* Read data */

 if (filesys_ch[ch].rd){
  rb = fread(dest, 1U, len, filesys_ch[ch].fp);
  filesys_ch[ch].pos += rb;
  return rb;
 }

 return 0U;
}



/*
** Flushes a channel. It internally closes any opened file, safely flushing
** them as needed.
*/
void  filesys_flush(auint ch)
{
 if (filesys_ch[ch].rd){ fclose(filesys_ch[ch].fp); }
 filesys_ch[ch].rd  = FALSE;
 filesys_ch[ch].wr  = FALSE;
}



/*
** Flushes all channels. Should be used before exit.
*/
void  filesys_flushall(void)
{
 auint i;
 for (i = 0U; i < FILESYS_CH_NO; i ++){
  filesys_flush(i);
 }
}
