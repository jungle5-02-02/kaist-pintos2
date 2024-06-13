/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "kernel/hash.h"
#include "userprog/process.h"
#include "vm/file.h"
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();

	lcg_state = 4321;
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	
	// Frame table init
	list_init(&frame_table.ft_list);
	lock_init(&frame_table_lock);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		// printf("couldn't found spt page. new page malloc ");
		struct page *page = calloc(1,sizeof(struct page));
		// printf("ok\n");
		// printf("this page is pointing to %p\n", page);
		bool (*page_initializer) (struct page *, enum vm_type, void *);
		if (VM_TYPE(type) == VM_ANON) page_initializer = anon_initializer;
		else if (VM_TYPE(type) == VM_FILE) page_initializer = file_backed_initializer;

		uninit_new(page, upage, init, type, aux, page_initializer);
		page->writable = writable;
		// printf("TYPE: %d\n", type);
		// printf("va is now %p\n", page->va);
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	// va로 hash_elem검색을 위한 빈 페이지 생성
	
	struct page binpage;
	binpage.va = pg_round_down(va); // pgrounddown은 뒤 12자리를 절삭함으로써 가상주소를 페이지 번호로 바꿔줌
	struct hash_elem e;
	binpage.hash_elem = e;
	// printf("binpage malloc ");
	// struct page *binpage = malloc(sizeof(page));
	// printf("ok\n");
	// binpage->va = pg_round_down(va);
	struct hash_elem *elem = hash_find(&spt->spt_hash, &binpage.hash_elem);
	// free(binpage);
	if (elem) {
		page = hash_entry(elem, struct page, hash_elem);
	}
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if(!hash_insert(&spt->spt_hash, &page->hash_elem)) succ = true;
	// if (!succ) printf("already have this page\n");
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	// RUSSIAN ROULETTE!!!!!!!
	lock_acquire(&frame_table_lock);
	int size = list_size(&frame_table.ft_list);
	// printf("size: %d\n", size);
	while(true) {
		lcg_state = (lcg_state*133 + 4294967)%29496729;
		if (lcg_state < 0) lcg_state = -lcg_state;
		int v_id = lcg_state % size;
		// printf("%d 번 프레임 당첨!\n", v_id);
		struct list_elem *e = list_begin(&frame_table.ft_list);
		for (int i = 0; i < v_id; i++) {
			e = list_next(e);
		}
		victim = list_entry(e, struct frame, frame_list_elem);
		if (victim->page == NULL) continue;
		// if (victim->page->va > 0x47479000) {printf("stack protect\n"); continue;}
		if (pml4_is_accessed(thread_current()->pml4, victim->page->va)) {
			pml4_set_accessed(thread_current()->pml4, victim->page->va, 0);
			continue;
		}
		break;
	}
	lock_release(&frame_table_lock);
	
	// printf("%p, %p YANKEE GO HOME\n", victim->kva, victim->page->va);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	void *kva = palloc_get_page(PAL_USER);
	
	if (kva == NULL) {
		frame = vm_evict_frame();
		// printf("ANON SWAP OUT\n");
	} 
	else {
		frame = (struct frame *) calloc(1, sizeof(struct frame));
		frame->kva = kva;
		lock_acquire(&frame_table_lock);
		list_push_back(&frame_table.ft_list, &frame->frame_list_elem);
		lock_release(&frame_table_lock);
	}
	ASSERT (frame != NULL);

	frame->page = NULL;
	lcg_state = lcg_state + 13523;
	// insert to frame table
	ASSERT (frame->page == NULL);
	// printf("FRAME PUSH\n");

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	// todo: 스택 크기를 증가시키기 위해 anon page를 하나 이상 할당하여 주어진 주소(addr)가 더 이상 예외 주소(faulted address)가 되지 않도록 합니다.
	// todo: 할당할 때 addr을 PGSIZE로 내림하여 처리
	vm_alloc_page(VM_ANON | STACK_MARKER, pg_round_down(addr), 1);
	lcg_state = lcg_state - 23;
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
                         bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
    struct page *page = NULL;

    if (addr == NULL)
        return false;

    if (is_kernel_vaddr(addr))
        return false;

    if (not_present) // 페이지 폴트가 발생하고, 물리 페이지가 없는 경우
    {
        void *rsp = user ? f->rsp : thread_current()->rsp;

        // 스택 확장 여부를 판단하는 조건문 간소화
        // if (rsp >= (USER_STACK - (1 << 20)) && addr <= USER_STACK && (rsp - 8 == addr || rsp == addr))
		if ((USER_STACK - (1 << 20) <= rsp - 8 && rsp - 8 == addr && addr <= USER_STACK) || (USER_STACK - (1 << 20) <= rsp && rsp <= addr && addr <= USER_STACK))
        {
            // 스택 확장을 위해 vm_stack_growth 호출
			// printf("%p is valid for stack growth\n", addr);
			// printf("CALL STACK GROWTH\n");
            vm_stack_growth(addr);
        }

        // 페이지가 보조 페이지 테이블에 존재하는지 확인
        page = spt_find_page(spt, addr);
        if (page == NULL) {
			// printf("CAN'T FIND PAGE!!\n");
		 	return false;}

        // 쓰기 가능한 페이지인지 확인
        if (write && !page->writable) {
			// printf("NOT WRITABLE!!\n");
			return false;}

		lcg_state = lcg_state + 3;
        // 페이지 클레임 수행
        return vm_do_claim_page(page);
    }
    return false; // 페이지 폴트가 아닌 경우
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	if ((page = spt_find_page(&thread_current()->spt, va))== NULL) return false; 
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;
	
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);
	// printf("current pml4 : %p\n", thread_current()->pml4);
	// printf("claim page ok. va : %p, kva : %p\n", page->va, frame->kva);
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, my_hash_func, my_hash_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	// TODO: 보조 페이지 테이블을 src에서 dst로 복사합니다.
	// TODO: src의 각 페이지를 순회하고 dst에 해당 entry의 사본을 만듭니다.
	// TODO: uninit page를 할당하고 그것을 즉시 claim해야 합니다.
	struct hash_iterator i;
	hash_first(&i, &src->spt_hash);
	while (hash_next(&i))
	{
		// src_page 정보
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = src_page->operations->type;
		void *upage = src_page->va;
		bool writable = src_page->writable;

		/* 1) type이 uninit이면 */
		if (type == VM_UNINIT)
		{ // uninit page 생성 & 초기화
			vm_initializer *init = src_page->uninit.init;
			void *aux = src_page->uninit.aux;
			vm_alloc_page_with_initializer(VM_ANON, upage, writable, init, aux);
			continue;
		}

		/* 2) type이 file이면 */
		if (type == VM_FILE)
		{
			struct file_page *file_aux = malloc(sizeof(struct file_page));
			file_aux->file = src_page->file.file;
			file_aux->ofs = src_page->file.ofs;
			file_aux->read_bytes = src_page->file.read_bytes;
			file_aux->zero_bytes = src_page->file.zero_bytes;
			if (!vm_alloc_page_with_initializer(type, upage, writable, NULL, file_aux))
				return false;
			struct page *page = spt_find_page(dst, upage);
			file_backed_initializer(page, type, NULL);
			page->frame = src_page->frame;
			pml4_set_page(thread_current()->pml4, page->va, src_page->frame->kva, src_page->writable);
			continue;
		}

		/* 3) type이 anon이면 */
		if (!vm_alloc_page(type, upage, writable)) // uninit page 생성 & 초기화
			return false;						   // init이랑 aux는 Lazy Loading에 필요. 지금 만드는 페이지는 기다리지 않고 바로 내용을 넣어줄 것이므로 필요 없음

		// vm_claim_page으로 요청해서 매핑 & 페이지 타입에 맞게 초기화
		if (!vm_claim_page(upage))
			return false;

		// 매핑된 프레임에 내용 로딩
		struct page *dst_page = spt_find_page(dst, upage);
		memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->spt_hash, hash_page_destroy);
}

void
supplemental_page_table_free (struct supplemental_page_table *spt ) {
	//TODO: Destroy all the supplemental_page_table hold by thread and
	 //TODO: writeback all the modified contents to the storage. 
	hash_destroy(&spt->spt_hash, hash_page_destroy);
}


uint64_t my_hash_func (const struct hash_elem *e, void *aux) {
	struct page *page = hash_entry(e, struct page, hash_elem);
	uint64_t va = page->va;
	return hash_int(va);
}
bool my_hash_less (const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	struct page *pagea = hash_entry(a, struct page, hash_elem);
	struct page *pageb = hash_entry(b, struct page, hash_elem);
	if (pagea->va < pageb -> va) return true;
	else return false;
}

void hash_page_destroy(struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	destroy(page);
	free(page);
}