/*
 *  Main
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


#include "types.h"
#include "cu_hfile.h"
#include "filesys.h"
#include "cu_avr.h"



/*
** Main entry point
*/
int main (int argc, char** argv)
{
 cu_state_cpu_t*   ecpu;
 char              tstr[128];
 char const*       game = "default.hex";

 if (argc > 1){ game = argv[1]; }
 filesys_setpath(game, &(tstr[0]), 100U); /* Locate everything beside the game */

 ecpu = cu_avr_get_state();

 if (!cu_hfile_load(&(tstr[0]), &(ecpu->crom[0]))){
  return 1;
 }

 ecpu->wd_seed = rand(); /* Seed the WD timeout used for PRNG seed in Uzebox games */

 cu_avr_reset();

 cu_avr_run();

 filesys_flushall();

 print_unf("\n"); /* New line to terminate any text line produced by running code */

 return 0;
}
