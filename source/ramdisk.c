/*
ramdisk.c:
Copyright (C) 2009  david leels <davidontech@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include "storage.h"

#define RAMDISK_SECTOR_SIZE		512
#define RAMDISK_SECTOR_MASK	(~(RAMDISK_SECTOR_SIZE-1))
#define RAMDISK_SECTOR_OFFSET	((RAMDISK_SECTOR_SIZE-1))

extern void *memcpy(void * dest,const void *src,unsigned int count);

// ramdisk������
int ramdisk_dout(struct storage_device *sd,void *dest,unsigned int addr,size_t size)
{

	memcpy(dest,(char *)(addr+sd->start_pos),size);

	return 0;
}

// ramdiskд����
int ramdisk_din(struct storage_device *sd,void *dest,unsigned int addr,size_t size)
{

	memcpy((char *)(addr+sd->start_pos),dest,size);

	return 0;
}

// ramdisk�豸�ṹ��
struct storage_device ramdisk_storage_device=
{
	.dout=ramdisk_dout,
	.din=ramdisk_din,
	.sector_size=RAMDISK_SECTOR_SIZE,
	.storage_size=2*1024*1024,
	.start_pos=0x40800000,
};

// ramdisk��ʼ������
int ramdisk_driver_init(void)
{
	int ret;
	remap_l1(0x30800000,0x40800000,2*1024*1024);					// pa��0x30800000��va��0x40800000��2M
	ret=register_storage_device(&ramdisk_storage_device,RAMDISK);	// ע�� storage_device
	return ret;
}


