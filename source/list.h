
struct list_head
{
	struct list_head *next, *prev;
};

// 初始化链表
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

// 在prev和next之间添加节点
static inline void __list_add(struct list_head *new_lst, struct list_head *prev, struct list_head *next)
{
	next->prev = new_lst;
	new_lst->next = next;
	new_lst->prev = prev;
	prev->next = new_lst;
}

// 在head之后添加一个节点
static inline void list_add(struct list_head *new_lst, struct list_head *head)
{
	__list_add(new_lst, head, head->next);
}

// 在head之前添加一个节点
static inline void list_add_tail(struct list_head *new_lst, struct list_head *head)
{
	__list_add(new_lst, head->prev, head);
}

// 删除prev和next之间的节点
static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

// 删除节点entry
static inline void list_del(struct list_head * entry)
{
	__list_del(entry->prev,entry->next);
}

// 删除节点ch到ct
static inline void list_remove_chain(struct list_head *ch,struct list_head *ct)
{
	ch->prev->next=ct->next;
	ct->next->prev=ch->prev;
}

// 在head之后增加节点ch到ct
static inline void list_add_chain(struct list_head *ch,struct list_head *ct,struct list_head *head)
{
	ch->prev=head;
	ct->next=head->next;
	head->next->prev=ct;
	head->next=ch;
}

// 在head之前增加节点ch到ct
static inline void list_add_chain_tail(struct list_head *ch,struct list_head *ct,struct list_head *head)
{
	ch->prev=head->prev;
	head->prev->next=ch;
	head->prev=ct;
	ct->next=head;
}

// 判断链表是否为空
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

// 求出结构体首地址和它的某一个成员地址的偏移
#define offsetof(TYPE, MEMBER) ((unsigned int) &((TYPE *)0)->MEMBER)


// 获得包含list_head的结构体
// <1> 定义了一个list_head类型的指针__mptr
// <2> 包含list_head的结构体的地址 = __mptr - 偏移值
#define container_of(ptr, type, member) ({			\
		const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
		(type *)( (char *)__mptr - offsetof(type,member) );})

// 获得包含list_head的结构体
#define list_entry(ptr,type,member)		container_of(ptr, type, member)



#define list_for_each(pos, head) \
		for (pos = (head)->next; pos != (head); pos = pos->next)



