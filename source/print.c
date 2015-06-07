/*
mem.c:
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

typedef char * va_list;
#define _INTSIZEOF(n)   ((sizeof(n)+sizeof(int)-1)&~(sizeof(int) - 1) )
#define va_start(ap,v) ( ap = (va_list)&v + _INTSIZEOF(v) )
#define va_arg(ap,t) ( *(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)) )
#define va_end(ap)    ( ap = (va_list)0 )

const char *digits="0123456789abcdef";
char numbers[68];

static char print_buf[1024];

#define FORMAT_TYPE_MASK		0xff00
#define FORMAT_TYPE_SIGN_BIT	0x0100

#define FORMAT_TYPE_NONE	0x000
#define FORMAT_TYPE_CHAR	0x100
#define FORMAT_TYPE_UCHAR	0x200
#define FORMAT_TYPE_SHORT	0x300
#define FORMAT_TYPE_USHORT	0x400
#define FORMAT_TYPE_INT		0x500
#define FORMAT_TYPE_UINT	0x600
#define FORMAT_TYPE_LONG	0x700
#define FORMAT_TYPE_ULONG	0x800
#define FORMAT_TYPE_STR		0xd00
#define FORMAT_TYPE_PTR		0x900
#define FORMAT_TYPE_SIZE_T	0xb00

#define FORMAT_TYPE(x)	((x)&FORMAT_TYPE_MASK)
#define SET_FORMAT_TYPE(x,t)	do{\
		(x)&=~FORMAT_TYPE_MASK;(x)|=(t);\
}while(0)
#define FORMAT_SIGNED(x)	((x)&FORMAT_TYPE_SIGN_BIT)

#define FORMAT_FLAG_MASK	0xffff0000
#define FORMAT_FLAG_SPACE	0x10000
#define FORMAT_FLAG_ZEROPAD	0x20000
#define FORMAT_FLAG_LEFT	0x40000

#define FORMAT_FLAG(x)	((x)&FORMAT_FLAG_MASK)
#define SET_FORMAT_FLAG(x,f)	((x)|=(f))

#define FORMAT_BASE_MASK	0xff
#define FORMAT_BASE_O		0x08
#define FORMAT_BASE_X		0x10
#define FORMAT_BASE_D		0x0a
#define FORMAT_BASE_B		0x02

#define FORMAT_BASE(x)	(FORMAT_BASE_MASK&(x))
#define SET_FORMAT_BASE(x,t)	do{(x)&=~FORMAT_BASE_MASK;(x)|=(t);}while(0)

#define do_div(n,base) ({ \
		int __res; \
		__res = ((unsigned int) n) % (unsigned int) base; \
		n = ((unsigned int) n) / (unsigned int) base; \
		__res; })


// 串口输出字符串
void __put_char(char *p,int num)
{
	while(*p&&num--)
	{
		// *(volatile unsigned int *)0xd0000020=*p++;
		serial_putc(*p++);
	};
}

void* memcpy(void * dest,const void *src,unsigned int count)
{
	char *tmp = (char *) dest, *s = (char *) src;

	while (count--)
		*tmp++ = *s++;

	return dest;
}

char *number(char *str, int num,int base,unsigned int flags)
{
	int i=0;
	int sign=0;

	if(FORMAT_SIGNED(flags)&&(signed int)num<0)
	{
		sign=1;
		num=~num+1;
	}

	// 把数字转换为对应的字符，例如把123转化为'1' '2' '3'
	do
	{
		numbers[i++]=digits[do_div(num,base)];
	}while(num!=0);

	// 八进制
	if(FORMAT_BASE(flags)==FORMAT_BASE_O)
	{
		numbers[i++]='0';
	}
	// 十六进制
	else if(FORMAT_BASE(flags)==FORMAT_BASE_X)
	{
		numbers[i++]='x';
		numbers[i++]='0';
	}
	// 二进制
	else if(FORMAT_BASE(flags)==FORMAT_BASE_B)
	{
		numbers[i++]='b';
		numbers[i++]='0';
	}
	// 是否需要-
	if(sign)
		numbers[i++]='-';

	// 拷贝到print_buf
	while (i-- > 0)
		*str++ = numbers[i];

	return str;
}

int format_decode(const char *fmt,unsigned int *flags)
{
	const char *start = fmt;

	*flags &= ~FORMAT_TYPE_MASK;
	*flags |= FORMAT_TYPE_NONE;

	// 循环直到遇到%
	for (; *fmt ; ++fmt)
	{
		if (*fmt == '%')
			break;
	}

	// 如果是普通字符，则第一次调用format_decode时fmt!=start，需直接返回；
	// 再次调用formmat_decode时，fmt = start，再来处理%后面的特殊字符。
	if (fmt != start || !*fmt)
		return fmt - start;


	// 判断%或者%l后面的是不是c/s/o/x/X/d/i/u
	SET_FORMAT_BASE(*flags,FORMAT_BASE_D);
	fmt++;
	switch (*fmt)
	{
	case 'c':
		SET_FORMAT_TYPE(*flags,FORMAT_TYPE_CHAR);
		break;

	case 's':
		SET_FORMAT_TYPE(*flags,FORMAT_TYPE_STR);
		break;

	case 'o':
		SET_FORMAT_BASE(*flags,FORMAT_BASE_O);
		SET_FORMAT_TYPE(*flags,FORMAT_TYPE_UINT);
		break;

	case 'x':
	case 'X':
		SET_FORMAT_BASE(*flags,FORMAT_BASE_X);
		SET_FORMAT_TYPE(*flags,FORMAT_TYPE_UINT);
		break;

	case 'd':
	case 'i':
		SET_FORMAT_TYPE(*flags,FORMAT_TYPE_INT);
		SET_FORMAT_BASE(*flags,FORMAT_BASE_D);
		break;
	case 'u':
		SET_FORMAT_TYPE(*flags,FORMAT_TYPE_UINT);
		SET_FORMAT_BASE(*flags,FORMAT_BASE_D);
		break;

	default:
		break;
	}
	return (++fmt)-start;
}


int vsnprintf(char *buf, int size, const char *fmt, va_list args)
{
	int num;
	char *str, *end, c,*s;
	int read;
	unsigned int spec=0;

	str = buf;								// print_buf的头
	end = buf + size;						// print_buf的尾

	if (end < buf)
	{
		end = ((void *)-1);
		size = end - buf;
	}

	while (*fmt)
	{
		const char *old_fmt = fmt;

		read = format_decode(fmt, &spec);
		fmt += read;

		// 普通字符
		if((FORMAT_TYPE(spec))==FORMAT_TYPE_NONE)
		{
			int copy = read;
			if (str < end)
			{
				if (copy > end - str)
					copy = end - str;
				// 将old_fmt中的普通字符拷贝到printf_buf中
				memcpy(str, old_fmt, copy);
			}
			// printf_buf += 普通字符个数
			str += read;

		}
		// 转义字符：
		// %c
		else if(FORMAT_TYPE(spec)==FORMAT_TYPE_CHAR)
		{
			c = (unsigned char) va_arg(args, unsigned char);
			if (str < end)
				*str = c;
			++str;
		}
		// %s
		else if(FORMAT_TYPE(spec)==FORMAT_TYPE_STR)
		{
			s = (char *) va_arg(args, char *);
			while(str<end&&*s!='\0')
			{
				*str++=*s++;
			}
		}
		// 数字
		else
		{
			// %d
			if(FORMAT_TYPE(spec)==FORMAT_TYPE_INT)
			{
				num = (int) va_arg(args, int);
			}
			// %u
			else if(FORMAT_TYPE(spec)==FORMAT_TYPE_ULONG)
			{
				num = (unsigned long) va_arg(args, unsigned long);
			}
			else if(FORMAT_TYPE(spec)==FORMAT_TYPE_LONG)
			{
				num = (long) va_arg(args, long);
			}
			else if(FORMAT_TYPE(spec)==FORMAT_TYPE_SIZE_T)
			{
				num = (int) va_arg(args, int);
			}
			else if(FORMAT_TYPE(spec)==FORMAT_TYPE_USHORT)
			{
				num = (unsigned short) va_arg(args, int);
			}
			else if(FORMAT_TYPE(spec)==FORMAT_TYPE_SHORT)
			{
				num = (short) va_arg(args, int);
			}
			else
			{
				num = va_arg(args, unsigned int);
			}

			str=number(str,num,spec&FORMAT_BASE_MASK,spec);
		}
	}
	if (size > 0)
	{
		if (str < end)
			*str = '\0';
		else
			end[-1] = '\0';
	}

	// 返回需打印的字符个数
	return str-buf;
}

// 打印函数
void printk(const char *fmt, ...)
{
	va_list args;
	unsigned int i;

	va_start (args, fmt);										// 获得变参的起始地址
	i = vsnprintf (print_buf, sizeof(print_buf),fmt, args);		// 处理变参
	va_end (args);

	__put_char (print_buf,i);									// 输出字符串print_buf
}
