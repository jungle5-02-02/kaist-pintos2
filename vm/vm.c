/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

#include "kernel/hash.h"
#include "userprog/process.h"
#include <string.h>
#include "vm/file.h"
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	
	// Frame table init
	hash_init(&frame_table.ft_hash, my_hash_func, my_hash_less, NULL);
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
	
	// struct page binpage;
	// binpage.va = pg_round_down(va); // pgrounddown은 뒤 12자리를 절삭함으로써 가상주소를 페이지 번호로 바꿔줌
	// struct hash_elem e;
	// binpage.hash_elem = e;
	// printf("binpage malloc ");
	struct page *binpage = malloc(sizeof(page));
	// printf("ok\n");
	binpage->va = pg_round_down(va);
	struct hash_elem *elem = hash_find(&spt->spt_hash, &binpage->hash_elem);
	free(binpage);
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

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
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
		PANIC("TODO: Eviction!!!!!!!!!!!!!!!!!!!!!!!!");
	} 

	frame = (struct frame *) calloc(1, sizeof(struct frame));
	frame->kva = kva;
	frame->page = NULL;

	// insert to frame table
	// lock_acquire(&frame_table_lock);
	// hash_insert(&frame_table.ft_hash, &frame->frame_hash_elem);
	// lock_release(&frame_table_lock);

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	addr = pg_round_down(addr);
	// printf("given addr is %p\n",addr);
	if (addr == NULL) return false;
	if (is_kernel_vaddr(addr)) {
		// printf("is kernel addr!\n");
		return false;}
	page = spt_find_page(spt, addr);
	if (page == NULL) {
		// printf("can't find page\n");
		return false;}
	////////// writeable check?? //////////
	// if (write) return false;
	///////////////////////////////////////
	////////// user??? ///////////

	///////////////////////////////////////
	//stack growth on demand (call vm_stack_growth)
	return vm_do_claim_page (page);
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
	if(!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)) PANIC(printf("pml4 set failed!!!!\n"));
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
	struct hash *src_hash = &src->spt_hash;
	ASSERT(!hash_empty(src_hash));
	struct hash_iterator h_iter;
	hash_first(&h_iter, src_hash);

	while (hash_next(&h_iter)) {
		struct page *srcpage = hash_entry(hash_cur(&h_iter), struct page, hash_elem);
		void *va = srcpage->va;
		enum vm_type type = srcpage->operations->type;
		bool writable = srcpage->writable;
		switch (VM_TYPE(type))
		{
		case VM_UNINIT :
		{
			vm_initializer *init = srcpage->uninit.init;
			void *aux = srcpage->uninit.aux;
			if(!vm_alloc_page_with_initializer(VM_ANON, va, writable, init, aux)) {printf("child VM UNINIT fail\n"); return false;}
			break;
		}
		case VM_ANON : 
		{
			if(!vm_alloc_page_with_initializer(type, va, writable, NULL, NULL)) {printf("child VM ANON fail\n"); return false;}
			if(!vm_claim_page(va)) {printf("ANON claim fail\n"); return false;}
			memcpy(spt_find_page(dst, va)->frame->kva, srcpage->frame->kva, PGSIZE);
			break;
		}
		case VM_FILE : 
		{
			// vm_initializer *init = file_backed_initializer;
			struct file_page srcaux = srcpage->file;
			struct file_page *file_aux = (struct file_page *) malloc(sizeof(struct file_page));
			file_aux->file = srcaux.file;
			file_aux->ofs = srcaux.ofs;
			file_aux->read_bytes = srcaux.read_bytes;
			file_aux->zero_bytes = srcaux.zero_bytes;

			if(!vm_alloc_page_with_initializer(type, va, writable, NULL, file_aux)) {printf("child VM FILE fail\n"); return false;}
			struct page *newpage = spt_find_page(dst, va);
			ASSERT(newpage->va == va);
			file_backed_initializer(newpage, type, NULL);
			newpage->frame = srcpage->frame;
			// printf("current pml4 : %p\n", thread_current()->pml4);
			if(!pml4_set_page(thread_current()->pml4, va, srcpage->frame->kva, srcpage->writable)) {printf("pml4 fail\n"); return false;}
			break;
		}
		default:
			PANIC(printf("no type???"));
			break;
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// ASSERT(!hash_empty(&spt->spt_hash));
	// printf("killing spt\n");
	hash_clear(&spt->spt_hash, hash_page_kill);
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

void hash_page_kill (struct hash_elem *elem, void *aux) {
	struct page *page = hash_entry(elem, struct page, hash_elem);
	destroy(page);
	free(page);
}