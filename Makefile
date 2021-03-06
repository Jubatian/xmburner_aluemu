############
# Makefile #
############
#
#  Copyright (C) 2016
#    Sandor Zsuga (Jubatian)
#  Uzem (the base of CUzeBox) is copyright (C)
#    David Etherton,
#    Eric Anderton,
#    Alec Bourque (Uze),
#    Filipe Rinaldi,
#    Sandor Zsuga (Jubatian),
#    Matt Pandina (Artcfox)
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#
#
# The main makefile of the program
#
#
# make all (or make): build the program
# make clean:         to clean up
#
#

include Make_defines.mk

CFLAGS  +=

OBJECTS  = $(OBD)/main.o
OBJECTS += $(OBD)/cu_hfile.o
OBJECTS += $(OBD)/cu_avr.o
OBJECTS += $(OBD)/cu_avrc.o
OBJECTS += $(OBD)/cu_avrfg.o
OBJECTS += $(OBD)/filesys.o

DEPS     = *.h Makefile Make_defines.mk Make_config.mk

all: $(OUT)
clean:
	rm    -f $(OBJECTS) $(OUT)
	rm -d -f $(OBD)

$(OUT): $(OBD) $(OBJECTS)
	$(CC) -o $(OUT) $(OBJECTS) $(CFSPD) $(LINK)

$(OBD):
	mkdir $(OBD)

# Objects

$(OBD)/main.o: main.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_hfile.o: cu_hfile.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_avr.o: cu_avr.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/cu_avrc.o: cu_avrc.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_avrfg.o: cu_avrfg.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/filesys.o: filesys.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

.PHONY: all clean
