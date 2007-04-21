/* debug.c */

#include <p18cxxx.h>
#include "debug.h"

#define BUFFER_SIZE 64

#ifdef DEBUG
static unsigned char g_buffer[ BUFFER_SIZE ];
static unsigned char g_index_in;     /* points to next free location */
static unsigned char g_index_out;    /* points to char being transmitted */
#endif

#pragma code

void debug_init( void )
{
#ifdef DEBUG
  g_index_in  = 0;
  g_index_out = 0;
  
  TRISC |= 0x80;
  TRISC &= ~0x40;
  SPBRG = 38;     /* fOSC/(64*(38+1)) = 9615 Baud */
  BAUDCON = 0x02; /* wake-up enabled */
  TXSTA = 0x20;   /* transmit enabled */
  RCSTA = 0x90;   /* serial port & receiver enabled */
  PIE1 |= 0x10;   /* enable TX interrupt */
#endif
}

void debug_txint( void )
{
#ifdef DEBUG
  /* a character has been transmitted */
  if ( g_index_in != g_index_out )
  {
    /* advance output pointer */
    g_index_out = ( g_index_out + 1 ) % BUFFER_SIZE;
    
    if ( g_index_in != g_index_out )
    {
      /* there are more characters to send */
      TXREG = g_buffer[ g_index_out ];
    }
  }
#endif
}

void debug_write( unsigned char c )
{
#ifdef DEBUG
  unsigned char old_index_in;
  
  old_index_in = g_index_in;
  g_index_in = ( g_index_in + 1 ) % BUFFER_SIZE;
  
  if ( g_index_in != g_index_out )
  {
    /* buffer is not full */
    g_buffer[ old_index_in ] = c;
    if ( old_index_in == g_index_out )
    {
      /* this is first char in buffer -> write to TXREG */
      TXREG = c;
    }
  }
  else
  {
    /* buffer is full -> replace last character */
    g_index_in = old_index_in;    /* do not advance pointer */
    g_buffer[ ( g_index_in - 1 ) % BUFFER_SIZE ] = 'X';
  }  
#endif
}