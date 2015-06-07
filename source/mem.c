#include "list.h"

#define	NULL	((void *)0)

#define _MEM_END	0x30700000			// 0x30700000~(0x30800000-1)用于存放各模式的栈以及页表
#define _MEM_START	0x300f0000			// 0x30000000~(0x300f0000-1)用于存放内核
										// 0X300f0000~(0x30700000-1)用于参与伙伴算法的分配，即用来存放页和struct page结构体
#define PAGE_SHIFT	(12)
#define PAGE_SIZE	(1<<PAGE_SHIFT)		// 页大小为4k(0b1 0000 0000 0000)
#define PAGE_MASK	(~(PAGE_SIZE-1))	// 0000 0000 0000

#define KERNEL_MEM_END		(_MEM_END)
// 存放页的起始地址，需4k对齐
#define KERNEL_PAGING_START	( (_MEM_START+(~PAGE_MASK)) & (PAGE_MASK) )
// 存放页的结束地址 = ( 可用空间/(4k+sizeof(struct page)) * 4k ) + KERNEL_PAGING_START
#define	KERNEL_PAGING_END	((  (KERNEL_MEM_END-KERNEL_PAGING_START)/(PAGE_SIZE+sizeof(struct page))  )*(PAGE_SIZE)+KERNEL_PAGING_START)

// 页数
#define KERNEL_PAGE_NUM		( (KERNEL_PAGING_END-KERNEL_PAGING_START)/PAGE_SIZE )

// 存放page结构体的结束地址
#define KERNEL_PAGE_END		_MEM_END
// 存放page结构体的起始地址
#define KERNEL_PAGE_START	(KERNEL_PAGE_END-KERNEL_PAGE_NUM*sizeof(struct page))

// page flags
#define PAGE_AVAILABLE		0x00				// 可用
#define PAGE_DIRTY			0x01
#define PAGE_PROTECT		0x02
#define PAGE_BUDDY_BUSY		0x04				// 已分配
#define PAGE_IN_CACHE		0x08

// buddy可以是页(4K)的2^0~2^8倍，共9种buddy
#define MAX_BUDDY_PAGE_NUM	(9)

#define AVERAGE_PAGE_NUM_PER_BUDDY	(KERNEL_PAGE_NUM/MAX_BUDDY_PAGE_NUM)
#define PAGE_NUM_FOR_EACH_BUDDY(j) ((AVERAGE_PAGE_NUM_PER_BUDDY>>(j))*(1<<(j)))
#define PAGE_NUM_FOR_MAX_BUDDY	((1<<MAX_BUDDY_PAGE_NUM)-1)

// 同一种buddy用一个链表连接，需9个链表头。
// 例如page_buddy[5]所代表的双向链表的内容都是大小为2^5的buddy
struct list_head page_buddy[MAX_BUDDY_PAGE_NUM];

struct kmem_cache
{
	unsigned int obj_size;
	unsigned int obj_nr;
	unsigned int page_order;
	unsigned int flags;
	struct page *head_page;
	struct page *end_page;
	void *nf_block;
};

struct page
{
	unsigned int vaddr;										// 页地址
	unsigned int flags;										// 页标志
	int order;												// 是否是buddy的首页
	struct kmem_cache *cachep;
	struct list_head list;
};


///////////////////////////////////////伙伴算法////////////////////////////////////////


// 初始化9种buddy所对应的链表头
void init_page_buddy(void)
{
	int i;
	for(i=0;i<MAX_BUDDY_PAGE_NUM;i++)
	{
		INIT_LIST_HEAD(&page_buddy[i]);
	}
}

