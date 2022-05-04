/**
 * @file queue.c
 * @brief Implementation of a queue that supports FIFO and LIFO operations.
 *
 * This queue implementation uses a singly-linked list to represent the
 * queue elements. Each queue element stores a string value.
 *
 * Assignment for basic C skills diagnostic.
 * Developed for courses 15-213/18-213/15-513 by R. E. Bryant, 2017
 * Extended to store strings, 2018
 *
 * TODO: fill in your name and Andrew ID
 * @author XXX <XXX@andrew.cmu.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "queue.h"

/**
 * @brief Allocates a new queue
 * @return The new queue, or NULL if memory allocation failed
 */
queue_t *queue_new(void) {
    queue_t *q = malloc(sizeof(queue_t));
    /* What if malloc returned NULL? */
    if (!q) {
        return NULL;
    } else { // if malloc success
        q->head = NULL;
        q->tail = NULL;
        q->num_ele = 0;
        return (q);
    }
}

/**
 * @brief Frees all memory used by a queue
 * @param[in] q The queue to free
 */
void queue_free(queue_t *q) {
    /* How about freeing the list elements and the strings? */
    /* Free queue structure */
    // we first check if the malloc of q fails
    if (!q) {
        return;
    } 
   
    else {
        
        list_ele_t *p = q->head; // get the first node in linkedList
        list_ele_t *cur;
        /* while (p != NULL) { // free list element
            cur = p;
            p = p->next;
            free(cur->value); // also free the  space used by the string
            free(cur);        // free the list element structure
        }
        free(q)*/ // do not forget to free the memory allocated for queue_t at
                 // the last

        while(p != NULL)
        {
            if(p->value != NULL)
            {
            cur = p->next;
            free(p->value); // removing the array of characters at the value of node
            }
            free(p); // removing the node itself
            p = cur;
        }
         free(q);
    }
}

/**
 * @brief Attempts to insert an element at head of a queue
 *
 * This function explicitly allocates space to create a copy of `s`.
 * The inserted element points to a copy of `s`, instead of `s` itself.
 *
 * @param[in] q The queue to insert into
 * @param[in] s String to be copied and inserted into the queue
 *
 * @return true if insertion was successful
 * @return false if q is NULL, or memory allocation failed
 */
bool queue_insert_head(queue_t *q, const char *s) {
    list_ele_t *newh;
    char *strptr;
    /* What should you do if the q is NULL? */
    if (!q) {
        return false;
    }
    newh = (list_ele_t *)malloc(sizeof(list_ele_t)); // typecasting and creat a malloc space for list element
    strptr = malloc((strlen(s) + 1) *sizeof(char)); // create malloc space for the string and include the null zero;
    /* if either call to malloc returns NULL? function return false */
    // free the allocated space
    if (!newh || !strptr) {
        if (!newh) {
            return false;
        } else if (newh != NULL && !strptr) {
            free(newh);
        }

        return false;
    } else {
        // check if the linked list has no element
        if (q->num_ele != 0) // Queue not NULL
        {
            newh->next = q->head;
            strncpy(strptr, s, strlen(s)+1);
            newh->value = strptr;
            q->head = newh;
            q->num_ele += 1;
           
          
            return true;
        } else if (q->num_ele == 0) // Queue is NULL
        {
            strncpy(strptr, s, strlen(s)+1);
            newh->value = strptr;
            q->head = newh;
            q->tail = newh;
            newh->next = NULL;
            q->num_ele += 1; // update the list length
            return true;
        }
    }
    return false;
}

/**
 * @brief Attempts to insert an element at tail of a queue
 *
 * This function explicitly allocates space to create a copy of `s`.
 * The inserted element points to a copy of `s`, instead of `s` itself?
 *
 * @param[in] q The queue to insert into
 * @param[in] s String to be copied and inserted into the queue
 *
 * @return true if insertion was successful
 * @return false if q is NULL, or memory allocation failed
 */
