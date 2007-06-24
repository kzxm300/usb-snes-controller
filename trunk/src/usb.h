#ifndef USB_H
#define USB_H

/* initializes the USB module */
void usb_init( void );

/* an USB interrupt occurred */
void usb_interrupt( void );

/* HID report data has been changed */
void usb_reportchanged( void );

/* HID report containing which button is pressed */
extern unsigned char g_hidreport[2]; 

#endif  /* defined USB_H */