/* main.c */

#include <p18cxxx.h>
#include "debug.h"

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

/* bit names of USB registers */
/* BDnSTAT register */
#define _UOWN     0x80
#define _DTS      0x40
#define _DTSEN    0x08
#define _BSTALL   0x04
/* UCON register */
#define _PPBRST   0x40
#define _SE0      0x20
#define _PKTDIS   0x10
#define _USBEN    0x08
#define _RESUME   0x04
#define _SUSPND   0x02
/* USTAT register */
#define _DIR      0x04
/* UEPn register */
#define _EPHSHK   0x10
#define _EPCONDIS 0x08
#define _EPOUTEN  0x04
#define _EPINEN   0x02
#define _EPSTALL  0x01

/* PID values in BDnSTAT register */
#define PID_OUT   (unsigned char)(0x1 << 2)
#define PID_IN    (unsigned char)(0x9 << 2)
#define PID_SOF   (unsigned char)(0x5 << 2)
#define PID_SETUP (unsigned char)(0xD << 2)
#define PID_DATA0 (unsigned char)(0x3 << 2)
#define PID_DATA1 (unsigned char)(0xB << 2)
#define PID_DATA2 (unsigned char)(0x7 << 2)
#define PID_MDATA (unsigned char)(0xF << 2)
#define PID_ACK   (unsigned char)(0x2 << 2)
#define PID_NAK   (unsigned char)(0xA << 2)
#define PID_STALL (unsigned char)(0xE << 2)
#define PID_NYET  (unsigned char)(0x6 << 2)
#define PID_ERR   (unsigned char)(0xC << 2)
#define PID_SPLIT (unsigned char)(0x8 << 2)
#define PID_PING  (unsigned char)(0x4 << 2)

/* USB request numbers */
enum req_num
{
  REQ_GET_STATUS        = 0x00,
  REQ_CLEAR_FEATURE     = 0x01,
  REQ_SET_FEATURE       = 0x03,
  REQ_SET_ADDRESS       = 0x05,
  REQ_GET_DESCRIPTOR    = 0x06,
  REQ_SET_DESCRIPTOR    = 0x07,
  REQ_GET_CONFIGURATION = 0x08,
  REQ_SET_CONFIGURATION = 0x09,
  REQ_GET_INTERFACE     = 0x0A,
  REQ_SET_INTERFACE     = 0x0B,
  REQ_SYNC_FRAME        = 0x0C
};

/* USB descriptor values */
enum desc_num
{
  /* standard descriptors */
  DESC_DEVICE        = 0x01,
  DESC_CONFIGURATION = 0x02,
  DESC_STRING        = 0x03,
  DESC_INTERFACE     = 0x04,
  DESC_ENDPOINT      = 0x05,
  DESC_DEVICE_QUALIFIER          = 0x06,
  DESC_OTHER_SPEED_CONFIGURATION = 0x07,
  DESC_INTERFACE_POWER           = 0x08,
  /* class descriptors */
  DESC_HID           = 0x21,
  DESC_HUB           = 0x29,
  DESC_REPORT        = 0x22,
  DESC_PHYSICAL      = 0x23
};

/* type of transfer currently performed */
enum trf_type
{
  TRF_NONE,
  TRF_IN,
  TRF_OUT
};

/* setup stage of a control transfer */
struct ctrltrf_setup
{
  unsigned char  bmRequestType;
  unsigned char  bRequest;
  unsigned short wValue;
  unsigned short wIndex;
  unsigned short wLength;
};

/* one entry in the buffer descriptor table */
struct bd_entry
{
  unsigned char  BDSTAT;  /* BD Status register */
  unsigned char  BDCNT;   /* BD Byte Count register */
  unsigned short BDADR;   /* BD Address register */
};

 
static const unsigned char dev_desc[] =
{
  18,           /* bLength: descriptor size in bytes */
  DESC_DEVICE,  /* bDescriptorType */
  0x00, 0x02,   /* bcdUSB: USB spec release number */
  0x00,         /* bDeviceClass: class code */
  0x00,         /* bDeviceSubclass: subclass code */
  0x00,         /* bDeviceProtocol: protocol code */
  0x08,         /* bMaxPacketSize: max packet size for EP0 */
  0x34, 0x12,   /* idVendor: vendor ID */
  0x01, 0x00,   /* idProduct: product ID */
  0x01, 0x00,   /* bcdDevice: device release number */
  0x00,         /* iManufacturer: index of string desc. */
  0x00,         /* iProduct: index of string desc. */
  0x00,         /* iSerialNumber: index of string desc. */
  0x01          /* bNumConfiguration: number of possible configs */
};

