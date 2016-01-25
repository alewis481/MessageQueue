#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/klist.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <asm/uaccess.h>


#define MAX_SIZE 512

struct message {
  char data[MAX_SIZE];
  long length;
  struct list_head list;
};

//used for locking in calls
DEFINE_SEMAPHORE(queueLock421);
LIST_HEAD(queueHead421);
int numElements421 = 0;

/*
sys_enqueue421

Adds a new item to the tail of the queue. The parameter len is the number of 
bytes you should copy starting at the data pointer. If there is insufficient 
memory to add the item to the queue or to store the data, return -ENOMEM. If 
there is an error accessing the user-space pointer to copy data, return -EFAULT.
 If the len parameter is negative, return -EINVAL. If the len parameter exceeds 
512, return -E2BIG. Return 0 on success. On any error, ensure that you do not 
modify any data in the queue.*/
asmlinkage long sys_enqueue421(const void __user *data, long len) {
  struct message *newMsg;
  long error;


  if(len < 0) 
    return -EINVAL;
  else if (len > MAX_SIZE) 
    return -E2BIG;

  //checks if address is valid
  if(!access_ok(VERIFY_READ, data, len)){
    return -EFAULT;
  }
  //allocate memory for new message
  newMsg = (struct message *)kmalloc(sizeof(struct message), GFP_KERNEL); 
  //if null, message failed to write
  if(newMsg == NULL)
    return -ENOMEM;

  INIT_LIST_HEAD(&newMsg->list);  
  newMsg->length = len;

  //verify that copy did not have any leftover bytes
  error = copy_from_user(newMsg->data, data, len);
  if(error) {
    //free memory from message not to be added to queue
    kfree(newMsg);
    return -EFAULT;
  }

  //critical section so lock
  down(&queueLock421);
  list_add_tail(&newMsg->list, &queueHead421);
  ++numElements421;
  up(&queueLock421);

  return 0;
}


/*
sys_dequeue421

Removes an item from the head of the queue, storing the data back to 
user-space. If the queue is empty, return -ENODATA. If there is an error 
accessing the user-space pointer to copy data, return -EFAULT. If the len 
parameter is negative, return -EINVAL. If the len parameter indicates that the 
space allocated in user-space for data is not large enough to store the entire 
message, return -E2BIG. Return 0 on success. On any error, ensure that you do 
not modify any data in the queue. The parameter len indicates an upper limit on 
how many bytes of space the user has allocated starting at the pointer data. If 
len bytes is greater than the size of the element at the head of the queue, 
simply copy the number of bytes that the head element contains, and ignore any 
extra space provided.*/
asmlinkage long sys_dequeue421(void __user *data, long len) {
  long uBound;
  long error;
  struct message * tmp;


  //LOCK HERE
  down(&queueLock421);
  if (list_empty(&queueHead421)) {
    //unlock before returning
    up(&queueLock421);
    return -ENODATA;
  }
  up(&queueLock421);
  //UNLOCK HERE

  //memory access error, return efault
  if(!access_ok(VERIFY_WRITE, data, len)) 
   return -EFAULT;
   

  if(len < 0) 
    return -EINVAL;
  else if(len > MAX_SIZE) 
    return -E2BIG;
  
  
  
  //LOCK HERE
  down(&queueLock421);
  tmp = list_entry(queueHead421.next, struct message, list);

  if(tmp->length > len) 
    return -E2BIG;
  //if length of data is larger than len parameter
  uBound = tmp->length < len ? tmp->length : len;

  //upper bound is message length if it is smaller
  error = copy_to_user(data, tmp->data, uBound);

  //error occurs if not all bytes are transferred
  if (error) { 
    up(&queueLock421);
    return -EFAULT;
  }
  
  list_del(queueHead421.next);
  kfree(tmp);

  --numElements421;
  //free lock
  up(&queueLock421);

  return 0;
}


/*
sys_peekLen421

Retrieves the length of the element at the head of the queue. If the queue is 
empty, return -ENODATA. On success, return the size of the data in the element 
at the head of the queue, in bytes. */
asmlinkage long sys_peekLen421(void) {
  struct message * tmp;
  int length;

  //set lock while accesing queue
  down(&queueLock421);

  if (list_empty(&queueHead421)) {
    up(&queueLock421);
    return -ENODATA;
  }
  tmp = list_entry(queueHead421.next, struct message, list);
  length = tmp->length;

  up(&queueLock421);

  return length;
}

/*
sys_queueLen421

Retrieves the number of elements currently in the queue. On success, return 
the number of elements in the queue. */
asmlinkage long sys_queueLen421(void) {
  return numElements421;
}




/*Deletes all of the messages currently in the queue, freeing all allocated 
memory and releasing all elements stored within. On success, return 0. */
asmlinkage long sys_clearQueue421(void) {
  struct list_head *pos, *n;
  struct message *tmp;
  //start lock
  down(&queueLock421);

  if (list_empty(&queueHead421)) {
    up(&queueLock421);
    return 0;
  }
  //delete and free memory for all items in queue
  list_for_each_safe(pos, n, &queueHead421) {
    tmp = list_entry(pos, struct message, list);
    list_del(pos);
    kfree(tmp);
    --numElements421;
  }
  //end lock
  up(&queueLock421);

  return 0;
}
