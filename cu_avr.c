/*
 *  AVR microcontroller emulation
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



#include "cu_avr.h"
#include "cu_avrc.h"
#include "cu_avrfg.h"



/* Marker for this compilation unit for sub-includes */
#define CU_AVR_C



/* CPU state */
cu_state_cpu_t  cpu_state;

/* Compiled AVR instructions */
uint32          cpu_code[32768];

/* Access info structure for SRAM */
uint8           access_mem[4096U];

/* Access info structure for I/O */
uint8           access_io[256U];

/* Precalculated flags */
uint8           cpu_pflags[CU_AVRFG_SIZE];

/* Whether the flags were already precalculated */
boole           pflags_done = FALSE;

/* Next hardware event's cycle. It can be safely set to
** WRAP32(cpu_state.cycle + 1U) to force full processing next time. */
auint           cycle_next_event;

/* Timer1 TCNT1 adjustment value: WRAP32(cpu_state.cycle - timer1_base)
** gives the correct TCNT1 any time. */
auint           timer1_base;

/* Interrupt might be waiting to be serviced flag. This is set nonzero by
** any event which should trigger an IT including setting the I flag in the
** status register. It can be safely set nonzero to ask for a certain IT
** check. */
boole           event_it;

/* Interrupt entry necessary if set. */
boole           event_it_enter;

/* Vector to call when entering interrupt */
auint           event_it_vect;

/* ALU behaviour modification: Set if enabled. */
boole           alu_ismod;

/* Maximal number of cycles to emulate. */
auint           cycle_count_max;

/* Guard port accessed (second access terminates) */
boole           guard_isacc;

/* Port state machines for 0xE0 - 0xFF */
auint           port_states[0x20U];

/* Port data for 0xE0 - 0xFF */
uint8           port_data[0x20U][4U];

/* Stuck bits, AND mask, RAM */
uint8           stuck_0_mem[4096U];

/* Stuck bits, OR mask, RAM */
uint8           stuck_1_mem[4096U];

/* Stuck bits, AND mask, IO */
uint8           stuck_0_io[256U];

/* Stuck bits, OR mask, IO */
uint8           stuck_1_io[256U];

/* Stuck bits, AND mask, ROM */
uint8           stuck_0_rom[65536U];

/* Stuck bits, OR mask, ROM */
uint8           stuck_1_rom[65536U];




/* Initial maximal number of cycles */
#define CYCLE_COUNT_MAX_INI 1000000U



/* Flags in CU_IO_SREG */
#define SREG_I  7U
#define SREG_T  6U
#define SREG_H  5U
#define SREG_S  4U
#define SREG_V  3U
#define SREG_N  2U
#define SREG_Z  1U
#define SREG_C  0U
#define SREG_IM 0x80U
#define SREG_TM 0x40U
#define SREG_HM 0x20U
#define SREG_SM 0x10U
#define SREG_VM 0x08U
#define SREG_NM 0x04U
#define SREG_ZM 0x02U
#define SREG_CM 0x01U


/* Macros for managing the flags */

/* Clear flags by mask */
#define SREG_CLR(fl, xm) (fl &= (auint)(~((auint)(xm))))

/* Set flags by mask */
#define SREG_SET(fl, xm) (fl |= (xm))

/* Set Zero if (at most) 16 bit "val" is zero */
#define SREG_SET_Z(fl, val) (fl |= SREG_ZM & (((auint)(val) - 1U) >> 16))

/* Set Carry by bit 15 (for multiplications) */
#define SREG_SET_C_BIT15(fl, val) (fl |= ((val) >> 15) & 1U)

/* Set Carry by bit 16 (for float multiplications and adiw & sbiw) */
#define SREG_SET_C_BIT16(fl, val) (fl |= ((val) >> 16) & 1U)

/* Combine N and V into S (for all ops updating N or V) */
#define SREG_COM_NV(fl) (fl |= (((fl) << 1) ^ ((fl) << 2)) & 0x10U)

/* Get carry flag (for carry overs) */
#define SREG_GET_C(fl) ((fl) & 1U)


/* Macro for calculating a multiplication's flags */
#define PROCFLAGS_MUL(fl, res) \
 do{ \
  SREG_CLR(fl, SREG_CM | SREG_ZM); \
  SREG_SET_C_BIT15(fl, res); \
  SREG_SET_Z(fl, res & 0xFFFFU); \
 }while(0)

