#include "storage.h"
#include "fs.h"
#include "string.h"
#include "elf.h"
#include "print.h"

// 测试mmu
void test_mmu(void)
{
	const char *p="test mmu\n";
	while(*p)
	{
		*(volatile unsigned int *)0xd0000020=*p++;
	};
}

// 测试printk
void test_printk(void)
{
	char *p="this is a string";
	char c='H';
	int d=-256;
	int k=0;
	printk("\ntest printk\n");
	printk("test string :::	%s\ntest char ::: %c\ntest digit ::: %d\ntest X ::: %x\ntest unsigned ::: %u\ntest zero ::: %d\n",p,c,d,d,d,k);
}

// 测试buddy算法
void test_buddy(void)
{
	printk("test buddy\n");
	char *p1,*p2,*p3,*p4;
	p1=(char *)get_free_pages(0,6);
	printk("the return address of get_free_pages %x\n",p1);
	p2=(char *)get_free_pages(0,6);
	printk("the return address of get_free_pages %x\n",p2);

	put_free_pages(p2,6);
	put_free_pages(p1,6);

	p3=(char *)get_free_pages(0,7);
	printk("the return address of get_free_pages %x\n",p3);
	p4=(char *)get_free_pages(0,7);
	printk("the return address of get_free_pages %x\n",p4);

	put_free_pages(p4,7);
	put_free_pages(p3,7);

	p1=(char *)get_free_pages(0,6);
	printk("the return address of get_free_pages %x\n",p1);
	p2=(char *)get_free_pages(0,6);
	printk("the return address of get_free_pages %x\n",p2);

	put_free_pages(p2,6);
	put_free_pages(p1,6);
}

void test_slab_kmalloc(void)
{
	printk("test slab & kmalloc\n");
	char *p1,*p2,*p3,*p4;
	p1 = (char *)kmalloc(127);
	printk("the first alloced address is %x\n",p1);
	p2 = (char *)kmalloc(124);
	printk("the second alloced address is %x\n",p2);
	kfree(p1);
	kfree(p2);
	p3 = (char *)kmalloc(119);
	printk("the third alloced address is %x\n",p3);
	p4 = (char *)kmalloc(512);
	printk("the forth alloced address is %x\n",p4);
}

void test_ramdisk()
{
	int i;
	char buf[128];

	storage[RAMDISK]->dout(storage[RAMDISK],buf,0,sizeof(buf));

	for(i=0;i<sizeof(buf);i++)
	{
		printk("%d ",buf[i]);
	}
	printk("\n");
}

void test_romfs()
{
	int i;
	struct inode *node;
	char buf[128];
	memset(buf,0,128);

	// 利用romfs的namei函数获得"/number.txt"的inode
	if((node=fs_type[ROMFS]->namei(fs_type[ROMFS],"abc/txt/number.txt")) == (void *)0)
	{
		printk("inode read eror\n");
	}

	// 利用romfs的get_daddr函数获得数据的实际地址，
	// 再利用romfs的存储设备的读函数将数据读出来
	printk("file size:%d\n",node->dsize);
	if(fs_type[ROMFS]->device->dout(fs_type[ROMFS]->device,buf,fs_type[ROMFS]->get_daddr(node),node->dsize))
	{
		printk("dout error\n");
	}

	for(i=0;i<sizeof(buf);i++)
	{
		printk("%c ",buf[i]);
	}

}

void test_exec()
{
	int i;
	struct inode *node;
	struct elf32_phdr *phdr;
	struct elf32_ehdr *ehdr;
	int pos,dpos;
	char *buf;


	// 分配1k
	if((buf=(char *)kmalloc(1024)) == (char *)0)
	{
		printk("get free pages error\n");
		goto HALT;
	}

	// 获得inode
	if((node=fs_type[ROMFS]->namei(fs_type[ROMFS],"app/main")) == (void *)0)
	{
		printk("inode read eror\n");
		goto HALT;
	}

	// 根据inode读数据
	if(fs_type[ROMFS]->device->dout(fs_type[ROMFS]->device,buf,fs_type[ROMFS]->get_daddr(node),node->dsize))
	{
		printk("dout error\n");
		goto HALT;
	}

	// 文件头
	ehdr=(struct elf32_ehdr *)buf;
	// program头
	phdr=(struct elf32_phdr *)((char *)buf+ehdr->e_phoff);

	// 根据program头的信息从segment中读数据或代码到目的内存
	for(i=0;i<ehdr->e_phnum;i++)
	{
		if(CHECK_PT_TYPE_LOAD(phdr))
		{
			// p_vaddr是目的
			// p_offset是源
			// p_filesz是长度
			if(fs_type[ROMFS]->device->dout(fs_type[ROMFS]->device,(char *)phdr->p_vaddr,fs_type[ROMFS]->get_daddr(node)+phdr->p_offset,phdr->p_filesz)<0){
				printk("dout error\n");
				goto HALT;
			}
			phdr++;
		}
	}

	// 跳转
	exec(ehdr->e_entry);

	HALT:
	while(1);
}

void test_process(void *p)
{
	while(1)
	{
		delay();
		printk("test process %dth\n",(int)p);
	}
}

void test_fork()
{
	int i;
	for(i=0; i<10; i++)
		do_fork(test_process,(void *)i);

	while(1)
	{
		delay();
		printk("this is the original process\n");
	};
}