static const unsigned char cfg_desc[] =
{
  /* configuration descriptor */
  9,                  /* bLength: descriptor size in bytes */
  DESC_CONFIGURATION, /* bDescriptorType */
  sizeof( cfg_desc ), 0, /* wTotalLength: size of all data for this config */
  1,                  /* bNumInterfaces: number of interfaces of config */
  1,                  /* bConfigurationValue: identifier for this config */
  0,                  /* iConfiguration: index of string descriptor */
  0,                  /* bmAttributes: self/bus powered and remote wakeup */
  98,                 /* MaxPower: bus power required [mA/2] */
  /* interface descriptor */
  9,                  /* bLength: descriptor size in bytes */
  DESC_INTERFACE,     /* bDescriptorType */
  0,                  /* bInterfaceNumber: identifier for this interface */
  0,                  /* bAlternateSetting: disting. mutually exclusive IFs */
  1,                  /* bNumEndpoints: endpoints in addition to EP0 */
  0x03,               /* bInterfaceClass */
  0,                  /* bInterfaceSubclass */
  0,                  /* bInterfaceProtocol */
  0,                  /* iInterface: index of string descriptor */
  /* TODO: HID descriptor */
  /* endpoint descriptor */
  7,                  /* bLength: descriptor size in bytes */
  DESC_ENDPOINT,      /* bDescriptorType */
  0x81,               /* bEndpointAddress: endpoint number and direction */
  0x03,               /* bmAttributes: type of supported transfer */
  0x08,               /* wMaxPacketSize: max. packet size supported */
  0x01                /* bInterval: maximum latency for polling */  
};

/* USB Memory */
#pragma udata usb_bdt = 0x400
static volatile struct bd_entry BD0OUT;  /* buffer descriptor table */
static volatile struct bd_entry BD0IN;
static volatile struct bd_entry BD1OUT;
static volatile struct bd_entry BD1IN;
#pragma udata usb_mem = 0x480
static volatile unsigned char   EP0RXBUF[ 8 ];  /* 8 byte buffer */
static volatile unsigned char   EP0TXBUF[ 8 ];  /* 8 byte buffer */
static volatile unsigned char   EP1RXBUF[ 8 ];  /* 8 byte buffer */
static volatile unsigned char   EP1TXBUF[ 8 ];  /* 8 byte buffer */
#pragma udata

/* static data */
static enum trf_type   g_curtrf;  /* indicates type of current transfer */
static unsigned char * g_curtrf_data;  /* data pointer for next transact. */
static unsigned char   g_curtrf_left;  /* number of bytes still to transf */
static unsigned char   g_curtrf_dts;   /* DTS value for next transaction */
static unsigned char   g_addr;  /* TODO: rework */
static unsigned char   g_test;

/* local prototypes */
void high_isr( void );
void process_ep0( void );

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
  
  /* TODO: check enables flags here? */
  if ( PIR2 & 0x20 )
  {
    /* USB interrupt */
    if ( UIR & 0x01 )
    {
      /* USB reset interrupt */
      /* UADDR has already been set to 0 */
      g_addr = 0;
      /* TODO: clear USTAT FIFO */
      /* EP0 is ready for SETUP transaction: */
      BD0OUT.BDSTAT = _UOWN;
      BD0OUT.BDCNT  = 8;
      DEBUG_OUT( 'R' );
    }
    if ( UIR & 0x02 )
    {
      /* USB error condition interrupt */
      /* error condition flags may be queried here */
      UEIR = 0x00;  /* clear USB error interrupt flags */
    }
    if ( UIR & 0x08 )
    {
      /* USB transaction complete interrupt */
      /* read USTAT register */
      if ( ( USTAT & 0x78 ) == 0U )
      {
        process_ep0();  /* process endpoint 0 */
      }
          
    }
    UIR = 0x00;  /* clear USB interrupt flags */
  }
  
  /* other interrupt flags may be queried here */
  
  /* clear interrupt flag bits */
  PIR1 = 0x00;
  PIR2 = 0x00;
}