/* Macro for calculating a floating multiplication's flags */
#define PROCFLAGS_FMUL(fl, res) \
 do{ \
  SREG_CLR(fl, SREG_CM | SREG_ZM); \
  SREG_SET_C_BIT16(fl, res); \
  SREG_SET_Z(fl, res & 0xFFFFU); \
 }while(0)


/* Macro for updating hardware from within instructions */
#define UPDATE_HARDWARE \
 do{ \
  cpu_state.cycle = WRAP32(cpu_state.cycle + 1U); \
  if (cycle_next_event == cpu_state.cycle){ cu_avr_hwexec(); } \
 }while(0)

/* Macro for consuming last instruction cycle before which ITs are triggered */
#define UPDATE_HARDWARE_IT \
 do{ \
  if (event_it){ cu_avr_itcheck(); } \
  UPDATE_HARDWARE; \
 }while(0)


/* Vectors base when IVSEL is 1 (MCUCR.1; Boot loader base) */
#define VBASE_BOOT     0x7800U


/* Various vectors */

/* Reset */
#define VECT_RESET     0x0000U
/* Timer1 Comparator A */
#define VECT_T1COMPA   0x001AU
/* Timer1 Comparator B */
#define VECT_T1COMPB   0x001CU
/* Timer1 Overflow */
#define VECT_T1OVF     0x001EU



/*
** Emulates cycle-precise hardware tasks. This is called through the
** UPDATE_HARDWARE macro if cycle_next_event matches the cycle counter (a new
** HW event is to be processed).
*/
static void cu_avr_hwexec(void)
{
 auint nextev = ~0U;
 auint t0;
 auint t1;
 auint t2;

 /* Timer 1 */

 if ((cpu_state.iors[CU_IO_TCCR1B] & 0x07U) != 0U){ /* Timer 1 started */

  t0 = (cpu_state.cycle - timer1_base) & 0xFFFFU;   /* Current TCNT1 value */
  t1 = ( ( ((auint)(cpu_state.iors[CU_IO_OCR1AL])     ) |
           ((auint)(cpu_state.iors[CU_IO_OCR1AH]) << 8) ) + 1U) & 0xFFFFU;
  t2 = ( ( ((auint)(cpu_state.iors[CU_IO_OCR1BL])     ) |
           ((auint)(cpu_state.iors[CU_IO_OCR1BH]) << 8) ) + 1U) & 0xFFFFU;

  if ((cpu_state.iors[CU_IO_TCCR1B] & 0x08U) != 0U){ /* Timer 1 in CTC mode: Counts to OCR1A, then resets */

   if (t0 == 0x0000U){                    /* Timer overflow (might happen if it starts above Comp. A, or Comp. A is 0) */
    cpu_state.iors[CU_IO_TIFR1] |= 0x01U;
    event_it = TRUE;
   }

   if (t0 == t2){
    cpu_state.iors[CU_IO_TIFR1] |= 0x04U; /* Comparator B interrupt */
    event_it = TRUE;
   }

   if (t0 == t1){
    cpu_state.iors[CU_IO_TIFR1] |= 0x02U; /* Comparator A interrupt */
    event_it = TRUE;
    timer1_base = cpu_state.cycle;        /* Reset timer to zero */
    t0 = 0U;                              /* Also reset for event calculation */
   }

   if ( (t0 != t2) &&
        (nextev > (t2 - t0)) ){ nextev = t2 - t0; } /* Next Comp. B match */
   if ( (nextev > (t1 - t0)) ){ nextev = t1 - t0; } /* Next Comp. A match */
   if ( (nextev > (0x10000U - t0)) ){ nextev = 0x10000U - t0; } /* Next Overflow (if timer was set above Comp. A) */

  }else{ /* Timer 1 in normal mode: wrapping 16 bit counter */

   if (t0 == 0x0000U){
    cpu_state.iors[CU_IO_TIFR1] |= 0x01U; /* Overflow interrupt */
    event_it = TRUE;
    timer1_base = cpu_state.cycle;        /* Reset timer to zero */
   }

   if (nextev > (0x10000U - t0)){ nextev = 0x10000U - t0; }

  }

 }

 /* Calculate next event's cycle */

 cycle_next_event = WRAP32(cpu_state.cycle + nextev);
}



