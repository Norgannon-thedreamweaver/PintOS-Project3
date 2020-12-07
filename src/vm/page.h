#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

struct page {
    void *upage;                 /* User virtual address. */
    bool read_only;             /* Read-only page? */
    struct thread *thread;

    struct hash_elem hash_elem;
    
    struct frame *frame;        /* Page frame. */

    block_sector_t sector;       /* Starting sector of swap area, or -1. */
};

#endif