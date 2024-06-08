/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
// 1.9 do_claim_page 함수 구현
#include "threads/mmu.h"
// 2.5 supplemental_page_table_copy 함수 구현
#include "threads/vaddr.h"
// 1.8 frame_list list 선언
#include <list.h>

// 1.8 frame_list list 선언
struct list frame_list;

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
	
	// 2.1 vm_alloc_page_with_initializer 구현
	
	if (spt_find_page (spt, upage) == NULL) { // upage가 이미 사용중인 주소인지 확인
		
		struct page *page = malloc(sizeof(struct page));// 새로운 페이지 구조체를 동적으로 할당

		if (page == NULL)
			goto err;

		
		int ty = VM_TYPE (type);
		bool (*initializer)(struct page *, enum vm_type, void *); // 가상 메모리 타입에 따라 initializer 함수 포인터를 설정
		
		switch(ty){
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}
		
        uninit_new(page, upage, init, type, aux, initializer); // 페이지 초기화
		page->writable = writable;
		page->swap =false;
		 
        if (!spt_insert_page(spt, page)) { // 페이지를 spt에 삽입
            free(page); // 삽입 실패시 page할당 해제
            goto err; // 오류처리
        }

        return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	// 1.6 spt_find_page 함수 구현
	
	struct page* target_page= malloc(sizeof(struct page)); // 검색에 사용할 임시 구조체 할당
	
	
	va = pg_round_down(va); // 가상 주소를 페이지 경계로 내림 정렬
	
	target_page->va = va; // 임시 구조체의 va 속성을 페이지의 va로 설정

	
	struct hash_elem *elem = hash_find(&spt->pages, &target_page->hash_elem); // spt에서 find

	
	free(target_page); // 임시 구조체 메모리 해제
	
	if (elem != NULL) {
		struct page *found_page = hash_entry(elem, struct page, hash_elem);
		return found_page; // 페이지 find 성공시 해당 페이지 반환
	}
	else {
		
        return NULL; // 페이지 find 실패시 NULL 반환
    }

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	// 1.7 spt_insert_page 함수 구현

	struct hash_elem *elem = hash_insert(&spt->pages, &page->hash_elem); // hash_insert함수는 성공하면 NULL반환
	
	if (elem == NULL)
	
		succ = true;


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
	
	// 1.8 vm_get_frame 함수 구현
	struct frame *frame = malloc(sizeof(struct frame)); // 물리 프레임 구조체 생성, 메모리 할당
    if (frame == NULL) {
        return NULL; // 할당 실패 시 NULL 반환
    }
	
    void *kva = palloc_get_page(PAL_USER | PAL_ZERO); // 가상 페이지 주소 할당
    if (kva == NULL) {
		free(frame);		
		frame = vm_evict_frame(); // 가상 주소가 모두 사용중인 경우 evict
    }else{
		frame->kva = kva;
	}

	frame->page = NULL;
	list_push_back(&frame_list,&frame->frame_elem);
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
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

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
	// 1.10 vm_claim_page 함수 구현
	
	struct supplemental_page_table *spt = &thread_current()->spt; // 현재 스레드에서 spt 로드
	struct page *page = spt_find_page(spt, va); // spt에서 va에 해당하는 페이지 찾기

    if (page == NULL) { // spt 안에 va에 해당하는 페이지가 없는 경우
		page = malloc(sizeof(struct page)); // malloc으로 할당 시도
		
		if(page==NULL){ 
			return false; // malloc 할당 실패시 처리
		}

		page -> va = va; // page의 va 값 설정
		
        spt_insert_page(spt,page); // page 구조체를 spt에 추가
    }

	return vm_do_claim_page (page); // 물리프레임 클레임요청
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	// 1.9 vm_do_claim_page 함수 구현
	struct thread *t = thread_current ();
	
	/* 프레임 구조체, 페이지 구조체 사이의 연결 (set links) */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	void *page_va = page->va; // 페이지 가상주소
    void *frame_pa = frame->kva; // 프레임 물리주소
	
    
    if (pml4_get_page(t->pml4, page_va) == NULL ){ // 현재 스레드의 페이지 테이블에서 페이지 VA 매핑여부 확인
		// 매핑이 안돼있으면 매핑
        if(pml4_set_page(t->pml4, page_va, frame_pa, page->writable)) {
        	// 매핑 성공시, (디스크로부터) 페이지를 프레임으로 swap_in .
			
        	return swap_in(page, frame_pa);
		}
    }
	
    return false;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	// 1.3 supplemental_page_table_init 함수 구현
	hash_init(&spt->pages, hash_func, less_func, NULL);
}

// 1.4 hash_func 함수 구현
uint64_t hash_func(const struct hash_elem *e, void *aux) {
    const struct page *pg = hash_entry(e, struct page, hash_elem);
    return hash_bytes(&pg->va, sizeof(void*));
}

// 1.5 less_func 함수 구현
bool less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
    const struct page *pg_a = hash_entry(a, struct page, hash_elem);
    const struct page *pg_b = hash_entry(b, struct page, hash_elem);
    return pg_a->va < pg_b->va;
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
    // 2.5 supplemental_page_table_copy 함수 구현
	struct hash_iterator i;
    struct hash *src_hash = &src->pages;
    struct hash *dst_hash = &dst->pages;

    hash_first(&i, src_hash);
    while (hash_next(&i)) {
        struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);   
        // Allocate and claim the page in dst
		enum vm_type type = src_page->operations->type;
		if(type== VM_UNINIT){
			struct uninit_page *uninit_page = &src_page->uninit;
			struct file_loader* file_loader = (struct file_loader*)uninit_page->aux;
			struct file_loader* new_file_loader = malloc(sizeof(struct file_loader));
			memcpy(new_file_loader, file_loader, sizeof(struct file_loader));
			vm_alloc_page_with_initializer(uninit_page->type,src_page->va,src_page->writable,uninit_page->init,new_file_loader);
		}else{
        	vm_alloc_page(src_page->operations->type, src_page->va, true);
        	vm_claim_page(src_page->va);
        	memcpy(src_page->va, src_page->frame->kva, PGSIZE);
		}
        
    }
    return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// 2.6 supplemental_page_table_kill 함수 구현
	hash_clear(&spt->pages, hash_action_destroy);
}

// 2.6 hash_action_destroy 함수 구현
void hash_action_destroy(struct hash_elem* hash_elem_, void *aux){
	struct page* page = hash_entry(hash_elem_, struct page, hash_elem);

	if(page!=NULL){
		if (VM_TYPE(page->operations->type) == VM_FILE && !page->swap) {
        	struct file_page *file_page = &page->file;
			struct file* file = file_page->file; // 파일 포인터 갱신
			//!!TODO: dirty bit check
			if(file)
				file_write_at(file, page->frame->kva, file_page->read_bytes, file_page->ofs);
		}
		//!!TODO: file close
		if(page->frame != NULL){
			free_frame(page->frame);
			page->frame = NULL;
		}
	   	vm_dealloc_page(page);
	}
}

// 2.3 free_frame 함수 구현
void free_frame(struct frame* frame){
	list_remove(&frame->frame_elem);
	pml4_clear_page(thread_current()->pml4, frame->page->va);
	palloc_free_page(frame->kva);
	free(frame);
}