/*
** Enters requested interrupt (event_it_enter must be true)
*/
static void cu_avr_interrupt(void)
{
 auint tmp;

 event_it_enter = FALSE; /* Requested IT entry performed */

 SREG_CLR(cpu_state.iors[CU_IO_SREG], SREG_IM);

 tmp   = ((auint)(cpu_state.iors[CU_IO_SPL])     ) +
         ((auint)(cpu_state.iors[CU_IO_SPH]) << 8);
 cpu_state.sram[tmp & 0x0FFFU] = (cpu_state.pc     ) & 0xFFU;
 access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
 tmp --;
 cpu_state.sram[tmp & 0x0FFFU] = (cpu_state.pc >> 8) & 0xFFU;
 access_mem[tmp & 0x0FFFU] |= CU_MEM_W;
 tmp --;
 cpu_state.iors[CU_IO_SPL] = (tmp     ) & 0xFFU;
 cpu_state.iors[CU_IO_SPH] = (tmp >> 8) & 0xFFU;

 cpu_state.pc = event_it_vect;

 UPDATE_HARDWARE;
 UPDATE_HARDWARE;
 UPDATE_HARDWARE;
}



/*
** Checks for interrupts and triggers if any is pending. Clears event_it when
** there are no more interrupts waiting for servicing.
*/
static void cu_avr_itcheck(void)
{
 auint vbase;

 /* Global interrupt enable? */

 if ((cpu_state.iors[CU_IO_SREG] & SREG_IM) == 0U){
  event_it = FALSE;
  return;
 }

 /* Interrupts are enabled, so check them */

 vbase = ((cpu_state.iors[CU_IO_MCUCR] >> 1) & 1U) * VBASE_BOOT;

 if       ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x02U) !=    0U ){ /* Timer 1 Comparator A */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x02U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1COMPA;

 }else if ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x04U) !=    0U ){ /* Timer 1 Comparator B */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x04U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1COMPB;

 }else if ( (cpu_state.iors[CU_IO_TIFR1] &
             cpu_state.iors[CU_IO_TIMSK1] & 0x01U) !=    0U ){ /* Timer 1 Overflow */

  cpu_state.iors[CU_IO_TIFR1] ^= 0x01U;
  event_it_enter = TRUE;
  event_it_vect  = vbase + VECT_T1OVF;

 }else{ /* No interrupts are pending */

  event_it = FALSE;

 }
}



