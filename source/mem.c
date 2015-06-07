#include "list.h"

#define	NULL	((void *)0)

#define _MEM_END	0x30700000			// 0x30700000~(0x30800000-1)���ڴ�Ÿ�ģʽ��ջ�Լ�ҳ��
#define _MEM_START	0x300f0000			// 0x30000000~(0x300f0000-1)���ڴ���ں�
										// 0X300f0000~(0x30700000-1)���ڲ������㷨�ķ��䣬���������ҳ��struct page�ṹ��
#define PAGE_SHIFT	(12)
#define PAGE_SIZE	(1<<PAGE_SHIFT)		// ҳ��СΪ4k(0b1 0000 0000 0000)
#define PAGE_MASK	(~(PAGE_SIZE-1))	// 0000 0000 0000

#define KERNEL_MEM_END		(_MEM_END)
// ���ҳ����ʼ��ַ����4k����
#define KERNEL_PAGING_START	( (_MEM_START+(~PAGE_MASK)) & (PAGE_MASK) )
// ���ҳ�Ľ�����ַ = ( ���ÿռ�/(4k+sizeof(struct page)) * 4k ) + KERNEL_PAGING_START
#define	KERNEL_PAGING_END	((  (KERNEL_MEM_END-KERNEL_PAGING_START)/(PAGE_SIZE+sizeof(struct page))  )*(PAGE_SIZE)+KERNEL_PAGING_START)

// ҳ��
#define KERNEL_PAGE_NUM		( (KERNEL_PAGING_END-KERNEL_PAGING_START)/PAGE_SIZE )

// ���page�ṹ��Ľ�����ַ
#define KERNEL_PAGE_END		_MEM_END
// ���page�ṹ�����ʼ��ַ
#define KERNEL_PAGE_START	(KERNEL_PAGE_END-KERNEL_PAGE_NUM*sizeof(struct page))

// page flags
#define PAGE_AVAILABLE		0x00				// ����
#define PAGE_DIRTY			0x01
#define PAGE_PROTECT		0x02
#define PAGE_BUDDY_BUSY		0x04				// �ѷ���
#define PAGE_IN_CACHE		0x08

// buddy������ҳ(4K)��2^0~2^8������9��buddy
#define MAX_BUDDY_PAGE_NUM	(9)

#define AVERAGE_PAGE_NUM_PER_BUDDY	(KERNEL_PAGE_NUM/MAX_BUDDY_PAGE_NUM)
#define PAGE_NUM_FOR_EACH_BUDDY(j) ((AVERAGE_PAGE_NUM_PER_BUDDY>>(j))*(1<<(j)))
#define PAGE_NUM_FOR_MAX_BUDDY	((1<<MAX_BUDDY_PAGE_NUM)-1)

// ͬһ��buddy��һ���������ӣ���9������ͷ��
// ����page_buddy[5]�������˫����������ݶ��Ǵ�СΪ2^5��buddy
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
	unsigned int vaddr;										// ҳ��ַ
	unsigned int flags;										// ҳ��־
	int order;												// �Ƿ���buddy����ҳ
	struct kmem_cache *cachep;
	struct list_head list;
};


///////////////////////////////////////����㷨////////////////////////////////////////


// ��ʼ��9��buddy����Ӧ������ͷ
void init_page_buddy(void)
{
	int i;
	for(i=0;i<MAX_BUDDY_PAGE_NUM;i++)
	{
		INIT_LIST_HEAD(&page_buddy[i]);
	}
}

