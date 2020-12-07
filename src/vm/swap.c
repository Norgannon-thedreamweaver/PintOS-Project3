#include "vm/swap.h"

/* swap device. */
static struct block *swap_device;

/*bookkeeping of swap sectors */
static struct bitmap *swap_bitmap;

static struct lock swap_lock;

/* Number of sectors per page. */
#define PAGE_SECTORS 8

/* Sets up swap. */
void
swap_init (void)
{    
    lock_init (&swap_lock);
    swap_device = block_get_role (BLOCK_SWAP);
    if (swap_device == NULL){
      printf ("get swap device fail\n");
      swap_bitmap = bitmap_create (0);
    }
    else
      swap_bitmap = bitmap_create (block_size (swap_device)
                                   / PAGE_SECTORS);
    if (swap_bitmap == NULL)
      PANIC ("OOM allocating swap bitmap");
}   

/* Swaps in page P, which must have a locked frame
   (and be swapped out). */
void
swap_in (struct page *p)
{
    size_t i;

    ASSERT (p->frame != NULL);
    ASSERT (p->frame->thread==thread_current());
    ASSERT (p->sector != NULL);

    for (i=0;i<PAGE_SECTORS;i++){
        block_read (swap_device, p->sector + i,
                  p->frame->base + i * BLOCK_SECTOR_SIZE);
    }
    lock_acquire (&swap_lock);
    bitmap_reset (swap_bitmap, p->sector / PAGE_SECTORS);
    p->sector = NULL;
    lock_release (&swap_lock);
}

/* Swaps out page P, which must have a locked frame. */
bool
swap_out (struct page *p)
{
    size_t swap_sector;
    size_t i;

    ASSERT (p->frame != NULL);
    ASSERT (p->frame->thread==thread_current());

    lock_acquire (&swap_lock);
    swap_sector = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
    if (swap_sector == BITMAP_ERROR){
        lock_release (&swap_lock);
        return false;
    }

    p->sector = swap_sector * PAGE_SECTORS;

    for (i=0;i<PAGE_SECTORS;i++){
      block_write (swap_device, p->sector + i,
                   p->frame->base + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock);
    return true;
}
