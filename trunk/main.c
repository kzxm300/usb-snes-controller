/* main.c */

#include <p18cxxx.h>
#include "debug.h"
#include "usb.h"

/* Configuration */
#pragma config FOSC = XTPLL_XT    /* XT oscillator, PLL */
#pragma config PLLDIV = 1         /* 4MHz input */
#pragma config CPUDIV = OSC3_PLL4	/* CPU=96MHz PLL / 4 */
#pragma config FCMEN = OFF        /* Fail-safe clock monitor */
#pragma config IESO = OFF         /* internal/external switch over */
#pragma config PWRT = ON          /* power-up timer */
#pragma config BOR = OFF          /* brown-out reset */
#pragma config WDT = OFF          /* watchdog timer */
#pragma config LVP = OFF          /* low voltage ICSP */
#pragma config VREGEN = ON        /* USB voltage regulator */
#pragma config MCLRE = OFF        /* Master Clear Reset */
#pragma config PBADEN = OFF       /* PORTB are digital I/O */


/* local prototypes */
void high_isr( void );

/* Interrupt Vector */
#pragma code high_vector = 0x08
void interrupt_at_high_vector( void )
{
  _asm goto high_isr _endasm
}
#pragma code    /* default code section */


/* Interrupt Service Routine */
#pragma interrupt high_isr
void high_isr( void )
{
  /* query interrupt flag bits */
  if ( PIR1 & 0x10 )
  {
    /* EUSART TX interrupt */
    debug_txint();
  }
  
  /* TODO: check enabled flags here, in case not all interrupts are 
    always enabled */
  if ( PIR2 & 0x20 )
  {
    /* USB interrupt */
    usb_interrupt();
  }
  
  /* other interrupt flags may be queried here */
  
  /* clear interrupt flag bits */
  PIR1 = 0x00;
  PIR2 = 0x00;
}


/* main entry point */
void main( void )
{
  ADCON1 = 0x0F; /* all pins to digital */
  LATA = 0x01; 
  TRISA = 0x00;  /* all pins to output */
  
  TRISB = 0xC0;
  TRISC = 0x00;

  /* initialize interrupts */
  /* IPEN in RCON is already 0 */
  PIE1 = 0x00;    /* disable interrupt sources */
  PIE2 = 0x00;

  /* initialize EUSART */
  debug_init();

  /* initialize USB */
  usb_init();
  
  /* global interrupt enable */
  INTCON = 0xC0;  
  
  while(1)
  {
//    LATA = 1;
//    LATA = 0;
  }
}
 
