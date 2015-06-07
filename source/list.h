
struct list_head
{
	struct list_head *next, *prev;
};

// ��ʼ������
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

// ��prev��next֮����ӽڵ�
static inline void __list_add(struct list_head *new_lst, struct list_head *prev, struct list_head *next)
{
	next->prev = new_lst;
	new_lst->next = next;
	new_lst->prev = prev;
	prev->next = new_lst;
}

// ��head֮�����һ���ڵ�
static inline void list_add(struct list_head *new_lst, struct list_head *head)
{
	__list_add(new_lst, head, head->next);
}

// ��head֮ǰ���һ���ڵ�
static inline void list_add_tail(struct list_head *new_lst, struct list_head *head)
{
	__list_add(new_lst, head->prev, head);
}

// ɾ��prev��next֮��Ľڵ�
static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

// ɾ���ڵ�entry
static inline void list_del(struct list_head * entry)
{
	__list_del(entry->prev,entry->next);
}

// ɾ���ڵ�ch��ct
static inline void list_remove_chain(struct list_head *ch,struct list_head *ct)
{
	ch->prev->next=ct->next;
	ct->next->prev=ch->prev;
}

// ��head֮�����ӽڵ�ch��ct
static inline void list_add_chain(struct list_head *ch,struct list_head *ct,struct list_head *head)
{
	ch->prev=head;
	ct->next=head->next;
	head->next->prev=ct;
	head->next=ch;
}

// ��head֮ǰ���ӽڵ�ch��ct
static inline void list_add_chain_tail(struct list_head *ch,struct list_head *ct,struct list_head *head)
{
	ch->prev=head->prev;
	head->prev->next=ch;
	head->prev=ct;
	ct->next=head;
}

// �ж������Ƿ�Ϊ��
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

// ����ṹ���׵�ַ������ĳһ����Ա��ַ��ƫ��
#define offsetof(TYPE, MEMBER) ((unsigned int) &((TYPE *)0)->MEMBER)


// ��ð���list_head�Ľṹ��
// <1> ������һ��list_head���͵�ָ��__mptr
// <2> ����list_head�Ľṹ��ĵ�ַ = __mptr - ƫ��ֵ
#define container_of(ptr, type, member) ({			\
		const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
		(type *)( (char *)__mptr - offsetof(type,member) );})

// ��ð���list_head�Ľṹ��
#define list_entry(ptr,type,member)		container_of(ptr, type, member)



#define list_for_each(pos, head) \
		for (pos = (head)->next; pos != (head); pos = pos->next)



