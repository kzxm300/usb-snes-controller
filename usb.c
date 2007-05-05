/* usb.c */

#include <p18cxxx.h>
#include <string.h>   /* for memcpy() */
#include "debug.h"

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
/* UIR and UIE register */
#define _SOFI     0x40
#define _STALLI   0x20
#define _IDLEI    0x10
#define _TRNI     0x08
#define _ACTVI    0x04
#define _UERRI    0x02
#define _URSTI    0x01

/* PID values in BDnSTAT register */
#define PID_OUT   (unsigned char)(0x1 << 2)
#define PID_IN    (unsigned char)(0x9 << 2)
#define PID_SETUP (unsigned char)(0xD << 2)

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
  REQ_SYNC_FRAME        = 0x0C,
  /* HID specific (see explanation below) */
  REQ_GET_REPORT        = 0x81,
  REQ_GET_IDLE          = 0x82,
  REQ_GET_PROTOCOL      = 0x83,
  REQ_SET_REPORT        = 0x89,
  REQ_SET_IDLE          = 0x8A,
  REQ_SET_PROTOCOL      = 0x8B
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

/* for specifying if a transfer is in/from RAM or ROM */
enum trf_mem
{
  TRF_RAM,
  TRF_ROM
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

static const rom unsigned char report_desc[60];  /* forward declaration */

 
static const rom unsigned char dev_desc[18] =
{
  sizeof( dev_desc ), /* bLength: descriptor size in bytes */
  DESC_DEVICE,        /* bDescriptorType */
  0x00, 0x02,         /* bcdUSB: USB spec release number */
  0x00,               /* bDeviceClass: class code */
  0x00,               /* bDeviceSubclass: subclass code */
  0x00,               /* bDeviceProtocol: protocol code */
  0x08,               /* bMaxPacketSize: max packet size for EP0 */
  0xD8, 0x04,         /* idVendor: vendor ID (0x04D8=Microchip) */
  0x01, 0x00,         /* idProduct: product ID */
  0x01, 0x00,         /* bcdDevice: device release number */
  0x01,               /* iManufacturer: index of string desc. */
  0x02,               /* iProduct: index of string desc. */
  0x03,               /* iSerialNumber: index of string desc. */
  0x01                /* bNumConfiguration: number of possible configs */
};

static const rom unsigned char cfg_desc[34] =
{
  /* configuration descriptor */
  9,                  /* bLength: descriptor size in bytes */
  DESC_CONFIGURATION, /* bDescriptorType */
  sizeof( cfg_desc ), 0, /* wTotalLength: size of all data for this config */
  1,                  /* bNumInterfaces: number of interfaces of config */
  1,                  /* bConfigurationValue: identifier for this config */
  0,                  /* iConfiguration: index of string descriptor */
  0,                  /* bmAttributes: self/bus powered and remote wakeup */
  15,                 /* MaxPower: bus power required [2*mA] */
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
  /* class descriptor */
  9,                  /* bLength: descriptor size in bytes */
  DESC_HID,           /* bDescriptorType */
  0x10, 0x01,         /* bcdHID: HID spec release number */
  0,                  /* bCountryCode: indentifies country for localized HW */
  1,                  /* bNumDescriptors: number of subordinate class desc. */
  DESC_REPORT,        /* bDescriptorType */
  sizeof( report_desc ), 0x00, /* wDescriptorLength: length of report desc. */
  /* endpoint descriptor */
  7,                  /* bLength: descriptor size in bytes */
  DESC_ENDPOINT,      /* bDescriptorType */
  0x81,               /* bEndpointAddress: endpoint number and direction */
  0x03,               /* bmAttributes: type of supported transfer */
  0x08, 0x00,         /* wMaxPacketSize: max. packet size supported */
  0x0A                /* bInterval: maximum latency for polling */  
};

static const rom unsigned char report_desc[60] =
{
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    //   COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0xff,                    //     LOGICAL_MINIMUM (-1)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x75, 0x02,                    //     REPORT_SIZE (2)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0xc0,                          // END_COLLECTION
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x04,                    //   REPORT_COUNT (4)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 0x06,                    //   USAGE_MAXIMUM (Button 6)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
    0x09, 0x3d,                    //   USAGE (Start)
    0x09, 0x3e,                    //   USAGE (akeup)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x02,                    //   REPORT_COUNT (2)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION
};

static const rom unsigned char string_desc_lang[4] =
{
  sizeof( string_desc_lang ),  /* bLength */
  DESC_STRING,                 /* bDescriptorType */
  0x09, 0x04                   /* wLANGID */
};

static const rom unsigned char string_desc_man[38] =
{
  sizeof( string_desc_man ),
  DESC_STRING,
  'C',0,'h',0,'r',0,'i',0,'s',0,'t',0,'i',0,'a',0,'n',0,' ',0,
  'E',0,'t',0,'t',0,'i',0,'n',0,'g',0,'e',0,'r',0
};

static const rom unsigned char string_desc_prod[52] =
{
  sizeof( string_desc_prod ),
  DESC_STRING,
  'S',0,'u',0,'p',0,'e',0,'r',0,' ',0,'N',0,'i',0,'n',0,'t',0,'e',0,'n',0,
  'd',0,'o',0,' ',0,'C',0,'o',0,'n',0,'t',0,'r',0,'o',0,'l',0,'l',0,'e',0,'r',0
};

static const rom unsigned char string_desc_serial[10] =
{
  sizeof( string_desc_serial ),
  DESC_STRING,
  '0',0,'0',0,'0',0,'1',0
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
static enum trf_mem    g_curtrf_mem;   /* whether data is in RAM or ROM */
static unsigned char * g_curtrf_data;  /* data pointer for next transact. */
static unsigned char   g_curtrf_left;  /* number of bytes still to transf */
static unsigned char   g_curtrf_dts;   /* DTS value for next transaction */
static unsigned char   g_addr;  /* TODO: rework */
static unsigned char   g_config;       /* current configuration */
static unsigned char   g_reportpending;/* if new report data is waiting */
static unsigned char   g_reportdts;    /* DTS value for next transaction */
unsigned char          g_hidreport[2]; /* HID report with button states */

/* local prototypes */
static void process_ep0( void );
static void process_ep1( void );

#pragma code


/* initialize USB module */
void usb_init( void )
{
  PIE2 |= 0x20;     /* enable USB interrupts */
  
  UCFG = 0x10;   /* low speed, internal transciever, on-chip pullup */
  UIE  = _IDLEI | _TRNI | _URSTI;       /* enable USB interrupts */
  UEP0 = _EPHSHK | _EPOUTEN | _EPINEN;  /* permit control transfers */
  UEP1 = _EPHSHK | _EPCONDIS | _EPINEN; /* only IN transfers */
  BD0OUT.BDSTAT = _UOWN; /* reset&activate */
  BD0OUT.BDCNT  = 8;     /* 8 Byte size for low-speed */
  BD0OUT.BDADR  = (unsigned short)&EP0RXBUF;
  BD0IN.BDSTAT  = 0x00;  /* reset */
  BD0IN.BDCNT   = 0;
  BD0IN.BDADR   = (unsigned short)&EP0TXBUF;
  BD1OUT.BDSTAT = 0x00;  /* reset */
  BD1OUT.BDCNT  = 8;     /* 8 Byte size for low-speed */
  BD1OUT.BDADR  = (unsigned short)&EP1RXBUF;
  BD1IN.BDSTAT  = 0x00;  /* reset */
  BD1IN.BDCNT   = 2;
  BD1IN.BDADR   = (unsigned short)&EP1TXBUF;
  EP1TXBUF[0] = 0;
  EP1TXBUF[1] = 0;
  UCON = _PPBRST | _PKTDIS | _USBEN;  /* enable USB module */
}

/* called whenever g_hidreport was changed */
/* NOTE: must not be called with interrupts disabled! */
void usb_reportchanged( void )
{
  INTCON &= ~0x80;  /* protection against interruption */
  
  /* check if we are allowed to modify this buffer */
  if ( ( BD1IN.BDSTAT & _UOWN ) == 0U )
  {
    /* prepare endpoint for next IN transaction */
    EP1TXBUF[0] = g_hidreport[0];
    EP1TXBUF[1] = g_hidreport[1];
    BD1IN.BDSTAT = _UOWN | _DTSEN | g_reportdts;
    g_reportdts ^= _DTS;
  }
  else
  {
    /* endpoint is busy right now */
    g_reportpending = 1;
  }
  
  INTCON |= 0x80;   /* enable interrupts again */
}


/* handle USB interrupt */
void usb_interrupt( void ) 
{
  if ( ( UIE & _URSTI ) && ( UIR & _URSTI ) )
  {
    /* USB reset interrupt */
    /* UADDR has already been set to 0 */
    g_addr          = 0;
    g_config        = 0;
    g_reportpending = 0;
    g_reportdts     = 0;
    /* EP0 is ready for SETUP transaction: */
    BD0OUT.BDSTAT = _UOWN;
    BD0OUT.BDCNT  = 8;
    DEBUG_OUT( 'R' );
    DEBUG_OUT( '\r' );
    DEBUG_OUT( '\n' );
    UIR = 0x00;         /* clear all other USB interrupts */
  }
  if ( ( UIE & _TRNI ) && ( UIR & _TRNI ) )
  {
    /* USB transaction complete interrupt */
    /* read USTAT register */
    switch ( USTAT & 0x78 )
    {
      case 0x00:
        process_ep0();  /* process endpoint 0 */
        break;
      case 0x08:
        process_ep1();  /* process endpoint 1 */
        break;
    }
  }
  if ( ( UIE & _UERRI ) && ( UIR & _UERRI ) )
  {
    /* USB error condition interrupt */
    /* (currently not enabled) */
    /* error condition flags may be queried here */
    UEIR = 0x00;  /* clear USB error interrupt flags */
  }
  if ( ( UIE & _IDLEI ) && ( UIR & _IDLEI ) )
  {
    /* idle condition detected */
    UCON |= _SUSPND;  /* place SIE in suspend state */
    UIR  = 0x00;      /* clear all interrupt conditions */
    PIR1 = 0x00;      /* (required for SLEEP mode) */
    PIR2 = 0x00;
    UIE = _ACTVI;     /* enable only ACTVIF interrupt */
    Sleep();          /* enter SLEEP state */
  }
  if ( ( UIE & _ACTVI ) && ( UIR & _ACTVI ) )
  {
    /* bus activity detected */
    UCON &= ~_SUSPND;   /* enable normal SIE operation again */
    UIE = _IDLEI | _TRNI | _URSTI;   /* enable USB interrupts again */
  }
  
  UIR = 0x00;  /* clear USB interrupt flags */
}


/* process interrupt at endpoint 0 */
static void process_ep0( void )
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
      if ( ( ((struct ctrltrf_setup *)EP0RXBUF)->bmRequestType & 0x60 ) == 0x20U )
      {
        /* Explanation: we "fold" bRequest and bmRequestType together into one
          value. By that we can use the same code for handling class-specific
          requests. */
        req |= 0x80;  /* bit 7 identifies this as class-specific request */
      }
      
      switch ( req )
      {
        case REQ_GET_DESCRIPTOR:
          DEBUG_OUT( 'D' );
          desc = ((struct ctrltrf_setup *)EP0RXBUF)->wValue >> 8;
          requested = ((struct ctrltrf_setup *)EP0RXBUF)->wLength ;
          DEBUG_OUT( '0' + ( requested >> 4 ) );
          DEBUG_OUT( '0' + ( requested & 0x0F ) );
          switch ( desc )
          {
            case DESC_DEVICE:
              DEBUG_OUT( 'd' );
              g_curtrf = TRF_IN;
              g_curtrf_data = dev_desc;
              g_curtrf_left = sizeof( dev_desc );
              /* endpoint for IN transaction will be prepared below */
              break;
            case DESC_CONFIGURATION:
              DEBUG_OUT( 'c' );
              g_curtrf = TRF_IN;
              g_curtrf_data = cfg_desc;
              g_curtrf_left = sizeof( cfg_desc );
              /* endpoint for IN transaction will be prepared below */
              break;
            case DESC_REPORT:
              DEBUG_OUT( 'r' );
              g_curtrf = TRF_IN;
              g_curtrf_data = report_desc;
              g_curtrf_left = sizeof( report_desc );
              /* endpoint for IN transaction will be prepared below */
              break;
            case DESC_STRING:
              DEBUG_OUT( 's' );
              g_curtrf = TRF_IN;
              switch ( ((struct ctrltrf_setup *)EP0RXBUF)->wValue & 0xFF )
              {
                case 0:
                  DEBUG_OUT( '0' );
                  g_curtrf_data = string_desc_lang;
                  g_curtrf_left = sizeof( string_desc_lang );
                  break;
                case 1:
                  DEBUG_OUT( '1' );
                  g_curtrf_data = string_desc_man;
                  g_curtrf_left = sizeof( string_desc_man );
                  break;
                case 2:
                  DEBUG_OUT( '2' );
                  g_curtrf_data = string_desc_prod;
                  g_curtrf_left = sizeof( string_desc_prod );
                  break;
                case 3:
                  DEBUG_OUT( '3' );
                  g_curtrf_data = string_desc_serial;
                  g_curtrf_left = sizeof( string_desc_serial );
                  break;
                default:
                  DEBUG_OUT( 'u' );
                  g_curtrf_left = 0;
              }
              /* endpoint for IN transaction will be prepared below */
              break;
            default:
              /* unsupported descriptor -> send STALL */
              /* (will be cleared with next SETUP transaction) */
              DEBUG_OUT( 'u' );
              DEBUG_OUT( desc );
              BD0OUT.BDSTAT = _UOWN | _BSTALL;
              BD0IN.BDSTAT = _UOWN | _BSTALL;
          }

          /* correct amount of bytes if necessary */
          if ( requested < g_curtrf_left )
          {
            g_curtrf_left = requested;
          }
          
          /* descriptors come from ROM */
          g_curtrf_mem = TRF_ROM;
          break;
/*        case REQ_SET_DESCRIPTOR:
          BD0OUT.BDSTAT = _UOWN | _BSTALL;
          BD0IN.BDSTAT = _UOWN | _BSTALL;
          /*
          g_curtrf = TRF_OUT;
          g_curtrf_mem = TRF_RAM;
          g_curtrf_data = &buffermem;
          g_curtrf_left = sizeof( buffermem );
          break;  */
        case REQ_SET_ADDRESS:
          DEBUG_OUT( 'A' );
          g_curtrf = TRF_OUT;
          g_curtrf_left = 0;
          g_addr = ((struct ctrltrf_setup *)EP0RXBUF)->wValue & 0x7F;
          break;
        case REQ_SET_CONFIGURATION:
          /* the lower byte of wValue contains configuration number */
          DEBUG_OUT( 'C' );
          DEBUG_OUT( 's' );
          g_curtrf = TRF_OUT;
          g_curtrf_left = 0;
          g_config = ((struct ctrltrf_setup *)EP0RXBUF)->wValue & 0xFF;
          break;
        case REQ_GET_CONFIGURATION:
          DEBUG_OUT( 'C' );
          DEBUG_OUT( 'g' );
          g_curtrf = TRF_IN;
          g_curtrf_mem = TRF_RAM;
          g_curtrf_data = &g_config;
          g_curtrf_left = 1;
          break;
        case REQ_GET_REPORT:
          DEBUG_OUT( 'P' );
          /* wValue: high-byte = report type, low-byte = report ID */
          /* we support only one report, therefore we need not check here */
          g_curtrf = TRF_IN;
          g_curtrf_mem = TRF_RAM;
          g_curtrf_data = g_hidreport;
          g_curtrf_left = 2;
          break;
        case REQ_SET_IDLE:
          DEBUG_OUT( 'L' );
          g_curtrf = TRF_OUT;
          g_curtrf_left = 0;
          break;
        default:
          /* unsupported request -> send STALL */
          /* (will be cleared with next SETUP transaction) */
          DEBUG_OUT( 'U' );
          DEBUG_OUT( req );
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
        memcpy( (void *)g_curtrf_data, (const void *)EP0RXBUF, tocopy );
        g_curtrf_data += tocopy;
        g_curtrf_left -= tocopy;
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
    //DEBUG_OUT( '0' + tocopy );
    if ( g_curtrf_mem == TRF_RAM )
    {
      memcpy( (void *)EP0TXBUF, (const void *)g_curtrf_data, tocopy );
    }
    else
    {
      /* copy from ROM */
      memcpypgm2ram( (void *)EP0TXBUF, (rom void *)g_curtrf_data, tocopy );
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


/* process interrupt at endpoint 1 */
static void process_ep1( void )
{
  /* endpoint 1 only supports interrupt IN transfers */
  /* therefore we need not check anything here */
  /* endpoint usually gets re-armed in usb_reportchanged(),
    except when endpoint was busy then */
  if ( g_reportpending != 0U )
  {
    DEBUG_OUT( '_' );
    /* there is new data waiting to be transmitted */
    EP1TXBUF[0] = g_hidreport[0];
    EP1TXBUF[1] = g_hidreport[1];
    BD1IN.BDSTAT = _UOWN | _DTSEN | g_reportdts;
    g_reportdts ^= _DTS;
    g_reportpending = 0;
  }
  else
  {
    DEBUG_OUT( '-' );
  }
}