// 初始化buddy
void init_page_map(void)
{
	int i;
	struct page *pg=(struct page *)KERNEL_PAGE_START;

	init_page_buddy();										// 初始化9种buddy所对应的链表头

	for(i=0;i<(KERNEL_PAGE_NUM);pg++,i++)					// 初始化buddy,实质就是设置每一页分别属于哪个buddy
	{
		pg->vaddr=KERNEL_PAGING_START+i*PAGE_SIZE;			// 填充struct page
		pg->flags=PAGE_AVAILABLE;
		INIT_LIST_HEAD(&(pg->list));

		// 尽可能的划分成最大的buddy
		if( i<(KERNEL_PAGE_NUM&(~PAGE_NUM_FOR_MAX_BUDDY)) )	// 如果i<(页数/256)*256
		{
			if((i&PAGE_NUM_FOR_MAX_BUDDY)==0)				// 如果i是256的整数倍
			{
				pg->order=MAX_BUDDY_PAGE_NUM-1;				// 则标志该page是buddy的上边界
			}
			else
			{
				pg->order=-1;
			}
			// 将该页中的list_head添加到对应buddy链表中
			list_add_tail(&(pg->list),&page_buddy[MAX_BUDDY_PAGE_NUM-1]);
		}
		else												// 当空间不足划分成最大的buddy时，就全部划分成最小的buddy。
		{
			pg->order=0;
			list_add_tail(&(pg->list),&page_buddy[0]);
		}
	}
}

#define BUDDY_END(x,order)			( (x)+(1<<(order))-1 )
#define NEXT_BUDDY_START(x,order)	( (x)+(1<<(order)) )
#define PREV_BUDDY_START(x,order)	( (x)-(1<<(order)) )

// buddy的申请
// order：用来表示buddy的种类
struct page *get_pages_from_list(int order)
{
	unsigned int vaddr;
	int neworder=order;
	struct page *pg,*ret;
	struct list_head *tlst,*tlst1;

	for(;neworder<MAX_BUDDY_PAGE_NUM;neworder++)			// 遍历9种buddy的链表
	{
		if(list_empty(&page_buddy[neworder]))				// buddy链表为空的话，则继续for循环
		{
			continue;
		}
		else												// 当找到有buddy链表不为空时，则取出buddy
		{
			// pg = buddy里的第一个struct page
			pg=list_entry(page_buddy[neworder].next,struct page,list);

			tlst=&(BUDDY_END(pg,neworder)->list);			// tlst = buddy里的最后一个struct page里的list_head

			tlst->next->prev=&page_buddy[neworder];			// 将buddy从链表中拆下来
			page_buddy[neworder].next=tlst->next;
			break;
		}
	}
	if(neworder == MAX_BUDDY_PAGE_NUM)
		return NULL;

	for(neworder--; neworder>=order; neworder--)			// 如果(neworder--) > order，则需要对原buddy进行拆分
	{
		// buddy的前半部分：
		// tlst = buddy里的第一个struct page里的list_head
		tlst=&(pg->list);
		// tlst1 = buddy里的中间的struct page里的list_head
		tlst1=&(BUDDY_END(pg,neworder)->list);
		// 更新order
		list_entry(tlst,struct page,list)->order=neworder;
		// 挂接链表tlst~tlst1到低阶buddy链表上
		list_add_chain_tail(tlst,tlst1,&page_buddy[neworder]);


		// buddy的后半部分：
		pg=NEXT_BUDDY_START(pg,neworder);
		pg->order = neworder;
	}

	pg->flags|=PAGE_BUDDY_BUSY;
	return pg;
}

// buddy的释放
// page：buddy的第一个struct page的地址
// order：用来表示buddy的种类
void put_pages_to_list(struct page *pg,int order)
{
	struct page *tprev,*tnext;

	if(!(pg->flags&PAGE_BUDDY_BUSY))						// 如果释放的是not busy的buddy，则报错
	{
		printk("something must be wrong when you see this message,that probably means you are forcing to release a page that was not alloc at all\n");
		return;
	}
	pg->flags &= ~(PAGE_BUDDY_BUSY);						// 清掉busy位

	for(;order<MAX_BUDDY_PAGE_NUM;order++)					// 循环合并
	{
		tnext=NEXT_BUDDY_START(pg,order);					// next buddy
		tprev=PREV_BUDDY_START(pg,order);					// prev buddy

		// 是否可以和next buddy合并
		if( (!(tnext->flags&PAGE_BUDDY_BUSY)) && (tnext->order==order) )
		{
			pg->order++;									// 更新order
			tnext->order=-1;
			// next buddy从buddy链表中拆下
			list_remove_chain(&(tnext->list),&(BUDDY_END(tnext,order)->list));
			// 合并next buddy
			BUDDY_END(pg,order)->list.next=&(tnext->list);
			tnext->list.prev=&(BUDDY_END(pg,order)->list);
			continue;
		}
		// 是否可以和前一个buddy合并
		else if( (!(tprev->flags&PAGE_BUDDY_BUSY))&&(tprev->order==order) )
		{
			pg->order=-1;
			// prev buddy从buddy链表中拆下
			list_remove_chain(&(tprev->list),&(BUDDY_END(tprev,order)->list));
			// 合并prev buddy
			BUDDY_END(tprev,order)->list.next=&(pg->list);
			pg->list.prev=&(BUDDY_END(tprev,order)->list);

			pg=tprev;
			pg->order++;
			continue;
		}
		else
		{
			break;
		}
	}

	// 将合并后的buddy放入对应的buddy链表中
	list_add_chain(&(pg->list),&((tnext-1)->list),&page_buddy[order]);
}

