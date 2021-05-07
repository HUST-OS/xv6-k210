#ifndef __USRMM_H
#define __USRMM_H

#include "riscv.h"
#include "types.h"

enum segtype { NONE, LOAD, TEXT, DATA, BSS, HEAP, MMAP, STACK };

struct seg{
  enum segtype type;
  uint64 addr;
  uint64 sz;
  struct seg *next;
};

/**
 * @brief create a new segment, returning -1 on failure
 * 
 * @param[in] pagetable the pagetable where the segment stays
 * @param[in] head      the head pointer of the linked list
 * @param[in] type      the type of the segment
 * @param[in] offset    the offset of the segment
 * @param[in] sz        the size of the segment
 * @param[in] flag      flag to signal the mode of the segment (W? R? X?)
 * 
 * @return    0 on success, -1 on failure
 */
int newseg(pagetable_t pagetable, struct seg *head, enum segtype type, uint64 offset, uint64 sz,long flag);

/**
 * @brief grasp the type of the segment to which a certain virtual address belongs
 * 
 * @param[in] head      the head pointer of the linked list
 * @param[in] addr      the addr
 * 
 * @return    the type
 */
enum segtype typeofseg(struct seg *head, uint64 addr);

/**
 * @brief free the memory referenced by pre and set the sz to 0
 * 
 * @param[in] pagetable the pagetable where the segment stays
 * @param[in] p         the pointer of the segment record
 */
void freeseg(pagetable_t pagetable, struct seg *p);

/**
 * @brief delete the segment referenced by pre->next if pre->next!=NULL otherwise pre
 * 
 * @param[in] pagetable the pagetable where the segment stays
 * @param[in] pre       the prior pointer of the node intended to delete if not the head
 * 
 * @return pre if pre is not head, NULL if pre is head
 */
struct seg *delseg(pagetable_t pagetable_t, struct seg *pre);

#endif