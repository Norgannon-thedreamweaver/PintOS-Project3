#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"
#include "userprog/syscall.h"

struct page {
    void *upage;                 /* User virtual address. */
    bool writable;             /* Read-only page? */
    struct thread *thread;

    struct hash_elem hash_elem;
    
    struct frame *frame;        /* Page frame. */

    block_sector_t sector;       /* Starting sector of swap area, or -1. */

    struct file* file;          /* File. */
    off_t file_offset;          /* Offset in file. */
    off_t file_bytes;           /* Bytes to read/write, 1...PGSIZE. */

    bool writeback;
};

void destroy_page (struct hash_elem *p_, void *aux);
void destroy_pages (struct thread *t);
struct page* find_page_by_vaddr (const void *vaddr);
struct page* page_alloc (void *vaddr, bool writable);
void page_free(void *vaddr);
bool page_fault_handler(void *fault_addr);
bool new_page_alloc (void *fault_addr) ;
bool page_swap_in(struct page *p);
void page_swap_out(struct page *p);
void page_swap_out_clock(void);

unsigned page_hash (const struct hash_elem *e, void *aux);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,void *aux);
bool install_page (void *upage, void *kpage, bool writable);
bool uninstall_page (void *vaddr);
#endif