/*
elf.h:
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


#ifndef __ELF_H__
#define __ELF_H__

typedef unsigned int elf32_addr;
typedef unsigned int elf32_word;
typedef signed int elf32_sword;
typedef unsigned short elf32_half;
typedef unsigned int elf32_off;

struct elf32_ehdr
{
	unsigned char e_ident[16];     		/* ħ���������Ϣ */
	elf32_half	e_type;                 /* Ŀ���ļ����� */
	elf32_half	e_machine;              /* Ӳ����ϵ:arm,x86������ */
	elf32_word	e_version;              /* Ŀ���ļ��汾 */
	elf32_addr	e_entry;                /* �������е�ַ */
	elf32_off	e_phoff;                /* ����ͷ��ƫ���� */
	elf32_off	e_shoff;                /* ��ͷ��ƫ���� */
	elf32_word	e_flags;                /* �������ض���־ */
	elf32_half	e_ehsize;               /* elfͷ������ */
	elf32_half	e_phentsize;            /* ����ͷ����һ����Ŀ�ĳ��� */
	elf32_half	e_phnum;                /* ����ͷ����Ŀ����  */
	elf32_half	e_shentsize;            /* ��ͷ����һ����Ŀ�ĳ��� */
	elf32_half	e_shnum;                /* ��ͷ����Ŀ���� */
	elf32_half	e_shstrndx;             /* ��ͷ���ַ������� */
};
struct elf32_phdr
{
	elf32_word	p_type;					/* ������ */
	elf32_off	p_offset;     	 		/* ��λ��������ļ���ʼ����ƫ���� */
	elf32_addr	p_vaddr;   				/* �����ڴ��еĵ�ַ */
	elf32_addr	p_paddr;   				/* �ε������ַ */
	elf32_word	p_filesz;				/* �����ļ��еĳ��� */
	elf32_word	p_memsz;				/* �����ڴ��еĳ��� */
	elf32_word	p_flags;				/* �εı�� */
	elf32_word	p_align;				/* �����ڴ��ж����� */
};

/*definition of elf class*/
#define ELFCLASSNONE	0
#define ELFCLASS32		1
#define ELFCLASS64		2
#define CHECK_ELF_CLASS(p)				((p)->e_ident[4])
#define CHECK_ELF_CLASS_ELFCLASS32(p)	(CHECK_ELF_CLASS(p)==ELFCLASS32)

/*definition of elf data*/
#define ELFDATANONE		0
#define ELFDATA2LSB		1
#define ELFDATA2MSB		2
#define CHECK_ELF_DATA(p)				((p)->e_ident[5])
#define CHECK_ELF_DATA_LSB(p)	(CHECK_ELF_DATA(p)==ELFDATA2LSB)

/*elf type*/
#define ET_NONE			0
#define ET_REL			1
#define ET_EXEC			2
#define ET_DYN			3
#define ET_CORE			4
#define ET_LOPROC		0xff00
#define ET_HIPROC		0xffff
#define CHECK_ELF_TYPE(p)			((p)->e_type)
#define CHECK_ELF_TYPE_EXEC(p)		(CHECK_ELF_TYPE(p)==ET_EXEC)

/*elf machine*/
#define EM_NONE			0
#define EM_M32			1
#define EM_SPARC		2
#define EM_386			3
#define EM_68k			4
#define EM_88k			5
#define EM_860			7
#define EM_MIPS			8
#define EM_ARM			40
#define CHECK_ELF_MACHINE(p)		((p)->e_machine)
#define CHECK_ELF_MACHINE_ARM(p)	(CHECK_ELF_MACHINE(p)==EM_ARM)

/*elf version*/
#define EV_NONE			0
#define EV_CURRENT		1
#define CHECK_ELF_VERSION(p)			((p)->e_ident[6])
#define CHECK_ELF_VERSION_CURRENT(p)	(CHECK_ELF_VERSION(p)==EV_CURRENT)

#define ELF_FILE_CHECK(hdr)	((((hdr)->e_ident[0])==0x7f)&&\
		(((hdr)->e_ident[1])=='E')&&\
		(((hdr)->e_ident[2])=='L')&&\
		(((hdr)->e_ident[3])=='F'))


#define PT_NULL 				0
#define PT_LOAD 				1
#define PT_DYNAMIC 				2
#define PT_INTERP 				3
#define PT_NOTE 				4
#define PT_SHLIB 				5
#define PT_PHDR 				6
#define PT_LOPROC				0x70000000
#define PT_HIPROC				0x7fffffff
#define CHECK_PT_TYPE(p)		((p)->p_type)
#define CHECK_PT_TYPE_LOAD(p)	(CHECK_PT_TYPE(p)==PT_LOAD)
#endif