void process_ep0( void )
{
  unsigned char  req;       /* bRequest field */
  unsigned char  desc;      /* descriptor type requested */
  unsigned char  tocopy;    /* amount of data to copy */
  unsigned short requested; /* number of bytes requested by the host */
  unsigned char  i;
  
  /* find out which BD caused interrupt */
  if ( ( USTAT & _DIR ) == 0U )
  {
    /* last transaction was OUT or SETUP transaction */
    if ( ( BD0OUT.BDSTAT & 0x3C ) == PID_SETUP )
    {
      /* received SETUP transaction */
      DEBUG_OUT( 'S' );
      g_curtrf = TRF_NONE;   /* abort any transfer currently running */
      g_curtrf_dts = _DTS;   /* next transaction must be DATA1 */
      
      req = ((struct ctrltrf_setup *)EP0RXBUF)->bRequest;
      switch ( req )
      {
        case REQ_GET_DESCRIPTOR:
          desc = ((struct ctrltrf_setup *)EP0RXBUF)->wValue >> 8;
          switch ( desc )
          {
            case DESC_DEVICE:
              DEBUG_OUT( 'd' );
              g_curtrf = TRF_IN;
              g_curtrf_data = (unsigned char*)&dev_desc;
              g_curtrf_left = sizeof( dev_desc );
              requested = ((struct ctrltrf_setup *)EP0RXBUF)->wLength ;
              if ( requested < g_curtrf_left )
              {
                g_curtrf_left = requested;
              }
              /* endpoint for IN transaction will be prepared below */
              break;
            case DESC_CONFIGURATION:
              DEBUG_OUT( 'c' );
              g_curtrf = TRF_IN;
              g_curtrf_data = (unsigned char*)&cfg_desc;
              g_curtrf_left = sizeof( cfg_desc );
              requested = ((struct ctrltrf_setup *)EP0RXBUF)->wLength ;
              if ( requested < g_curtrf_left )
              {
                g_curtrf_left = requested;
              }
              /* endpoint for IN transaction will be prepared below */
              break;
            default:
              /* unsupported descriptor -> send STALL */
              /* (will be cleared with next SETUP transaction) */
              DEBUG_OUT( 'u' );
              BD0OUT.BDSTAT = _UOWN | _BSTALL;
              BD0IN.BDSTAT = _UOWN | _BSTALL;
          }
          break;
        case REQ_SET_DESCRIPTOR:
          /* TODO: not completely implemented yet */
          BD0OUT.BDSTAT = _UOWN | _BSTALL;
          BD0IN.BDSTAT = _UOWN | _BSTALL;
          /*
          g_curtrf = TRF_OUT;
          g_curtrf_data = &buffermem;
          g_curtrf_left = sizeof( buffermem );
          */
          break;
        case REQ_SET_ADDRESS:
          DEBUG_OUT( 'a' );
          g_curtrf = TRF_OUT;
          g_curtrf_left = 0;
          g_addr = ((struct ctrltrf_setup *)EP0RXBUF)->wValue & 0x7F;
          break;
        case REQ_SET_CONFIGURATION:
          /* the lower byte of wValue contains configuration number */
          DEBUG_OUT( 's' );
          g_curtrf = TRF_OUT;
          g_curtrf_left = 0;
          break;
        default:
          /* unsupported request -> send STALL */
          /* (will be cleared with next SETUP transaction) */
          DEBUG_OUT( 'U' );
          BD0OUT.BDSTAT = _UOWN | _BSTALL;
          BD0IN.BDSTAT = _UOWN | _BSTALL;
      }
      
      /* clear PKTDIS because it is set after each SETUP transaction */
      UCON &= ~_PKTDIS;
      
    } /* if ( pid == PID_SETUP ) */
    else
    {
      /* received OUT transaction */
      
      DEBUG_OUT( 'O' );
      if ( g_curtrf == TRF_IN )
      {
        /* OUT transaction from host means Status stage */
        /* -> transfer is complete */
        g_curtrf = TRF_NONE;
      }
      else if ( g_curtrf == TRF_OUT )
      {
        /* copy received data to memory */
        tocopy = BD0OUT.BDCNT;
        for ( i = 0; i < tocopy; ++i )
        {
          g_curtrf_data[ i ] = EP0RXBUF[ i ];
        }
        g_curtrf_data += tocopy;
        g_curtrf_dts ^= _DTS;       /* toggle DTS bit */
      }
    } /* if ( pid != PID_SETUP ) */
    
  } /* if ( ( USTAT & _DIR ) == 0 ) */
  else
  {
    /* last transaction was IN transaction */
    
    DEBUG_OUT( 'I' );
    if ( g_curtrf == TRF_IN )
    {
      /* host received our data transaction */
      /* if we have more data to send, endpoint will be prepared below */
      g_curtrf_dts ^= _DTS;     /* toggle DTS bit */
    }
    else if ( g_curtrf == TRF_OUT )
    {
      /* IN transaction from host means Status stage */
      /* host sent acknowledge -> transfer complete */
      g_curtrf = TRF_NONE;
      if ( g_addr != 0U )
      {
        /* we received our new address in this transaction! */
        DEBUG_OUT( '0' + ( g_addr >> 4 ) );
        DEBUG_OUT( '0' + ( g_addr & 0x0F ) );
        UADDR = g_addr;
        g_addr = 0;
        LATA = 0;
      }
    }
  } /* if ( ( USTAT & _DIR ) != 0 ) */

 
  /* prepare next transaction */
  if ( g_curtrf == TRF_IN )
  {
    /* transaction is IN, prepare next IN transaction */
    /* NOTE: We send packet regardless of whether there is still data
       remaining or not. When the host requests more data than we have,
       we are required to send a zero-length data packet. */
    tocopy = ( g_curtrf_left <= 8U ) ? g_curtrf_left : 8;
    for ( i = 0; i < tocopy; ++i )
    {
      EP0TXBUF[ i ] = g_curtrf_data[ i ];
    }
    g_curtrf_left -= tocopy;
    g_curtrf_data += tocopy;
    BD0IN.BDCNT  = tocopy;
    BD0IN.BDSTAT = _UOWN | _DTSEN | g_curtrf_dts;
    
    /* also prepare RX buffer, for receiving Status transaction */
    BD0OUT.BDCNT  = 0;    /* Status transaction has empty data packet */
    BD0OUT.BDSTAT = _UOWN | _DTSEN | _DTS;  /* Status is always DATA1 */
  }
  else if ( g_curtrf == TRF_OUT )
  {
    /* transaction is OUT, prepare RX buffer further OUT transactions */
    BD0OUT.BDCNT  = ( g_curtrf_left <= 8U ) ? g_curtrf_left : 8;
    BD0OUT.BDSTAT = _UOWN | _DTSEN | g_curtrf_dts;
    
    /* also prepare TX buffer, for sending Status transaction */
    BD0IN.BDCNT  = 0;       /* empty data packet */
    BD0IN.BDSTAT = _UOWN | _DTSEN | _DTS;   /* Status is always DATA1 */
  }
  else
  {
    /* transfer has been completed (g_curtrf = TRF_NONE) */
    /* prepare to receive next SETUP transaction */
    BD0OUT.BDCNT  = 8;
    BD0OUT.BDSTAT = _UOWN;
  }
}


