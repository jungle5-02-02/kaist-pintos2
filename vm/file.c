/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/mmu.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
	struct file_page *load_info = &page->uninit.aux;
	file_page->file = load_info->file;
	file_page->ofs = load_info->ofs;
	file_page->read_bytes = load_info->read_bytes;
	file_page->zero_bytes = load_info->zero_bytes;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	// return lazy_load_segment(page, file_page);
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if (pml4_is_dirty(thread_current()->pml4, page->va)) {
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
		// pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	if (addr < 0x1000) {printf("invalid addr!\n"); return;}
	if (length == 0) {printf("Zero File Length!\n"); return;}
	void *given_addr = addr;
	struct file *open_file = file_open(file);
	size_t file_read_bytes = (length > file_length(open_file) ? file_length(open_file) : length);
	size_t zero_bytes = PGSIZE - (file_read_bytes%PGSIZE);
	// if (!load_segment(open_file, offset, addr, file_read_bytes, zero_bytes, writable)) return;
	
	/**/
	while (file_read_bytes > 0) {
		struct file_page *load_info = malloc(sizeof(struct file_page));
		size_t read_bytes;
		load_info->file = open_file;
		load_info->ofs = offset;
		if (file_read_bytes >= PGSIZE) {
			load_info->read_bytes = PGSIZE;
			load_info->zero_bytes = 0;
		}
		else {
			load_info->read_bytes = file_read_bytes;
			load_info->zero_bytes = zero_bytes;
		}

		if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, load_info)) {printf("vm_file alloc failed!\n"); return;}

		struct page *page = spt_find_page(&thread_current()->spt, given_addr);

		file_read_bytes -= PGSIZE;
		addr += PGSIZE;
		offset += PGSIZE;
	}
	/**/
	return given_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page* page = spt_find_page(spt, addr);
	if(!page) {printf("can't find page for unmap!\n"); return;}
	for (int i = 0; i < hash_size(&spt->spt_hash); i++) {
		destroy(page);
		addr += PGSIZE;
		page = spt_find_page(spt, addr);
	}
}
