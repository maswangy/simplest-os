/*
 * test.h
 *
 *  Created on: 2012-10-19
 *      Author: dong
 */

#ifndef TEST_H_
#define TEST_H_


void test_mmu(void);
void test_printk(void);
void test_buddy(void);
void test_slab_kmalloc(void);
void test_process(void *p);
void test_serial(void);
void test_ramdisk();
void test_romfs();
void test_exec();
void test_fork();

#endif /* TEST_H_ */
