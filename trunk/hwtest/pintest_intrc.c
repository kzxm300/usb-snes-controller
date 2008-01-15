/* main.c */

#include <p18cxxx.h>

/* Configuration */
#pragma config FOSC = INTOSCIO_EC	/* Internal oscillator, RA6 available */
#pragma config USBDIV = 1         /* USB runs from PLL */
#pragma config FCMEN = OFF        /* Fail-safe clock monitor */
#pragma config IESO = OFF         /* internal/external switch over */
#pragma config PWRT = ON          /* power-up timer */
#pragma config BOR = OFF          /* brown-out reset */
#pragma config WDT = OFF          /* watchdog timer */
#pragma config LVP = OFF          /* low voltage ICSP */
#pragma config VREGEN = OFF       /* USB voltage regulator */
#pragma config MCLRE = ON         /* Master Clear Reset (must be!) */
#pragma config PBADEN = OFF       /* PORTB are digital I/O */

/* NOTE: MCLRE must be ON when INTRC is selected! */
/* NOTE: PIC does not seem to run when MCLR is floating? */

/* setting bit to 1 includes corresponding pin in test */
static const unsigned char testpins_portA = 0x3C;
static const unsigned char testpins_portB = 0x00;
static const unsigned char testpins_portC = 0x00;

/* NOTE: USB lines cannot be tested because they cannot be set to output */


/* local prototypes */
static void delay( unsigned char timems );

static void delay( unsigned char timems )
{
  _asm
    MOVLW -2  /* operate on first function parameter */
    /* 1 instruction cycle is 4 oscillator periods */
    /* at 31.25kHz it is 128us */
    /* so 8 instruction cycles are about 1ms */
    start:
      DECF PLUSW2, 1, 0   /* 1 instruction cycle */
      BZ done             /* 1 instruction cycle */
      NOP                 /* 1 instruction cycle */
      NOP                 /* 1 instruction cycle */
      NOP                 /* 1 instruction cycle */
      NOP                 /* 1 instruction cycle */
      BRA start           /* 2 instruction cycles */
    done:
  _endasm
}

/* main entry point */
void main( void )
{
  unsigned char i;
  
  ADCON1 = 0x0F; /* all pins to digital */
  LATA = 0x00;
  LATB = 0x00;
  LATC = 0x00;
  TRISA = 0x00;  /* all pins to output */
  TRISB = 0xC0;
  TRISC = 0x00;

  /* initialize interrupts */
  /* IPEN in RCON is already 0 */
  PIE1 = 0x00;    /* disable interrupt sources */
  PIE2 = 0x00;

  /* initializes power mode settings */
  OSCCON = 0x00;  /* Sleep mode enabled, primary oscillator */
  
  while (1)
  {
    for ( i = 0; i < 8U; i++ )
    {
      if ( ( testpins_portA & ( 1 << i ) ) != 0U )
      {
        /* we have to test this pin */
        LATA |= 1 << i;        /* set to 1 */
        delay( 250 );
        delay( 250 );
        delay( 250 );
        delay( 250 );
        LATA &= (unsigned char)~( 1 << i );  /* set to 0 */
      }
    }
  }
}
 
