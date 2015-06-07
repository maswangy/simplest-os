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

// romfs�ļ�ϵͳͷ
struct romfs_super_block
{
	unsigned int word0;
	unsigned int word1;
	unsigned int size;
	unsigned int checksum;
	char name[0];
};

// romfs�ļ�ͷ
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
#define ROMFS_NAME_ALIGN_SIZE	(16)						// 16�ֽڶ���
#define ROMFS_SUPER_UP_MARGIN	(16)						// superblock��ȥ������ռ�Ŀռ�
#define ROMFS_NAME_MASK	(~(ROMFS_NAME_ALIGN_SIZE-1))		// ~(0xf) = 0x1111 1111 1111 0000
#define ROMFS_NEXT_MASK 0xfffffff0


// ��ô����д洢�ĵ�һ���ļ���λ��:name + superblock��ƫ��16+����16
// <<24�ǽ�����ת��Ϊ���
#define romfs_get_first_file_header(p)	\
		( ( ( (strlen(((struct romfs_inode *)(p))->name)+ROMFS_NAME_ALIGN_SIZE+ROMFS_SUPER_UP_MARGIN) ) & ROMFS_NAME_MASK ) << 24 )

// ����ļ������ݵ�ʵ�ʵ�ַ
#define romfs_get_file_data_offset(p,num)	\
		( ( (((num)+ROMFS_NAME_ALIGN_SIZE)&ROMFS_NAME_MASK)+ROMFS_SUPER_UP_MARGIN+(p) ) )


// ȥ��dir�Ķ���Ŀ¼��������tmp��
static char *bmap(char *tmp,char *dir)
{
	unsigned int n;
	// �����״γ���c��λ�õ�ָ�룬���s�в�����c�򷵻�NULL
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

// namei���������ļ�ϵͳ���ͺ��ļ�����������ļ���Ӧ��inode
struct inode *simple_romfs_namei(struct super_block *block,char *dir)
{
	struct inode *inode;
	struct romfs_inode *p;
	unsigned int tmp,next,num;
	char name[ROMFS_MAX_FILE_NAME],fname[ROMFS_MAX_FILE_NAME];

	memset(name,0,ROMFS_MAX_FILE_NAME);
	memset(fname,0,ROMFS_MAX_FILE_NAME);

	// romfs�ļ�ͷ�����ֵ
	unsigned int max_p_size=(ROMFS_MAX_FILE_NAME+sizeof(struct romfs_inode));
	max_p_size=max_p_size>(block->device->sector_size)? max_p_size : (block->device->sector_size);


	//������·�����ļ���������fname��
	get_the_file_name(dir,fname);
	printk("\ntarget file name:%s\n",fname);

	// ��romfs�ļ�ͷ����ռ�
	if((p=(struct romfs_inode *)kmalloc(max_p_size))==NULL)
	{
		goto ERR_OUT_NULL;
	}


	// ȥ��dir�Ķ���Ŀ¼��������name��
	dir=bmap(name,dir);
	printk("top folder:%s\n",name);
	if(dir)
	{
		printk("second path:%s\n",dir);
	}


	// �Ӵ洢�豸��0��ַ����1sector�����ݵ�p��,���ǵĴ洢�豸���Ǵ������ַ0x30800000��ʼ��2M ram
	if(block->device->dout(block->device,p,0,block->device->sector_size))
		goto ERR_OUT_KMALLOC;

	// ��õ�һ���ļ��ĵ�ַ
	next=romfs_get_first_file_header(p);


	// ��һ��ѭ��������Ŀ���ļ��ĸ�Ŀ¼
	while(1)
	{
		// ���ת����С�ˣ�����16����
		tmp=(be32_to_le32(next))&ROMFS_NEXT_MASK;
		printk("current addr(little end):%d\n",(int)tmp);

		if(tmp>=block->device->storage_size)
			goto ERR_OUT_KMALLOC;

		if(tmp!=0)
		{
			// �Ӵ洢�豸��tmp��ַ����1sector�����ݵ�p��
			if(block->device->dout(block->device,p,tmp,block->device->sector_size))
			{
				goto ERR_OUT_KMALLOC;
			}
			printk("current folder & top folder:%s & %s\n",p->name,name);

			// ��·��һ���İ���,name��Ŀ���ļ��Ķ���Ŀ¼��p->name�ǵ�ǰĿ¼
			if(!strcmp(p->name,name))
			{
				// fname��Ŀ���ļ�
				if(!strcmp(name,fname))
				{
					goto FOUND;
				}
				else
				{
					// ������ֱ���ҵ���Ŀ¼
					dir=bmap(name,dir);
					// ��һ��Ŀ¼�ĵ�ַ�����spec ?
					next=p->spec;	
					if(dir==NULL)
					{
						goto FOUNDDIR;
					}
				}
			}
			// ���Ŀ¼������ȣ���Ѱ����һ��Ŀ¼
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



	// �ڶ���ѭ���ڸ�Ŀ¼����Ŀ���ļ�
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
	// ����inode
	if((inode = (struct inode *)kmalloc(sizeof(struct inode)))==NULL)
	{
		goto ERR_OUT_KMALLOC;
	}

	num=strlen(p->name);				
	if((inode->name=(char *)kmalloc(num,0))==NULL)
	{
		goto ERR_OUT_KMEM_CACHE_ALLOC;
	}

	// ����inode
	strcpy(inode->name,p->name);			// �ļ���
	inode->dsize=be32_to_le32(p->size);		// ���ݴ�С
	inode->daddr=tmp;						// �ļ����ڵĵ�ַ
	inode->super=&romfs_super_block;		// �ļ�ϵͳ����
	kfree(p);

	// ����inode
	return inode;

	ERR_OUT_KMEM_CACHE_ALLOC:
	kfree(inode);

	ERR_OUT_KMALLOC:
	kfree(p);

	ERR_OUT_NULL:
	return NULL;
}

// ����ļ������ݵ�ʵ�ʵ�ַ
unsigned int romfs_get_daddr(struct inode *node)
{
	int name_size=strlen(node->name);
	return romfs_get_file_data_offset(node->daddr,name_size);
}

// һ��super_block����һ���ļ�ϵͳ
struct super_block romfs_super_block =
{
		.namei	   = simple_romfs_namei,			// namei�������Ը����ļ�ϵͳ���ͺ��ļ�����������ļ���Ӧ��inode
		.get_daddr = romfs_get_daddr,				// get_addr�������Ը���inode���ʵ�����ݵ�λ��
		.name      = "romfs",						// �ļ�ϵͳ������
};


int romfs_init(void)
{
	int ret;
	extern struct storage_device *storage[MAX_STORAGE_DEVICE];

	// ����super_block
	romfs_super_block.device=storage[RAMDISK];
	ret=register_file_system(&romfs_super_block,ROMFS);

	return ret;
}
