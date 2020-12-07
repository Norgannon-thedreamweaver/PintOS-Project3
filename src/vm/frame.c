#include "vm/frame.h"

struct list frames;
struct list_elem* frames_ptr;

struct lock frame_lock;
void
frame_init () 
{
    list_init(&frames);
    lock_init (&frame_lock);
    frames_ptr=NULL;
}

struct frame *
frame_alloc(){
    struct frame *f = (struct frame *)malloc (sizeof(struct frame));
    if(f == NULL)
        PANIC ("OOM allocating frame table");
    f->base=NULL;
    f->page=NULL;
    f->thread=thread_current();
    lock_acquire(&frame_lock);
    if(list_empty(&frames))
        frames_ptr=&f->elem;
    list_push_back(&frames,&f->elem);
    if(frames_ptr==NULL || frames_ptr==list_end (&frames))
        frames_ptr=list_begin(&frames);
    lock_release(&frame_lock);
    return f;
}

void
frame_free(struct frame * f){
    lock_acquire(&frame_lock);
    if(frames_ptr==&f->elem)
        frames_ptr=list_next (&f->elem);
    list_remove(&f->elem);
    if(list_empty(&frames))
        frames_ptr=NULL;
    else if(frames_ptr==list_end (&frames))
        frames_ptr=list_begin(&frames);
    free(f);
    lock_release(&frame_lock);
}

struct frame *
frame_find_victim(){
    lock_acquire (&frame_lock);

    struct list_elem* e;
    struct frame *f;
    bool access,dirty;
    size_t len=list_size(&frames);
    if(len==0 || frames_ptr==NULL)
        return NULL;
    size_t i;
    int j;
    for(j=0;j<2;j++){
        for(i=0,e=frames_ptr;i<len;e=list_next (e)){
            if(e == list_end (&frames))
                e = list_begin (&frames);
    
            f=list_entry(e,struct frame,elem);
            access=pagedir_is_accessed (f->thread->pagedir, f->page->upage);
            dirty=pagedir_is_dirty(f->thread->pagedir, f->page->upage);
    
            if(!access && !dirty){
                frames_ptr=list_next (frames_ptr);
                list_remove(e);
                if(frames_ptr==list_end (&frames))
                    frames_ptr=list_begin(&frames);

                lock_release(&frame_lock);
                return f;
            }
        }
    
        for(i=0,e=frames_ptr;i<len;e=list_next (e)){
            if(e == list_end (&frames))
                e = list_begin (&frames);
    
            f=list_entry(e,struct frame,elem);
            access=pagedir_is_accessed (f->thread->pagedir, f->page->upage);
            dirty=pagedir_is_dirty(f->thread->pagedir, f->page->upage);
    
            if(!access && dirty){
                frames_ptr=list_next (frames_ptr);
                list_remove(e);
                if(frames_ptr==list_end (&frames))
                    frames_ptr=list_begin(&frames);

                lock_release(&frame_lock);
                return f;
            }
            else{
                pagedir_set_accessed(f->thread->pagedir, f->page->upage,false);
            }
        }
    }
    lock_release(&frame_lock);
    return  NULL;
}