bool queue_insert_tail(queue_t *q, const char *s) {
    /* You need to write the complete code for this function */
    /* Remember: It should operate in O(1) time */
    list_ele_t *newh;
    char *strptr;
    if (!q) {
        return false;
    }
    newh = (list_ele_t *)malloc(sizeof(list_ele_t)); // typecasting and creat a malloc space for list element
    strptr = malloc((strlen(s) + 1) *sizeof(char)); // create malloc space for the string and include the null zero;
    /* if either call to malloc returns NULL? function return false */
    if (!newh || !strptr) {
        if (!newh) {
            return false;
        } else if (newh != NULL && !strptr) {
            free(newh);
        }

        return false;
    } else {
        // check if the linked list has no element
        if (q->num_ele != 0) // if Queue not NULL
        {
            strncpy(strptr, s, strlen(s)+1);
            newh->value = strptr;
            q->tail->next = newh;
            newh->next = NULL; // add as the last element
            q->num_ele += 1;
            q->tail = newh; // update the tail
            return true;
        } else if (q->num_ele == 0) // if Queue is NULL
        {
            // procedure is the same as we insert at the head if the queue is
            // NULL!
            strncpy(strptr, s, strlen(s)+1);
            newh->value = strptr;
            q->head = newh;
            q->tail = newh;
            newh->next = NULL;
            q->num_ele += 1; // update the list length
            return true;
        }
    }
    return false;
}

/**
 * @brief Attempts to remove an element from head of a queue
 *
 * If removal succeeds, this function frees all memory used by the
 * removed list element and its string value before returning.
 *
 * If removal succeeds and `buf` is non-NULL, this function copies up to
 * `bufsize - 1` characters from the removed string into `buf`, and writes
 * a null terminator '\0' after the copied string.
 *
 * @param[in]  q       The queue to remove from
 * @param[out] buf     Output buffer to write a string value into
 * @param[in]  bufsize Size of the buffer `buf` points to
 *
 * @return true if removal succeeded
 * @return false if q is NULL or empty
 */
bool queue_remove_head(queue_t *q, char *buf, size_t bufsize) {
    // false if q is NULL or empty
    list_ele_t *cur;
    if (!q || q->num_ele == 0) {
        return false;
    }
    // else, we want to extract the string and put it in the buf
    cur = q->head;
    if (buf == NULL) // check buffer
    {
        return false;
    }
    //strncpy(buf, q->head->value, bufsize - 1);
    //buf[bufsize - 1] = '\0'; // add a null zero after the string
    // cut off the connections, discuss if there is only one node case
    /*q->head = q->head->next;
    cur->next = NULL;
    if (q->num_ele == 1)
    {
        q->tail = q->head;
    }
    free(cur->value);
    free(cur);
    q->num_ele -= 1;*/
    if(q->head == q->tail) { // check if the queue has only one element
        q->head = NULL;
        q->tail = NULL;
    }
    else {
        q->head = q->head->next;
    }
    strncpy(buf, cur->value, bufsize - 1);
    buf[bufsize - 1] = '\0'; // add a null zero after the string
    cur->next = NULL;
    free(cur->value);
    free(cur);
    q->num_ele -= 1;
    return true;
}

/**
 * @brief Returns the number of elements in a queue
 *
 * This function runs in O(1) time.
 *
 * @param[in] q The queue to examine
 *
 * @return the number of elements in the queue, or
 *         0 if q is NULL or empty
 */
size_t queue_size(queue_t *q) {
    /* You need to write the code for this function */
    /* Remember: It should operate in O(1) time */
    if (!q || q->num_ele == 0) {
        return 0;
    } else {
        return q->num_ele; // O(1) operation
    }
}

/**
 * @brief Reverse the elements in a queue
 *
 * This function does not allocate or free any list elements, i.e. it does
 * not call malloc or free, including inside helper functions. Instead, it
 * rearranges the existing elements of the queue.
 *
 * @param[in] q The queue to reverse
 */
void queue_reverse(queue_t *q) {
    /* You need to write the code for this function */
    // check if q is NULL or queue is empty or has only one element, then
    // reverse is not necessary
    list_ele_t *start;
    list_ele_t *cur;
    list_ele_t *prev = NULL;
    list_ele_t *next = NULL;

    if (!q || q->num_ele == 0 || q->num_ele == 1) {
        return; // do nothing
    } else{
        start = q->head;
        cur = q->head;
        while (cur != NULL) {
            next = cur->next;
            cur->next = prev;
            prev = cur;
            cur = next;
        }
        q->head = prev;
        q->tail = start;
        return;
    }
}
/*void debug(queue_t *q)
{
    int idx;
    list_ele_t *cur = q->head;
    for (idx = 1; idx <= q->num_ele; idx ++)
    {
        
        printf("the node %d is %s\n", idx, cur->value);
        cur = cur->next;
        
    }
    return;
}*/