/*
** Writes an I/O port
*/
static void  cu_avr_write_io(auint port, auint val)
{
 auint pval = cpu_state.iors[port]; /* Previous value */
 auint cval = val & 0xFFU;          /* Current (requested) value */
 auint t0;

 access_io[port] |= CU_MEM_W;

 switch (port){

  case CU_IO_PORTA:   /* Controller inputs, SPI RAM Chip Select */
  case CU_IO_DDRA:

   break;

  case CU_IO_PORTB:   /* Sync output */
  case CU_IO_DDRB:

   break;

  case CU_IO_PORTC:   /* Pixel output */

   /* Special shortcut masking with the DDR register. This port tolerates a
   ** bit of inaccuracy since it is only graphics and is most frequently
   ** written "normally" (the DDR is used for fade effects on it) */
   cval &= cpu_state.iors[CU_IO_DDRC];
   break;

  case CU_IO_PORTD:   /* SD card Chip Select */
  case CU_IO_DDRD:

   break;

  case CU_IO_OCR2A:   /* PWM audio output */

   break;

  case CU_IO_TCNT1H:  /* Timer1 counter, high */

   cpu_state.latch = cval; /* Write into latch (value written to the port itself is ignored) */
   break;

  case CU_IO_TCNT1L:  /* Timer1 counter, low */

   t0    = (cpu_state.latch << 8) | cval;
   timer1_base = WRAP32(cpu_state.cycle - t0);
   cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
   break;

  case CU_IO_TIFR1:   /* Timer1 interrupt flags */

   cval  = pval & (~cval);
   break;

  case CU_IO_TCCR1B:  /* Timer1 control */
  case CU_IO_OCR1AH:  /* Timer1 comparator A, high */
  case CU_IO_OCR1AL:  /* Timer1 comparator A, low */
  case CU_IO_OCR1BH:  /* Timer1 comparator B, high */
  case CU_IO_OCR1BL:  /* Timer1 comparator B, low */

   cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
   break;

  case CU_IO_SPDR:    /* SPI data */

   break;

  case CU_IO_SPCR:    /* SPI control */

   break;

  case CU_IO_SPSR:    /* SPI status */

   break;

  case CU_IO_EECR:    /* EEPROM control */

   break;

  case CU_IO_EEARH:   /* EEPROM address & data registers */
  case CU_IO_EEARL:
  case CU_IO_EEDR:

   break;

  case CU_IO_WDTCSR:  /* Watchdog timer control */

   break;

  case CU_IO_SPMCSR:  /* Store program memory control & status */

   break;

  case CU_IO_SREG:    /* Status register */

   if ((((~pval) & cval) & SREG_IM) != 0U){
    event_it = TRUE;  /* Interrupts become enabled, so check them */
   }
   break;

  case 0xE0U:         /* Single character output */

   print_message("%c", cval);
   break;

  case 0xE1U:         /* Decimal number output */

   print_message("%u", cval);
   break;

  case 0xE2U:         /* Hexadecimal number output */

   print_message("%02X", cval);
   break;

  case 0xE3U:         /* Binary number output */

   print_message("%u%u%u%u%u%u%u%u",
                 (cval >> 7) & 1U,
                 (cval >> 6) & 1U,
                 (cval >> 5) & 1U,
                 (cval >> 4) & 1U,
                 (cval >> 3) & 1U,
                 (cval >> 2) & 1U,
                 (cval >> 1) & 1U,
                 (cval     ) & 1U);
   break;

  case 0xE7U:         /* Terminate program */

   cycle_count_max = cpu_state.cycle;
   break;

  case 0xE8U:         /* Guard port */

   if (guard_isacc){ cycle_count_max = cpu_state.cycle; } /* Terminate program */
   guard_isacc = TRUE;
   break;

  case 0xEAU:         /* Reset sequentally accessed ports */

   if (cpu_state.iors[0xE9U] == 0xA5U){ /* Port lock inactive */
    for (t0 = 0U; t0 < 0x20U; t0++){
     port_states[t0] = 0U;
    }
   }else{
    cval = pval;
   }
   break;

  case 0xEBU:         /* Count of cycles to emulate */

   if (cpu_state.iors[0xE9U] == 0xA5U){ /* Port lock inactive */
    switch (port_states[0x0BU]){
     case 0U: port_data[0x0BU][0U] = cval; port_states[0x0BU]++; break;
     case 1U: port_data[0x0BU][1U] = cval; port_states[0x0BU]++; break;
     case 2U: port_data[0x0BU][2U] = cval; port_states[0x0BU]++; break;
     default:
      cycle_count_max = ((auint)(port_data[0x0BU][0U])      ) |
                        ((auint)(port_data[0x0BU][1U]) <<  8) |
                        ((auint)(port_data[0x0BU][2U]) << 16) |
                        ((auint)(cval)                 << 24);
      port_states[0x0BU] = 0U;
      break;
    }
   }else{
    cval = pval;
   }
   break;

  case 0xF0U:         /* Behaviour mod. enable */

   if (cpu_state.iors[0xE9U] == 0xA5U){ /* Port lock inactive */
    if (cval != 0x5AU){ alu_ismod = FALSE; } /* An "ijmp" will enable it */
   }else{
    cval = pval;
   }
   break;

  case 0xF1U:         /* Register / Memory stuck bits */

   if (cpu_state.iors[0xE9U] == 0xA5U){ /* Port lock inactive */
    switch (port_states[0x11U]){
     case 0U: port_data[0x11U][0U] = cval; port_states[0x11U]++; break;
     case 1U: port_data[0x11U][1U] = cval; port_states[0x11U]++; break;
     case 2U: port_data[0x11U][2U] = cval; port_states[0x11U]++; break;
     default:
      t0 = ((auint)(port_data[0x0BU][2U])     ) |
           ((auint)(cval)                 << 8);
      if (t0 < 256U){
       stuck_1_io[t0] = port_data[0x11U][0U];
       stuck_0_io[t0] = port_data[0x11U][1U];
      }else{
       stuck_1_mem[t0 & 0x0FFFU] = port_data[0x11U][0U];
       stuck_0_mem[t0 & 0x0FFFU] = port_data[0x11U][1U];
      }
      port_states[0x11U] = 0U;
      break;
    }
   }else{
    cval = pval;
   }
   break;

  case 0xF2U:         /* ROM stuck bits */

   if (cpu_state.iors[0xE9U] == 0xA5U){ /* Port lock inactive */
    switch (port_states[0x12U]){
     case 0U: port_data[0x12U][0U] = cval; port_states[0x12U]++; break;
     case 1U: port_data[0x12U][1U] = cval; port_states[0x12U]++; break;
     case 2U: port_data[0x12U][2U] = cval; port_states[0x12U]++; break;
     case 3U: port_data[0x12U][3U] = cval; port_states[0x12U]++; break;
     default:
      t0 = ((auint)(port_data[0x0BU][2U])     ) |
           ((auint)(port_data[0x0BU][3U]) << 8);
      stuck_1_rom[t0] = port_data[0x12U][0U];
      stuck_0_rom[t0] = port_data[0x12U][1U];
      port_states[0x12U] = 0U;
      break;
    }
   }else{
    cval = pval;
   }
   break;

  default:

   break;

 }

 cpu_state.iors[port] = cval;
}