// 将struct page地址转化为页地址
void *page_address(struct page *pg)
{
	return (void *)(pg->vaddr);
}

// 用户申请buddy API
struct page *alloc_pages(unsigned int flag,int order)
{
	struct page *pg;
	int i;
	pg=get_pages_from_list(order);
	if(pg==NULL)
		return NULL;
	for(i=0;i<(1<<order);i++)
	{
		(pg+i)->flags|=PAGE_DIRTY;
	}
	return pg;
}

// 用户申请内存 API
void *get_free_pages(unsigned int flag,int order)
{
	struct page * page;

	page = alloc_pages(flag, order);						// 分配buddy
	if (!page)
		return NULL;

	return	page_address(page);								// 返回buddy所指的内存的首地址
}

// 算出addr是属于哪个struct page
struct page *virt_to_page(unsigned int addr)
{
	unsigned int i;

	i=((addr)-KERNEL_PAGING_START)>>PAGE_SHIFT;				// 第几页
	if(i>KERNEL_PAGE_NUM)
		return NULL;
	return (struct page *)KERNEL_PAGE_START+i;
}

// 用户释放buddy API
void free_pages(struct page *pg,int order)
{
	int i;
	for(i=0;i<(1<<order);i++)
	{
		(pg+i)->flags &= ~PAGE_DIRTY;
	}
	put_pages_to_list(pg,order);
}

// 用户释放内存 API
void put_free_pages(void *addr,int order)
{
	free_pages(virt_to_page((unsigned int)addr),order);
}



/////////////////////////////////////////slab//////////////////////////////////////////


#define KMEM_CACHE_DEFAULT_ORDER	(0)
#define KMEM_CACHE_MAX_ORDER		(5)
#define KMEM_CACHE_SAVE_RATE		(90)
#define KMEM_CACHE_PERCENT			(100)
#define KMEM_CACHE_MAX_WAST			(PAGE_SIZE-KMEM_CACHE_SAVE_RATE*PAGE_SIZE/KMEM_CACHE_PERCENT)


int find_right_order(unsigned int size)
{
	int order;
	for(order=0;order<=KMEM_CACHE_MAX_ORDER;order++)
	{
		// size <= (4k*0.1)*(1<<order),eg: 4K <= (4k*0.1)*(1<<4)
		if( size<=(KMEM_CACHE_MAX_WAST)*(1<<order) )
		{
			return order;
		}
	}
	if(size>(1<<order))
		return -1;
	return order;
}

// 初始化slab
int kmem_cache_line_object(void *head,unsigned int size,int order)
{
	void **pl;
	char *p;
	pl=(void **)head;										// pl指向"指向子内存块的的指针"
	p=(char *)head+size;									// p指向下一个子内存块
	int i;
	int s=PAGE_SIZE*(1<<order);								// slab的总大小

	for(i=0;s>size;i++,s-=size)
	{
		*pl=(void *)p;										// 上一块子内存的头指向下一块子内存的头

		pl=(void **)p;										// 更新pl和p
		p=p+size;
	}

	if(s==size)
		i++;

	return i;												// 返回子内存块的个数
}

// 创建slab，实质就是初始化struct kmem_cache
struct kmem_cache *kmem_cache_create(struct kmem_cache *cache,unsigned int size,unsigned int flags)
{
	void **nf_block=&(cache->nf_block);

	// 根据size计算出order值
	int order=find_right_order(size);
	if( order==-1 )
		return NULL;

