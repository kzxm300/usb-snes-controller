#ifndef USB_H
#define USB_H

/* initializes the USB module */
void usb_init( void );

/* an USB interrupt occurred */
void usb_interrupt( void );

#endif  /* defined USB_H */