/*
** Reads from an I/O port
*/
static auint cu_avr_read_io(auint port)
{
 auint t0;
 auint ret;

 access_io[port] |= CU_MEM_R;

 switch (port){

  case CU_IO_TCNT1L:
   t0  = WRAP32(cpu_state.cycle - timer1_base); /* Current TCNT1 value */
   cpu_state.latch = (t0 >> 8) & 0xFFU;
   ret = t0 & 0xFFU;
   break;

  case CU_IO_TCNT1H:
   ret = cpu_state.latch;
   break;

  case 0xE7U:         /* Terminate program */

   cycle_count_max = cpu_state.cycle;
   break;

  case 0xE8U:         /* Guard port */

   if (guard_isacc){ cycle_count_max = cpu_state.cycle; } /* Terminate program */
   guard_isacc = TRUE;
   break;

  default:
   ret = cpu_state.iors[port];
   if (alu_ismod){
    ret &= stuck_0_io[port];
    ret |= stuck_1_io[port];
   }
   break;
 }

 return ret;
}



/*
** Emulates a single (compiled) AVR instruction and any associated hardware
** tasks.
*/
#include "cu_avr_e.h"



/*
** Resets the CPU as if it was power-cycled. It properly initializes
** everything from the state as if cu_avr_crom_update() and cu_avr_io_update()
** was called.
*/
void  cu_avr_reset(void)
{
 auint i;

 for (i = 0U; i < 4096U; i++){
  access_mem[i] = 0U;
  stuck_0_mem[i] = 0xFFU;
  stuck_1_mem[i] = 0x00U;
 }

 for (i = 0U; i < 256U; i++){
  access_io[i] = 0U;
  stuck_0_io[i] = 0xFFU;
  stuck_1_io[i] = 0x00U;
 }

 for (i = 0U; i < 65536U; i++){
  stuck_0_rom[i] = 0xFFU;
  stuck_1_rom[i] = 0x00U;
 }

 for (i = 0U; i < 256U; i++){ /* Most I/O regs are reset to zero */
  cpu_state.iors[i] = 0U;
 }
 cpu_state.iors[CU_IO_SPL] = 0xFFU;
 cpu_state.iors[CU_IO_SPH] = 0x10U;

 cpu_state.latch = 0U;

 /* Check whether something is loaded. Normally the AVR is fused to use the
 ** boot loader, so if anything is present there, use that. If there is no
 ** boot loader (only an application is loaded), boot that instead. */

 if ( (cpu_state.crom[(VBASE_BOOT * 2U) + 0U] != 0U) ||
      (cpu_state.crom[(VBASE_BOOT * 2U) + 1U] != 0U) ){ cpu_state.iors[CU_IO_MCUCR] |= 2U; }

 cpu_state.pc = (((cpu_state.iors[CU_IO_MCUCR] >> 1) & 1U) * VBASE_BOOT) + VECT_RESET;

 cpu_state.cycle    = 0U;
 cycle_next_event   = WRAP32(cpu_state.cycle + 1U);
 timer1_base        = cpu_state.cycle;
 event_it           = TRUE;
 event_it_enter     = FALSE;
 alu_ismod          = FALSE;
 cycle_count_max    = CYCLE_COUNT_MAX_INI;
 guard_isacc        = FALSE;

 for (i = 0U; i < 0x20U; i++){
  port_states[i] = 0U;
 }

 if (!pflags_done){
  cu_avrfg_fill(&cpu_pflags[0]);
  pflags_done = TRUE;
 }

 cu_avr_crom_update(0U, 65536U);
 cu_avr_io_update();

 cpu_state.crom_mod = FALSE; /* Initial code ROM state: not modified. */
}