	// 申请一个buddy用作slab用途
	if( (cache->head_page=alloc_pages(0,order))==NULL )				// slab起始页的struct page
		return NULL;
	*nf_block=page_address(cache->head_page);						// 下一个可用的子内存块的地址
	// 初始化slab
	cache->obj_nr=kmem_cache_line_object(*nf_block,size,order);		// 子内存块的数目
	cache->obj_size=size;											// 子内存块的大小
	cache->page_order=order;										// buddy的order值
	cache->flags=flags;
	cache->end_page=BUDDY_END(cache->head_page,order);				// slab末尾页的struct page
	cache->end_page->list.next=NULL;

	return cache;
}

// 销毁slab
void kmem_cache_destroy(struct kmem_cache *cache)
{
	int order=cache->page_order;
	struct page *pg=cache->head_page;
	struct list_head *list;
	while(1)
	{
		list=BUDDY_END(pg,order)->list.next;
		free_pages(pg,order);										// 释放buddy
		if(list)													// 如果还有buddy，则继续释放
		{
			pg=list_entry(list,struct page,list);
		}
		else
		{
			return;
		}
	}
}

// 通过slab分配内存
void *kmem_cache_alloc(struct kmem_cache *cache,unsigned int flag)
{
	void *p;
	struct page *pg;
	if(cache==NULL)
		return NULL;
	void **nf_block=&(cache->nf_block);
	unsigned int *nr=&(cache->obj_nr);
	int order=cache->page_order;

	// 如果子内存块已经用完了，则需要重新分配一个buddy并重新设置struct kmem_cache
	if(!*nr)
	{
		if((pg=alloc_pages(0,order))==NULL)				// 重新申请一个buddy
			return NULL;
		*nf_block=page_address(pg);						// nf_block指向下一个可用的子内存块
		cache->end_page->list.next=&pg->list;			// 将新的buddy连接到原slab上
		cache->end_page=BUDDY_END(pg,order);
		cache->end_page->list.next=NULL;
		*nr+=kmem_cache_line_object(*nf_block,cache->obj_size,order);
	}

	(*nr)--;											// 更新子内存块数目
	p=*nf_block;
	*nf_block=*(void **)p;								// nf_block指向下一个子内存块
	pg=virt_to_page((unsigned int)p);					// 设置struct page 里的cachep
	pg->cachep=cache;
	return p;
}

// 释放通过slab申请到的内存
void kmem_cache_free(struct kmem_cache *cache,void *objp)
{
	// objp是一个地址，地址也是一个数字，
	// 先把这个数字转化为指针，再将该指针指向下一个子内存块
	*(void **)objp=cache->nf_block;						// objp指向下一个子内存块
	cache->nf_block=objp;								// nf_block指向需要被释放的子内存块objp
	cache->obj_nr++;									// 更新子内存块数目
}


/////////////////////////////////////////kmalloc//////////////////////////////////////////


#define KMALLOC_BIAS_SHIFT			(5)
#define KMALLOC_MAX_SIZE			(4096)
#define KMALLOC_MINIMAL_SIZE_BIAS	(1<<(KMALLOC_BIAS_SHIFT))
// 128，即有128个slab
#define KMALLOC_CACHE_SIZE			(KMALLOC_MAX_SIZE/KMALLOC_MINIMAL_SIZE_BIAS)

struct kmem_cache kmalloc_cache[KMALLOC_CACHE_SIZE]={{0,0,0,0,NULL,NULL,NULL},};

#define kmalloc_cache_size_to_index(size)	( ( ( (size))>>(KMALLOC_BIAS_SHIFT ) ) )

int kmalloc_init(void)
{
	int i=0;
	for(i=0;i<KMALLOC_CACHE_SIZE;i++)					// 循环创建128个slab，size是32byte...4K
	{
		if(kmem_cache_create(&kmalloc_cache[i],(i+1)*KMALLOC_MINIMAL_SIZE_BIAS,0)==NULL)
			return -1;
	}
	return 0;
}

void *kmalloc(unsigned int size)
{
	int index=kmalloc_cache_size_to_index(size);		// 根据size确定使用哪个slab
	if(index>=KMALLOC_CACHE_SIZE)
		return NULL;
	return kmem_cache_alloc(&kmalloc_cache[index],0);	// 分配子内存块
}

void kfree(void *addr)
{
	struct page *pg;
	pg=virt_to_page((unsigned int)addr);
	kmem_cache_free(pg->cachep,addr);
}
