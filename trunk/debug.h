#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG

#ifdef DEBUG
  #define DEBUG_OUT(a) debug_write(a)
#else
  #define DEBUG_OUT(a)
#endif

void debug_init( void );
void debug_txint( void );
void debug_write( unsigned char c );

#endif  /* defined DEBUG_H */