/*
** Run emulation. Returns according to the return values defined in cu_types
** (emulating up to about 2050 cycles).
*/
auint cu_avr_run(void)
{
 auint i;

 for (i = 0U; i < cycle_count_max; i++){
  cu_avr_exec();       /* Note: This inlines as only this single call exists */
 }

 return 0U;
}



/*
** Returns emulator's cycle counter. It may be used to time emulation when it
** doesn't generate proper video signal. This is the cycle member of the CPU
** state (32 bits wrapping).
*/
auint cu_avr_getcycle(void)
{
 return cpu_state.cycle;
}



/*
** Returns memory access info block. It can be written (with zeros) to clear
** flags which are only set by the emulator. Note that the highest 256 bytes
** of the RAM come first here! (so address 0x0100 corresponds to AVR address
** 0x0100)
*/
uint8* cu_avr_get_meminfo(void)
{
 return &access_mem[0];
}


/*
** Returns I/O register access info block. It can be written (with zeros) to
** clear flags which are only set by the emulator.
*/
uint8* cu_avr_get_ioinfo(void)
{
 return &access_io[0];
}


/*
** Returns whether the Code ROM was modified since reset. This can be used to
** determine if it is necessary to include the Code ROM in a save state.
** Internal Code ROM writes and the cu_avr_crom_update() function can set it.
*/
boole cu_avr_crom_ismod(void)
{
 return cpu_state.crom_mod;
}



/*
** Returns AVR CPU state structure. It may be written, the Code ROM must be
** recompiled (by cu_avr_crom_update()) if anything in that area was updated
** or freshly written.
*/
cu_state_cpu_t* cu_avr_get_state(void)
{
 auint t0 = WRAP32(cpu_state.cycle - timer1_base); /* Current TCNT1 value */

 cpu_state.iors[CU_IO_TCNT1H] = (t0 >> 8) & 0xFFU;
 cpu_state.iors[CU_IO_TCNT1L] = (t0     ) & 0xFFU;

 return &cpu_state;
}



/*
** Updates a section of the Code ROM. This must be called after writing into
** the Code ROM so the emulator recompiles the affected instructions. The
** "base" and "len" parameters specify the range to update in bytes.
*/
void  cu_avr_crom_update(auint base, auint len)
{
 auint wbase = base >> 1;
 auint wlen  = (len + (base & 1U) + 1U) >> 1;
 auint i;

 if (wbase > 0x7FFFU){ wbase = 0x7FFFU; }
 if ((wbase + wlen) > 0x8000U){ wlen = 0x8000U - wbase; }

 for (i = wbase; i < (wbase + wlen); i++){
  cpu_code[i] = cu_avrc_compile(
      ((auint)(cpu_state.crom[((i << 1) + 0U) & 0xFFFFU])     ) |
      ((auint)(cpu_state.crom[((i << 1) + 1U) & 0xFFFFU]) << 8),
      ((auint)(cpu_state.crom[((i << 1) + 2U) & 0xFFFFU])     ) |
      ((auint)(cpu_state.crom[((i << 1) + 3U) & 0xFFFFU]) << 8) );
 }

 cpu_state.crom_mod = TRUE;
}



/*
** Updates the I/O area. If any change is performed in the I/O register
** contents (iors, 0x20 - 0xFF), this have to be called to update internal
** emulator state over it. It also updates state related to additional
** variables in the structure (such as the watchdog timer).
*/
void  cu_avr_io_update(void)
{
 auint t0;

 t0    = (cpu_state.iors[CU_IO_TCNT1H] << 8) |
         (cpu_state.iors[CU_IO_TCNT1L]     );
 timer1_base = WRAP32(cpu_state.cycle - t0);

 cycle_next_event = WRAP32(cpu_state.cycle + 1U); /* Request HW processing */
 event_it         = TRUE; /* Request interrupt processing */
}
