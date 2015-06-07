/*
romfs.c:
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

#include "fs.h"
#include "storage.h"
#include "string.h"
#include "print.h"

#define NULL (void *)0

#define be32_to_le32(x) \
		((unsigned int)( \
				(((unsigned int)(x) & (unsigned int)0x000000ffUL) << 24) | \
				(((unsigned int)(x) & (unsigned int)0x0000ff00UL) <<  8) | \
				(((unsigned int)(x) & (unsigned int)0x00ff0000UL) >>  8) | \
				(((unsigned int)(x) & (unsigned int)0xff000000UL) >> 24) ))

// romfs文件系统头
struct romfs_super_block
{
	unsigned int word0;
	unsigned int word1;
	unsigned int size;
	unsigned int checksum;
	char name[0];
};

// romfs文件头
struct romfs_inode
{
	unsigned int next;
	unsigned int spec;
	unsigned int size;
	unsigned int checksum;
	char name[0];
};

struct super_block romfs_super_block;

#define ROMFS_MAX_FILE_NAME	(128)
#define ROMFS_NAME_ALIGN_SIZE	(16)						// 16字节对齐
#define ROMFS_SUPER_UP_MARGIN	(16)						// superblock除去卷名所占的空间
#define ROMFS_NAME_MASK	(~(ROMFS_NAME_ALIGN_SIZE-1))		// ~(0xf) = 0x1111 1111 1111 0000
#define ROMFS_NEXT_MASK 0xfffffff0


// 获得磁盘中存储的第一个文件的位置:name + superblock的偏移16+对齐16
// <<24是将数据转化为大端
#define romfs_get_first_file_header(p)	\
		( ( ( (strlen(((struct romfs_inode *)(p))->name)+ROMFS_NAME_ALIGN_SIZE+ROMFS_SUPER_UP_MARGIN) ) & ROMFS_NAME_MASK ) << 24 )

// 获得文件中数据的实际地址
#define romfs_get_file_data_offset(p,num)	\
		( ( (((num)+ROMFS_NAME_ALIGN_SIZE)&ROMFS_NAME_MASK)+ROMFS_SUPER_UP_MARGIN+(p) ) )


// 去掉dir的顶层目录，保存在tmp中
static char *bmap(char *tmp,char *dir)
{
	unsigned int n;
	// 返回首次出现c的位置的指针，如果s中不存在c则返回NULL
	char *p=strchr(dir,'/');

	if(!p)
	{
		strcpy(tmp,dir);
		return NULL;
	}

	n = p-dir;
	n = (n>ROMFS_MAX_FILE_NAME)?ROMFS_MAX_FILE_NAME:n;
	strncpy(tmp,dir,n);
	return p+1;
}

static char *get_the_file_name(char *p,char *name)
{
	char *tmp=p;
	int index;
	for(index=0;*tmp;tmp++)
	{
		if(*tmp=='/')
		{
			index=0;
			continue;
		}
		else
		{
			name[index]=*tmp;
			index++;
		}
	}
	name[index]='\0';
	return name;
}

// namei函数根据文件系统类型和文件完整名获得文件对应的inode
struct inode *simple_romfs_namei(struct super_block *block,char *dir)
{
	struct inode *inode;
	struct romfs_inode *p;
	unsigned int tmp,next,num;
	char name[ROMFS_MAX_FILE_NAME],fname[ROMFS_MAX_FILE_NAME];

	memset(name,0,ROMFS_MAX_FILE_NAME);
	memset(fname,0,ROMFS_MAX_FILE_NAME);

	// romfs文件头的最大值
	unsigned int max_p_size=(ROMFS_MAX_FILE_NAME+sizeof(struct romfs_inode));
	max_p_size=max_p_size>(block->device->sector_size)? max_p_size : (block->device->sector_size);


	//将不含路径的文件名保存在fname中
	get_the_file_name(dir,fname);
	printk("\ntarget file name:%s\n",fname);

	// 给romfs文件头分配空间
	if((p=(struct romfs_inode *)kmalloc(max_p_size))==NULL)
	{
		goto ERR_OUT_NULL;
	}


	// 去掉dir的顶层目录，保存在name中
	dir=bmap(name,dir);
	printk("top folder:%s\n",name);
	if(dir)
	{
		printk("second path:%s\n",dir);
	}


	// 从存储设备的0地址处读1sector的数据到p中,我们的存储设备就是从物理地址0x30800000开始的2M ram
	if(block->device->dout(block->device,p,0,block->device->sector_size))
		goto ERR_OUT_KMALLOC;

	// 获得第一个文件的地址
	next=romfs_get_first_file_header(p);


	// 第一个循环用来找目标文件的父目录
	while(1)
	{
		// 大端转换成小端，并且16对齐
		tmp=(be32_to_le32(next))&ROMFS_NEXT_MASK;
		printk("current addr(little end):%d\n",(int)tmp);

		if(tmp>=block->device->storage_size)
			goto ERR_OUT_KMALLOC;

		if(tmp!=0)
		{
			// 从存储设备的tmp地址处读1sector的数据到p中
			if(block->device->dout(block->device,p,tmp,block->device->sector_size))
			{
				goto ERR_OUT_KMALLOC;
			}
			printk("current folder & top folder:%s & %s\n",p->name,name);

			// 将路径一层层的剥开,name是目标文件的顶层目录，p->name是当前目录
			if(!strcmp(p->name,name))
			{
				// fname是目标文件
				if(!strcmp(name,fname))
				{
					goto FOUND;
				}
				else
				{
					// 剥掉，直到找到父目录
					dir=bmap(name,dir);
					// 下一级目录的地址存放在spec ?
					next=p->spec;	
					if(dir==NULL)
					{
						goto FOUNDDIR;
					}
				}
			}
			// 如果目录名不相等，则寻找下一个目录
			else
			{
				next=p->next;	
			}
		}
		else
		{
			goto ERR_OUT_KMALLOC;
		}
	}



	// 第二个循环在父目录下找目标文件
	FOUNDDIR:
	printk("FOUNDDIR\n");
	while(1)
	{
		tmp=(be32_to_le32(next))&ROMFS_NEXT_MASK;
		printk("current addr(little end):%d\n",(int)tmp);

		if(tmp!=0)
		{
			if(block->device->dout(block->device,p,tmp,block->device->sector_size))
			{
				goto ERR_OUT_KMALLOC;
			}

			printk("current file & target file:%s & %s\n",p->name,name);
			if(!strcmp(p->name,name))
			{
				goto FOUND;
			}
			else
			{
				next=p->next;
			}
		}
		else
		{
			goto ERR_OUT_KMALLOC;
		}
	}


	FOUND:
	printk("FOUND\n");
	// 分配inode
	if((inode = (struct inode *)kmalloc(sizeof(struct inode)))==NULL)
	{
		goto ERR_OUT_KMALLOC;
	}

	num=strlen(p->name);				
	if((inode->name=(char *)kmalloc(num,0))==NULL)
	{
		goto ERR_OUT_KMEM_CACHE_ALLOC;
	}

	// 设置inode
	strcpy(inode->name,p->name);			// 文件名
	inode->dsize=be32_to_le32(p->size);		// 数据大小
	inode->daddr=tmp;						// 文件所在的地址
	inode->super=&romfs_super_block;		// 文件系统类型
	kfree(p);

	// 返回inode
	return inode;

	ERR_OUT_KMEM_CACHE_ALLOC:
	kfree(inode);

	ERR_OUT_KMALLOC:
	kfree(p);

	ERR_OUT_NULL:
	return NULL;
}

// 获得文件中数据的实际地址
unsigned int romfs_get_daddr(struct inode *node)
{
	int name_size=strlen(node->name);
	return romfs_get_file_data_offset(node->daddr,name_size);
}

// 一个super_block代表一种文件系统
struct super_block romfs_super_block =
{
		.namei	   = simple_romfs_namei,			// namei函数可以根据文件系统类型和文件完整名获得文件对应的inode
		.get_daddr = romfs_get_daddr,				// get_addr函数可以根据inode获得实际数据的位置
		.name      = "romfs",						// 文件系统的名字
};


int romfs_init(void)
{
	int ret;
	extern struct storage_device *storage[MAX_STORAGE_DEVICE];

	// 设置super_block
	romfs_super_block.device=storage[RAMDISK];
	ret=register_file_system(&romfs_super_block,ROMFS);

	return ret;
}
