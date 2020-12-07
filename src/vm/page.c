#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

/* Maximum pages of stack, in bytes. */
/* Right now it is 8 megabyte. */
#define STACK_PAGE_MAX 2048

/* Destroys a page when process exit*/
void
destroy_page (struct hash_elem *p_, void *aux)
{
  struct page *p = hash_entry (p_, struct page, hash_elem);
  frame_lock (p);
  if (p->frame)
    frame_free (p->frame);
  if(p->sector!=NO_SECTOR){
    reset_swap_bitmap(p->sector);
  }
  free (p);
}

/* Destroys page table when process exit */
void
destroy_pages (struct thread* t)
{
  struct hash *h = t->pages;
  if (h != NULL)
    hash_destroy (h, destroy_page);
}

/* find page corresponding to VADDR in a process's pages */
struct page *
find_page_by_vaddr (const void *vaddr){
  if (!is_user_vaddr(vaddr))
    return NULL;

  struct page p;
  struct hash_elem *elem;

  /* Find existing page. */
  p.upage = (void *) pg_round_down (vaddr);
  elem = hash_find (thread_current ()->pages, &p.hash_elem);
  if (elem != NULL)
    return hash_entry (elem, struct page, hash_elem);
  return NULL;
}

struct page *
page_alloc (void *vaddr, bool writable)
{
  struct thread *t = thread_current ();
  struct page *p = malloc (sizeof(struct page));
  if(p == NULL)
    PANIC ("OOM allocating page table");
  
  p->upage = pg_round_down (vaddr);
  p->writable=writable;
  p->thread = t;
  p->frame = NULL;
  p->sector = NO_SECTOR;
  
  if (hash_insert (t->pages, &p->hash_elem) != NULL){
    /* Already mapped. */
    free (p);
    p = NULL;
  }
  return p;
}


bool
new_page_alloc (void *fault_addr) {
  void* kpage=palloc_get_page(PAL_USER|PAL_ZERO);
  while(kpage==NULL){
    page_swap_out_clock();
    kpage=palloc_get_page(PAL_USER|PAL_ZERO);
  }

  struct page* p=page_alloc(fault_addr,true);
  struct frame *f=frame_alloc();
  f->base=kpage;
  f->page=p;
  p->frame=f;
  return install_page(p->upage,kpage,p->writable);
}


bool
page_swap_in(struct page *p){
  void* kpage=palloc_get_page(PAL_USER|PAL_ZERO);
  while(kpage==NULL){
    page_swap_out_clock();
    kpage=palloc_get_page(PAL_USER|PAL_ZERO);
  }
  if(kpage!=NULL){
    struct frame* f=frame_alloc();
    f->base=kpage;
    f->page=p;
    p->frame=p;
    install_page(p->upage,kpage,p->writable);
    swap_in(p);
    return true;
  }
  else{
    printf("NO FRAME WHEN PAGE SWAP IN");
    return false;
  }
}

bool
page_fault_handler(void *fault_addr){
  if(PHYS_BASE-STACK_PAGE_MAX*PGSIZE<pg_round_down(fault_addr))
    return false;
  if (thread_current ()->pages == NULL)
    return false;

  struct page *p=find_page_by_vaddr(fault_addr);

  if (p == NULL){
    return new_page_alloc(fault_addr);
  }
  else if(p->sector!=NO_SECTOR){
    return page_swap_in(p);
  }
  else{
    return false;
  }
}

void
page_swap_out(struct page *p){
  ASSERT (p->frame != NULL);
  ASSERT (p->frame->thread==thread_current());

  bool dirty = pagedir_is_dirty (p->thread->pagedir, p->upage);

  uninstall_page(p->upage);

  if(swap_out(p)){
    palloc_free_page (p->upage);
    frame_free(p->frame);
    p->frame=NULL;
  }
  else
    PANIC("NO SWAP BLOCK");
}

void
page_swap_out_clock(){
  struct frame* victim=frame_find_victim();
  if(victim==NULL)
    PANIC("NO VICTIM");
  page_swap_out(victim->page);
}


/* Returns a hash value for the page that E refers to. */
unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, hash_elem);
  return ((unsigned) p->upage) >> PGBITS;
}

bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->upage < b->upage;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/* delete a mapping from user virtual address UPAGE
*/
bool
uninstall_page (void *vaddr)
{
  void* upage=pg_round_down(vaddr);
  struct thread *t = thread_current ();

  if(pagedir_get_page (t->pagedir, upage) == NULL)
    return false;
  
  pagedir_clear_page(t->pagedir, upage);
  return true;
}