// ��ʼ��buddy
void init_page_map(void)
{
	int i;
	struct page *pg=(struct page *)KERNEL_PAGE_START;

	init_page_buddy();										// ��ʼ��9��buddy����Ӧ������ͷ

	for(i=0;i<(KERNEL_PAGE_NUM);pg++,i++)					// ��ʼ��buddy,ʵ�ʾ�������ÿһҳ�ֱ������ĸ�buddy
	{
		pg->vaddr=KERNEL_PAGING_START+i*PAGE_SIZE;			// ���struct page
		pg->flags=PAGE_AVAILABLE;
		INIT_LIST_HEAD(&(pg->list));

		// �����ܵĻ��ֳ�����buddy
		if( i<(KERNEL_PAGE_NUM&(~PAGE_NUM_FOR_MAX_BUDDY)) )	// ���i<(ҳ��/256)*256
		{
			if((i&PAGE_NUM_FOR_MAX_BUDDY)==0)				// ���i��256��������
			{
				pg->order=MAX_BUDDY_PAGE_NUM-1;				// ���־��page��buddy���ϱ߽�
			}
			else
			{
				pg->order=-1;
			}
			// ����ҳ�е�list_head��ӵ���Ӧbuddy������
			list_add_tail(&(pg->list),&page_buddy[MAX_BUDDY_PAGE_NUM-1]);
		}
		else												// ���ռ䲻�㻮�ֳ�����buddyʱ����ȫ�����ֳ���С��buddy��
		{
			pg->order=0;
			list_add_tail(&(pg->list),&page_buddy[0]);
		}
	}
}

#define BUDDY_END(x,order)			( (x)+(1<<(order))-1 )
#define NEXT_BUDDY_START(x,order)	( (x)+(1<<(order)) )
#define PREV_BUDDY_START(x,order)	( (x)-(1<<(order)) )

// buddy������
// order��������ʾbuddy������
struct page *get_pages_from_list(int order)
{
	unsigned int vaddr;
	int neworder=order;
	struct page *pg,*ret;
	struct list_head *tlst,*tlst1;

	for(;neworder<MAX_BUDDY_PAGE_NUM;neworder++)			// ����9��buddy������
	{
		if(list_empty(&page_buddy[neworder]))				// buddy����Ϊ�յĻ��������forѭ��
		{
			continue;
		}
		else												// ���ҵ���buddy����Ϊ��ʱ����ȡ��buddy
		{
			// pg = buddy��ĵ�һ��struct page
			pg=list_entry(page_buddy[neworder].next,struct page,list);

			tlst=&(BUDDY_END(pg,neworder)->list);			// tlst = buddy������һ��struct page���list_head

			tlst->next->prev=&page_buddy[neworder];			// ��buddy�������в�����
			page_buddy[neworder].next=tlst->next;
			break;
		}
	}
	if(neworder == MAX_BUDDY_PAGE_NUM)
		return NULL;

	for(neworder--; neworder>=order; neworder--)			// ���(neworder--) > order������Ҫ��ԭbuddy���в��
	{
		// buddy��ǰ�벿�֣�
		// tlst = buddy��ĵ�һ��struct page���list_head
		tlst=&(pg->list);
		// tlst1 = buddy����м��struct page���list_head
		tlst1=&(BUDDY_END(pg,neworder)->list);
		// ����order
		list_entry(tlst,struct page,list)->order=neworder;
		// �ҽ�����tlst~tlst1���ͽ�buddy������
		list_add_chain_tail(tlst,tlst1,&page_buddy[neworder]);


		// buddy�ĺ�벿�֣�
		pg=NEXT_BUDDY_START(pg,neworder);
		pg->order = neworder;
	}

	pg->flags|=PAGE_BUDDY_BUSY;
	return pg;
}

