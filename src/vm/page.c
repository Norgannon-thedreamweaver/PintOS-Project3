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

/* Maximum size of process stack, in bytes. */
/* Right now it is 8 megabyte. */
#define STACK_PAGE_MAX 2048

/* Destroys a page when process exit*/
static void
destroy_page (struct hash_elem *p_, void *aux UNUSED)
{
  struct page *p = hash_entry (p_, struct page, hash_elem);
  frame_lock (p);
  if (p->frame)
    frame_free (p->frame);
  free (p);
}

/* Destroys page table when process exit */
void
destroy_pages (void)
{
  struct hash *h = thread_current ()->pages;
  if (h != NULL)
    hash_destroy (h, destroy_page);
}

/* find page corresponding to VADDR in a process's pages */
static struct page *
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
/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
static struct page *
page_for_addr (const void *address)
{
  if (address < PHYS_BASE)
    {
      struct page p;
      struct hash_elem *e;

      /* Find existing page. */
      p.addr = (void *) pg_round_down (address);
      e = hash_find (thread_current ()->pages, &p.hash_elem);
      if (e != NULL)
        return hash_entry (e, struct page, hash_elem);

      /* -We need to determine if the program is attempting to access the stack.
         -First, we ensure that the address is not beyond the bounds of the stack space (1 MB in this
          case).
         -As long as the user is attempting to acsess an address within 32 bytes (determined by the space
          needed for a PUSHA command) of the stack pointers, we assume that the address is valid. In that
          case, we should allocate one more stack page accordingly.
      */
      if ((p.addr > PHYS_BASE - STACK_MAX) && ((void *)thread_current()->user_esp - 32 < address))
      {
        return page_allocate (p.addr, false);
      }
    }

  return NULL;
}


struct page *
page_alloc (void *vaddr, bool read_only)
{
  struct thread *t = thread_current ();
  struct page *p = malloc (sizeof(struct page));
  if(p == NULL)
        PANIC ("OOM allocating page table");
  
  p->upage = pg_round_down (vaddr);
  p->read_only = read_only;
  p->thread = t;
  p->frame = NULL;
  p->sector = NULL;
  
  if (hash_insert (t->pages, &p->hash_elem) != NULL){
    /* Already mapped. */
    free (p);
    p = NULL;
  }
  return p;
}

/* Evicts the page containing address VADDR
   and removes it from the page table. */
void
page_deallocate (void *vaddr)
{
  struct page *p = page_for_addr (vaddr);
  ASSERT (p != NULL);
  frame_lock (p);
  if (p->frame)
    {
      struct frame *f = p->frame;
      if (p->file && !p->private)
        page_out (p);
      frame_free (f);
    }
  hash_delete (thread_current ()->pages, &p->hash_elem);
  free (p);
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
static bool
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
static bool
uninstall_page (void *vaddr)
{
  void* upage=pg_round_down(vaddr);
  struct thread *t = thread_current ();

  if(pagedir_get_page (t->pagedir, upage) == NULL)
    return false;
  
  pagedir_clear_page(t->pagedir, upage);
  return true;
}