/*
 * serial.h
 *
 *  Created on: 2012-10-23
 *      Author: dong
 */

#ifndef SERIAL_H_
#define SERIAL_H_

void serial_init(void);
void serial_putc(const char c);
int serial_getc(void);
void serial_puts(const char *s);

#endif /* SERIAL_H_ */
