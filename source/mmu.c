/*
mmu.c:
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


/*mask for page table base addr*/
#define PAGE_TABLE_L1_BASE_ADDR_MASK	(0xffffc000)

#define VIRT_TO_PTE_L1_INDEX(addr)		(((addr)&0xfff00000)>>18)

#define PTE_L1_SECTION_NO_CACHE_AND_WB	(0x0<<2)
#define PTE_L1_SECTION_DOMAIN_DEFAULT	(0x0<<5)
#define PTE_ALL_AP_L1_SECTION_DEFAULT	(0x1<<10)

#define PTE_L1_SECTION_PADDR_BASE_MASK	(0xfff00000)
#define PTE_BITS_L1_SECTION				(0x2)

#define L1_PTR_BASE_ADDR				0x30700000			// ҳ�����ַ

#define PHYSICAL_MEM_ADDR				0x30000000			// �ڴ�ռ������ַ
#define VIRTUAL_MEM_ADDR				0x30000000			// �ڴ�ռ������ַ
#define MEM_MAP_SIZE					0x800000			// �ڴ�ռ��С��8M

#define PHYSICAL_IO_ADDR				0x48000000			// �Ĵ����ռ������ַ
#define VIRTUAL_IO_ADDR					0xc8000000			// �Ĵ����ռ������ַ
#define IO_MAP_SIZE						0x18000000			// �Ĵ����ռ��С��18M�㹻��

#define VIRTUAL_VECTOR_ADDR				0x0					// �ж������������ַ
#define PHYSICAL_VECTOR_ADDR			0x30000000			// �ж������������ַ

// ����mmu
void start_mmu(void)
{
	unsigned int ttb=L1_PTR_BASE_ADDR;

	asm (
		"mcr p15,0,%0,c2,c0,0\n"   						 // c2 = %0 = ttb ,����ҳ�����ַ
		"mvn r0,#0\n"                  
		"mcr p15,0,r0,c3,c0,0\n"    					 // c3 = r0 = 0xffffffff , ����1��
		"mov r0,#0x1\n"
		"mcr p15,0,r0,c1,c0,0\n"						 // c1 = 1 , ����mmu
		"mov r0,r0\n"					  				 // ��ռ���mmuǰ��ˮ���ϵ�ָ��
		"mov r0,r0\n"
		"mov r0,r0\n"
		:											  	 // �����б�(��෵�ظ�c)
		: "r" (ttb)										 // �����б�(c���ݸ����Ĳ���)
		:"r0"											 // �����������б�
	);
}

// ���������ַ����ҳ����
unsigned int gen_l1_pte(unsigned int paddr)
{
	return (paddr&PTE_L1_SECTION_PADDR_BASE_MASK)|\
										PTE_BITS_L1_SECTION;
}

// ����ҳ�����ַ�������ַ����ҳ�����ַ
unsigned int gen_l1_pte_addr(unsigned int baddr, unsigned int vaddr)
{
	return (baddr&PAGE_TABLE_L1_BASE_ADDR_MASK)|\
								VIRT_TO_PTE_L1_INDEX(vaddr);
}

void init_sys_mmu(void)
{
	unsigned int pte;
	unsigned int pte_addr;
	int j;

	/*
	 * ����ʹ�õ���һ��ҳ��ҳ��СΪ1M����һ��ҳ�������1M��
	 * pte��page table entry��ҳ����
	 */

	//	���ж�������ӳ�䵽0��ַ���������ܴ����ж�
	for(j=0;j<MEM_MAP_SIZE>>20;j++)
	{
		pte=gen_l1_pte(PHYSICAL_VECTOR_ADDR+(j<<20));			// ����ҳ����
		pte|=PTE_ALL_AP_L1_SECTION_DEFAULT;
		pte|=PTE_L1_SECTION_NO_CACHE_AND_WB;
		pte|=PTE_L1_SECTION_DOMAIN_DEFAULT;
		pte_addr=gen_l1_pte_addr(L1_PTR_BASE_ADDR,\
								VIRTUAL_VECTOR_ADDR+(j<<20));	// ҳ�����ַ
		*(volatile unsigned int *)pte_addr=pte;					// *ҳ�����ַ = ҳ����
	}

	// �����ڴ���صĶ�ҳ����
	for(j=0;j<MEM_MAP_SIZE>>20;j++)
	{
		pte=gen_l1_pte(PHYSICAL_MEM_ADDR+(j<<20));
		pte|=PTE_ALL_AP_L1_SECTION_DEFAULT;
		pte|=PTE_L1_SECTION_NO_CACHE_AND_WB;
		pte|=PTE_L1_SECTION_DOMAIN_DEFAULT;
		pte_addr=gen_l1_pte_addr(L1_PTR_BASE_ADDR,\
								VIRTUAL_MEM_ADDR+(j<<20));
		*(volatile unsigned int *)pte_addr=pte;
	}

	// ���üĴ������ڿռ�Ķ�ҳ����
	for(j=0;j<IO_MAP_SIZE>>20;j++){
		pte=gen_l1_pte(PHYSICAL_IO_ADDR+(j<<20));
		pte|=PTE_ALL_AP_L1_SECTION_DEFAULT;
		pte|=PTE_L1_SECTION_NO_CACHE_AND_WB;
		pte|=PTE_L1_SECTION_DOMAIN_DEFAULT;
		pte_addr=gen_l1_pte_addr(L1_PTR_BASE_ADDR,\
								VIRTUAL_IO_ADDR+(j<<20));
		*(volatile unsigned int *)pte_addr=pte;
	}

	start_mmu();
}

// ӳ��ramdisk
void remap_l1(unsigned int paddr,unsigned int vaddr,int size)
{
	unsigned int pte;
	unsigned int pte_addr;
	for(;size>0;size-=1<<20)
	{
		pte=gen_l1_pte(paddr);
		pte_addr=gen_l1_pte_addr(L1_PTR_BASE_ADDR,vaddr);
		*(volatile unsigned int *)pte_addr=pte;
	}
}