// buddy���ͷ�
// page��buddy�ĵ�һ��struct page�ĵ�ַ
// order��������ʾbuddy������
void put_pages_to_list(struct page *pg,int order)
{
	struct page *tprev,*tnext;

	if(!(pg->flags&PAGE_BUDDY_BUSY))						// ����ͷŵ���not busy��buddy���򱨴�
	{
		printk("something must be wrong when you see this message,that probably means you are forcing to release a page that was not alloc at all\n");
		return;
	}
	pg->flags &= ~(PAGE_BUDDY_BUSY);						// ���busyλ

	for(;order<MAX_BUDDY_PAGE_NUM;order++)					// ѭ���ϲ�
	{
		tnext=NEXT_BUDDY_START(pg,order);					// next buddy
		tprev=PREV_BUDDY_START(pg,order);					// prev buddy

		// �Ƿ���Ժ�next buddy�ϲ�
		if( (!(tnext->flags&PAGE_BUDDY_BUSY)) && (tnext->order==order) )
		{
			pg->order++;									// ����order
			tnext->order=-1;
			// next buddy��buddy�����в���
			list_remove_chain(&(tnext->list),&(BUDDY_END(tnext,order)->list));
			// �ϲ�next buddy
			BUDDY_END(pg,order)->list.next=&(tnext->list);
			tnext->list.prev=&(BUDDY_END(pg,order)->list);
			continue;
		}
		// �Ƿ���Ժ�ǰһ��buddy�ϲ�
		else if( (!(tprev->flags&PAGE_BUDDY_BUSY))&&(tprev->order==order) )
		{
			pg->order=-1;
			// prev buddy��buddy�����в���
			list_remove_chain(&(tprev->list),&(BUDDY_END(tprev,order)->list));
			// �ϲ�prev buddy
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

	// ���ϲ����buddy�����Ӧ��buddy������
	list_add_chain(&(pg->list),&((tnext-1)->list),&page_buddy[order]);
}

// ��struct page��ַת��Ϊҳ��ַ
void *page_address(struct page *pg)
{
	return (void *)(pg->vaddr);
}

// �û�����buddy API
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

// �û������ڴ� API
void *get_free_pages(unsigned int flag,int order)
{
	struct page * page;

	page = alloc_pages(flag, order);						// ����buddy
	if (!page)
		return NULL;

	return	page_address(page);								// ����buddy��ָ���ڴ���׵�ַ
}

// ���addr�������ĸ�struct page
struct page *virt_to_page(unsigned int addr)
{
	unsigned int i;

	i=((addr)-KERNEL_PAGING_START)>>PAGE_SHIFT;				// �ڼ�ҳ
	if(i>KERNEL_PAGE_NUM)
		return NULL;
	return (struct page *)KERNEL_PAGE_START+i;
}

// �û��ͷ�buddy API
void free_pages(struct page *pg,int order)
{
	int i;
	for(i=0;i<(1<<order);i++)
	{
		(pg+i)->flags &= ~PAGE_DIRTY;
	}
	put_pages_to_list(pg,order);
}

// �û��ͷ��ڴ� API
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

// ��ʼ��slab
int kmem_cache_line_object(void *head,unsigned int size,int order)
{
	void **pl;
	char *p;
	pl=(void **)head;										// plָ��"ָ�����ڴ��ĵ�ָ��"
	p=(char *)head+size;									// pָ����һ�����ڴ��
	int i;
	int s=PAGE_SIZE*(1<<order);								// slab���ܴ�С

	for(i=0;s>size;i++,s-=size)
	{
		*pl=(void *)p;										// ��һ�����ڴ��ͷָ����һ�����ڴ��ͷ

		pl=(void **)p;										// ����pl��p
		p=p+size;
	}

	if(s==size)
		i++;

	return i;												// �������ڴ��ĸ���
}

// ����slab��ʵ�ʾ��ǳ�ʼ��struct kmem_cache
struct kmem_cache *kmem_cache_create(struct kmem_cache *cache,unsigned int size,unsigned int flags)
{
	void **nf_block=&(cache->nf_block);

	// ����size�����orderֵ
	int order=find_right_order(size);
	if( order==-1 )
		return NULL;

	// ����һ��buddy����slab��;
	if( (cache->head_page=alloc_pages(0,order))==NULL )				// slab��ʼҳ��struct page
		return NULL;
	*nf_block=page_address(cache->head_page);						// ��һ�����õ����ڴ��ĵ�ַ
	// ��ʼ��slab
	cache->obj_nr=kmem_cache_line_object(*nf_block,size,order);		// ���ڴ�����Ŀ
	cache->obj_size=size;											// ���ڴ��Ĵ�С
	cache->page_order=order;										// buddy��orderֵ
	cache->flags=flags;
	cache->end_page=BUDDY_END(cache->head_page,order);				// slabĩβҳ��struct page
	cache->end_page->list.next=NULL;

	return cache;
}

// ����slab
void kmem_cache_destroy(struct kmem_cache *cache)
{
	int order=cache->page_order;
	struct page *pg=cache->head_page;
	struct list_head *list;
	while(1)
	{
		list=BUDDY_END(pg,order)->list.next;
		free_pages(pg,order);										// �ͷ�buddy
		if(list)													// �������buddy��������ͷ�
		{
			pg=list_entry(list,struct page,list);
		}
		else
		{
			return;
		}
	}
}

// ͨ��slab�����ڴ�
void *kmem_cache_alloc(struct kmem_cache *cache,unsigned int flag)
{
	void *p;
	struct page *pg;
	if(cache==NULL)
		return NULL;
	void **nf_block=&(cache->nf_block);
	unsigned int *nr=&(cache->obj_nr);
	int order=cache->page_order;

	// ������ڴ���Ѿ������ˣ�����Ҫ���·���һ��buddy����������struct kmem_cache
	if(!*nr)
	{
		if((pg=alloc_pages(0,order))==NULL)				// ��������һ��buddy
			return NULL;
		*nf_block=page_address(pg);						// nf_blockָ����һ�����õ����ڴ��
		cache->end_page->list.next=&pg->list;			// ���µ�buddy���ӵ�ԭslab��
		cache->end_page=BUDDY_END(pg,order);
		cache->end_page->list.next=NULL;
		*nr+=kmem_cache_line_object(*nf_block,cache->obj_size,order);
	}

	(*nr)--;											// �������ڴ����Ŀ
	p=*nf_block;
	*nf_block=*(void **)p;								// nf_blockָ����һ�����ڴ��
	pg=virt_to_page((unsigned int)p);					// ����struct page ���cachep
	pg->cachep=cache;
	return p;
}

// �ͷ�ͨ��slab���뵽���ڴ�
void kmem_cache_free(struct kmem_cache *cache,void *objp)
{
	// objp��һ����ַ����ַҲ��һ�����֣�
	// �Ȱ��������ת��Ϊָ�룬�ٽ���ָ��ָ����һ�����ڴ��
	*(void **)objp=cache->nf_block;						// objpָ����һ�����ڴ��
	cache->nf_block=objp;								// nf_blockָ����Ҫ���ͷŵ����ڴ��objp
	cache->obj_nr++;									// �������ڴ����Ŀ
}


/////////////////////////////////////////kmalloc//////////////////////////////////////////


#define KMALLOC_BIAS_SHIFT			(5)
#define KMALLOC_MAX_SIZE			(4096)
#define KMALLOC_MINIMAL_SIZE_BIAS	(1<<(KMALLOC_BIAS_SHIFT))
// 128������128��slab
#define KMALLOC_CACHE_SIZE			(KMALLOC_MAX_SIZE/KMALLOC_MINIMAL_SIZE_BIAS)

struct kmem_cache kmalloc_cache[KMALLOC_CACHE_SIZE]={{0,0,0,0,NULL,NULL,NULL},};

#define kmalloc_cache_size_to_index(size)	( ( ( (size))>>(KMALLOC_BIAS_SHIFT ) ) )

int kmalloc_init(void)
{
	int i=0;
	for(i=0;i<KMALLOC_CACHE_SIZE;i++)					// ѭ������128��slab��size��32byte...4K
	{
		if(kmem_cache_create(&kmalloc_cache[i],(i+1)*KMALLOC_MINIMAL_SIZE_BIAS,0)==NULL)
			return -1;
	}
	return 0;
}

void *kmalloc(unsigned int size)
{
	int index=kmalloc_cache_size_to_index(size);		// ����sizeȷ��ʹ���ĸ�slab
	if(index>=KMALLOC_CACHE_SIZE)
		return NULL;
	return kmem_cache_alloc(&kmalloc_cache[index],0);	// �������ڴ��
}

void kfree(void *addr)
{
	struct page *pg;
	pg=virt_to_page((unsigned int)addr);
	kmem_cache_free(pg->cachep,addr);
}
