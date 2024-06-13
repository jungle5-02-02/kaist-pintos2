/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "vm/anon.h"
#include "threads/mmu.h"
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);
bool swap_table[10000];
/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	// swap_disk = NULL;

	swap_disk = disk_get(1, 1);
	lock_init(&swap_table_lock);
	int swap_size = disk_size(swap_disk)/8;
	// bool swap_table[swap_size];
	for (int i = 0; i < swap_size; i++) swap_table[i] = false;
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_index = -1;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	int swap_index = anon_page->swap_index;
	lock_acquire(&swap_table_lock);
	for (int ds = 0; ds < 8; ds++) {
		disk_read(swap_disk, swap_index*8 + ds, kva + ds*DISK_SECTOR_SIZE);
	}
	swap_table[swap_index] = false;
	anon_page->swap_index = -1;
	lock_release(&swap_table_lock);
	// printf("ANON SWAP IN\n");
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	ASSERT(page != NULL);
	struct anon_page *anon_page = &page->anon;
	lock_acquire(&swap_table_lock);
	for (int i = 0; i < disk_size(swap_disk)/8; i++) {
		if (swap_table[i]) continue;
		for (int ds = 0; ds < 8; ds++){
			disk_write(swap_disk, i*8+ds, page->va + ds*DISK_SECTOR_SIZE);
		}
		anon_page->swap_index = i;
		swap_table[i] = true;
		page->frame->page = NULL;
		page->frame = NULL;
		pml4_clear_page(thread_current()->pml4, page->va);
		lock_release(&swap_table_lock);
		return true;
	}
	lock_release(&swap_table_lock);
	return false;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if (anon_page->swap_index == -1) return;
	lock_acquire(&swap_table_lock);
	swap_table[anon_page->swap_index] = NULL;
	lock_release(&swap_table_lock);
	return;
}
