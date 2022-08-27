/**
 * @brief  stack to push/pop integer(s), API header  
 * @author yqhuang@uwaterloo.ca
 */
#include "../cURL_IPC/cURL_proc.h"
/* a stack that can hold integers */
/* Note this structure can be used by shared memory,
   since the items field points to the memory right after it.
   Hence the structure and the data items it holds are in one
   continuous chunk of memory.

   The memory layout:
   +===============+
   | size          | 4 bytes
   +---------------+
   | pos           | 4 bytes
   +---------------+
   | items         | 8 bytes
   +---------------+
   | items[0]      | 4 bytes
   +---------------+
   | items[1]      | 4 bytes
   +---------------+
   | ...           | 4 bytes
   +---------------+
   | items[size-1] | 4 bytes
   +===============+
*/

typedef struct int_stack
{
    int size;               /* the max capacity of the stack */
    int pos;                /* position of last item pushed onto the stack */
    RECV_BUF *items;             /* stack of buffers */
} ISTACK;

int sizeof_shm_stack(int size);
int init_shm_stack(ISTACK *p, int stack_size);
ISTACK *create_stack(int size);
void destroy_stack(ISTACK *p);
int is_full(ISTACK *p);
int is_empty(ISTACK *p);
int push(ISTACK *p, RECV_BUF item);
int pop(ISTACK *p, RECV_BUF *p_item);
