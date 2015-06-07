/*
 * interrupt.h
 *
 *  Created on: 2012-10-20
 *      Author: dong
 */

#ifndef INTERRUPT_H_
#define INTERRUPT_H_

void enable_irq(void);
void disable_irq(void);
void umask_int(unsigned int offset);
void common_irq_handler(void);
void interrupt_init();


#endif /* INTERRUPT_H_ */