void main(void)
{
  g_test = 0;
  ADCON1 = 0x0F; /* all pins to digital */
  LATA = 0x01; 
  TRISA = 0x00;  /* all pins to output */
  
  TRISB = 0xC0;
  TRISC = 0x00;

  /* initialize interrupts */
  /* IPEN in RCON is already 0 */
  PIE1 = 0x00;    /* disable most interrupt sources */
  PIE2 = 0x20;    /* enable USB interrupts */
  
  /* initialize EUSART */
  debug_init();
  DEBUG_OUT( '\r' );
  DEBUG_OUT( '\n' );

  /* initialize USB */
  UCFG = 0x00;  /* low speed, internal transciever */
  UIE  = 0x7F;  /* enable all USB interrupts */
  /* TODO: only enable implemented interrupts */
  UEP0 = _EPHSHK | _EPOUTEN | _EPINEN;  /* permit control transfers */
  //UEP1 = 0x1E;  /* endpoint 1: IN+OUT transfers */
  BD0OUT.BDSTAT = _UOWN;  /* reset&activate */
  BD0OUT.BDCNT  = 8;     /* 8 Byte size for low-speed */
  BD0OUT.BDADR  = (unsigned short)&EP0RXBUF;
  BD0IN.BDSTAT  = 0x00;  /* reset */
  BD0IN.BDADR   = (unsigned short)&EP0TXBUF;
  BD0IN.BDCNT   = 0;
  BD1OUT.BDSTAT = 0x00;  /* reset */
  BD1OUT.BDCNT  = 8;     /* 8 Byte size for low-speed */
  BD1OUT.BDADR  = (unsigned short)&EP1RXBUF;
  BD1IN.BDSTAT  = 0x00;  /* reset */
  BD1IN.BDADR   = (unsigned short)&EP1TXBUF;
  BD1IN.BDCNT   = 0;
  UCON = _PPBRST | _PKTDIS | _USBEN;  /* enable USB module */

  INTCON = 0xC0;  /* global interrupt enable */
  
  
  while(1)
  {
//    LATA = 1;
//    LATA = 0;
  }//end while
}//end main
 