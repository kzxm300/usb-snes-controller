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
  if ( ( PIR1 & 0x10 ) && ( PIE1 & 0x10 ) )
  {
    /* EUSART TX interrupt */
    debug_txint();
  }
  
  if ( ( PIR2 & 0x20 ) && ( PIE2 & 0x20 ) )
  {
    /* USB interrupt */
    usb_interrupt();
  }
  
  /* other interrupt flags may be queried here */
  
  /* clear interrupt flag bits */
  PIR1 = 0x00;
  PIR2 = 0x00;
}


/* wait a specific amount of cycles */
/* TODO: rewrite in assembler */
static void delay( unsigned short timeus )
{
  volatile unsigned short cycles;
  
  for ( cycles = timeus * 24; cycles > 0U; --cycles );
}

/* main entry point */
void main( void )
{
  unsigned char  button;  /* current button number */
  unsigned short but_state;  /* bit array of button states */
  
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
  
  /* initialization of SNES interface */
  LATA  |= 0x02;  /* RA1 (clock) to high */
  TRISA |= 0x08;  /* RA3 (data) to input */
  
  while(1)
  {
    /* trigger controller to latch status of all buttons */
    /* send positive pulse on LAT, 12us */
    LATA |= 0x04;
    delay( 12 );
    LATA &= ~0x04;
    
    /* wait 6us for controller to drive first button state */
    delay( 6 );
    
    /* go over all 16 buttons */
    but_state = 0;
    for ( button = 0; button < 16U; ++button )
    {
      /* issue falling edge on CLK */
      LATA &= ~0x02;
      
      /* sample button state from DAT */
      if ( ( PORTA & 0x08 ) == 0U )
      {
        /* button is pressed */
        but_state |= (unsigned short)1 << button;
      }
      
      /* wait 6us */
      delay( 6 );
      
      /* issue rising edge on CLK, controller will drive next bit */
      LATA |= 0x02;

      /* wait 6us for controller to drive next button state */
      delay( 6 );
    }
    
    /* interpret sampled button states */
    if ( but_state != 0U )
    {
      LATA &= ~0x01;
    }
    else
    {
      LATA |= 0x01;
    }
  }
}
 
