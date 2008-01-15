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

/* pins on PortA */
enum snes_pins
{
  SNES_LATCH = 0x04,
  SNES_CLOCK = 0x20,
  SNES_DATA  = 0x08,
  SNES_VCC   = 0x10
};

/* SNES buttons */
enum snes_buttons
{
  BUT_B      = 0x0001,
  BUT_Y      = 0x0002,
  BUT_SELECT = 0x0004,
  BUT_START  = 0x0008,
  BUT_UP     = 0x0010,
  BUT_DOWN   = 0x0020,
  BUT_LEFT   = 0x0040,
  BUT_RIGHT  = 0x0080,
  BUT_A      = 0x0100,
  BUT_X      = 0x0200,
  BUT_L      = 0x0400,
  BUT_R      = 0x0800
};

/* local prototypes */
void high_isr( void );
static void delay( unsigned char timeus );


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
  if ( ( PIE1 & 0x10 ) && ( PIR1 & 0x10 ) )
  {
    /* EUSART TX interrupt */
    debug_txint();
  }
  
  if ( ( PIE2 & 0x20 ) && ( PIR2 & 0x20 ) )
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
static void delay( unsigned char timeus )
{
  _asm
    MOVLW -2  /* operate on first function parameter */
    start:
      DECF PLUSW2, 1, 0
      BZ done
      NOP        /* 6 NOPs would take 1us */
      NOP
      BRA start
    done:
  _endasm
}

/* main entry point */
void main( void )
{
  unsigned char  but;         /* current button number */
  unsigned short buttons;     /* bit array of button states */
  unsigned short old_buttons; /* old value of butstates */ 
  
  ADCON1 = 0x0F; /* all pins to digital */
  LATA = 0x01; 
  TRISA = 0x00;  /* all pins to output */
  
  TRISB = 0xC0;
  TRISC = 0x00;

  /* initialize interrupts */
  /* IPEN in RCON is already 0 */
  PIE1 = 0x00;    /* disable interrupt sources */
  PIE2 = 0x00;

  /* initializes power mode settings */
  OSCCON = 0x00;  /* Sleep mode enabled, primary oscillator */
  
  /* initialize EUSART */
  debug_init();

  /* initialize USB */
  usb_init();
  
  /* global interrupt enable */
  INTCON = 0xC0;  
  
  /* initialization of SNES interface */
  LATA  |= SNES_VCC;    /* RA4 (supply) to high */
  LATA  |= SNES_CLOCK;  /* RA1 (clock) to high */
  TRISA |= SNES_DATA;   /* RA3 (data) to input */
  
  while (1)
  {
    /* trigger controller to latch status of all buttons */
    /* send positive pulse on LAT, 12us */
    LATA |= SNES_LATCH;
    delay( 12 );
    LATA &= ~SNES_LATCH;
    
    /* wait 6us for controller to drive first button state */
    delay( 6 );
    
    /* go over all 16 buttons */
    old_buttons = buttons;
    buttons = 0;
    for ( but = 0; but < 16U; ++but )
    {
      /* issue falling edge on CLK */
      LATA &= ~SNES_CLOCK;
      
      /* sample button state from DAT */
      if ( ( PORTA & SNES_DATA ) == 0U )
      {
        /* button is pressed */
        buttons |= (unsigned short)1 << but;
      }
      
      /* wait 6us */
      delay( 6 );
      
      /* issue rising edge on CLK, controller will drive next bit */
      LATA |= SNES_CLOCK;

      /* wait 6us for controller to drive next button state */
      delay( 6 );
    }
    
    /* interpret sampled button states */
    if ( buttons != 0U )
    {
      LATA &= ~0x01;
    }
    else
    {
      LATA |= 0x01;
    }
    if ( buttons != old_buttons )
    {
      /* state of buttons changed -> re-interpret them */
      g_hidreport[0] = 0;
      g_hidreport[1] = 0;
      
      if ( buttons & BUT_LEFT )   g_hidreport[0] |= 0x03;
      if ( buttons & BUT_RIGHT )  g_hidreport[0] |= 0x01;
      if ( buttons & BUT_DOWN )   g_hidreport[0] |= 0x04;
      if ( buttons & BUT_UP )     g_hidreport[0] |= 0x0C;
      if ( buttons & BUT_B )      g_hidreport[1] |= 0x01;
      if ( buttons & BUT_Y )      g_hidreport[1] |= 0x02;
      if ( buttons & BUT_A )      g_hidreport[1] |= 0x04;
      if ( buttons & BUT_X )      g_hidreport[1] |= 0x08;
      if ( buttons & BUT_L )      g_hidreport[1] |= 0x10;
      if ( buttons & BUT_R )      g_hidreport[1] |= 0x20;
      if ( buttons & BUT_START )  g_hidreport[1] |= 0x40;
      if ( buttons & BUT_SELECT ) g_hidreport[1] |= 0x80;
      
      /* inform USB that new values are present */
      usb_reportchanged();
    }
  }
}
 
