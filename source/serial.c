#include "s3c2440.h"

#define UART_NR	0

void serial_setbrg(unsigned int baudrate)
{
	S3C24X0_UART * const uart = S3C24X0_GetBase_UART(UART_NR);
	int i;
	unsigned int reg = 0;
	
	/* value is calculated so : (int)(PCLK/16./baudrate) -1
 	 * reg = get_PCLK() / (16 * baudrate) - 1;
 	 */
	
	/* PCLK is 50625000 */
	/* baudrate is 115200 */
	reg = 0x1A; 
	
	/* FIFO enable, Tx/Rx FIFO clear */
	uart->UFCON = 0x07;
	uart->UMCON = 0x0;
	/* Normal,No parity,1 stop,8 bit */
	uart->ULCON = 0x3;
	/*
	 * tx=level,rx=edge,disable timeout int.,enable rx error int.,
	 * normal,interrupt or polling
	 */
	uart->UCON = 0x245;
	uart->UBRDIV = reg;

	for (i = 0; i < 100; i++);
}

/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, no start bits.
 *
 */
void serial_init(void)
{
	serial_setbrg(115200);
}

/*
 * Read a single byte from the serial port. Returns 1 on success, 0
 * otherwise. When the function is succesfull, the character read is
 * written into its argument c.
 */
int serial_getc(void)
{
	S3C24X0_UART * const uart = S3C24X0_GetBase_UART(UART_NR);

	/* wait for character to arrive */
	while (!(uart->UTRSTAT & 0x1));

	return uart->URXH & 0xff;
}

/*
 * Output a single byte to the serial port.
 */
void serial_putc(const char c)
{
	S3C24X0_UART * const uart = S3C24X0_GetBase_UART(UART_NR);

	/* wait for room in the tx FIFO */
	while (!(uart->UTRSTAT & 0x2));

	uart->UTXH = c;

	/* If \n, also do \r */
	if (c == '\n')
		serial_putc ('\r');
}

/*
 * Test whether a character is in the RX buffer
 */
int serial_tstc(void)
{
	S3C24X0_UART * const uart = S3C24X0_GetBase_UART(UART_NR);

	return uart->UTRSTAT & 0x1;
}

void serial_puts(const char *s)
{
	while (*s) {
		serial_putc(*s++);
	}
}
