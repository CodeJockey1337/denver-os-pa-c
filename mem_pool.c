/*
 * Created by Ivo Georgiev on 2/9/16.
 
 * Operating Systems Assignment 1
 * Completed by: P. Ross Baldwin and group
 * Metropolitan State University of Denver
 * Spring 2016
 *
 * Group:
 *  Ross Baldwin
 *  Jedediah Hernandez
 *  Christopher
 *  M. Ryan Wingard
 */
 

#include <stdlib.h>
#include <assert.h>
#include <stdio.h> // for perror()

#include "mem_pool.h"

/*************/
/*           */
/* Constants */
/*           */
/*************/
static const float      MEM_FILL_FACTOR                 = 0.75;
static const unsigned   MEM_EXPAND_FACTOR               = 2;

static const unsigned   MEM_POOL_STORE_INIT_CAPACITY    = 20;
static const float      MEM_POOL_STORE_FILL_FACTOR      = 0.75;
static const unsigned   MEM_POOL_STORE_EXPAND_FACTOR    = 2;

static const unsigned   MEM_NODE_HEAP_INIT_CAPACITY     = 40;
static const float      MEM_NODE_HEAP_FILL_FACTOR       = 0.75;
static const unsigned   MEM_NODE_HEAP_EXPAND_FACTOR     = 2;

static const unsigned   MEM_GAP_IX_INIT_CAPACITY        = 40;
static const float      MEM_GAP_IX_FILL_FACTOR          = 0.75;
static const unsigned   MEM_GAP_IX_EXPAND_FACTOR        = 2;


/*********************/
/*                   */
/* Type declarations */
/*                   */
/*********************/

//The _node struct creates the doubly linked list of "allocation nodes"
typedef struct _node {
    alloc_t alloc_record;
    //Unsigned integers are always positive
    unsigned used;  //Used = 1, allocated = 0 IS A GAP
    unsigned allocated;
    struct _node * next; // Doubly-linked list for gap deletion
    struct _node * prev; // Doubly-linked list for gap deletion
} node_t, *node_pt;

//The _gap struct defines a node which is a gap in the allocation table
typedef struct _gap {
    size_t size;
    node_pt node;
} gap_t, *gap_pt;

//The _pool_mgr struct contains a memory pool, and a pointer to the list of allocation nodes for that pool.
//  Also holds some convienience variables
typedef struct _pool_mgr {
    pool_t pool;
    node_pt node_heap;
    unsigned total_nodes;
    unsigned used_nodes;
    gap_pt gap_ix;
    unsigned gap_ix_capacity;
} pool_mgr_t, *pool_mgr_pt;



/***************************/
/*                         */
/* Static global variables */
/*                         */
/***************************/
//?????
//pool_store is a pointer which points to AN ARRAY OF POINTERS.
//The array of pointers is of type pool_mgr_pt
//Using array subscripting on this pointer array will yield the POINTER AT THAT POISITION
//  This is because the deref of the pointer array is still a pointer
//  Must double deref to get to the pool_mgr_t
static pool_mgr_pt *pool_store = NULL; // an array of pointers, only expands
//You can apparently use array subscripting on pointers in C
static unsigned pool_store_size = 0;
static unsigned pool_store_capacity = 0;


/*************DEBUGGING****************/
//debug_flag == 1 turns on debugging
//debug_flag == 0 turns off debugging
static unsigned debug_flag = 0;
/*************DEBUGGING****************/


/********************************************/
/*                                          */
/* Forward declarations of static functions */
/*                                          */
/********************************************/
static alloc_status _mem_resize_pool_store();
static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr);
static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr);
static alloc_status
        _mem_add_to_gap_ix(pool_mgr_pt pool_mgr,
                           size_t size,
                           node_pt node);
static alloc_status
        _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                size_t size,
                                node_pt node);
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr);



/****************************************/
/*                                      */
/* Definitions of user-facing functions */
/*                                      */
/****************************************/

/*************DEBUGGING****************/
//custom helper function, debug(char * string)
//  debug function just does a printf
//  used to easily enable or disable verbose debugging
//  only prints debug lines if debug_flag == 1
void debug(const char * string){
    if(debug_flag == 1){
        printf("%s", string);
    }
}
void debug2 (const char* string, int a){
    if(debug_flag == 1){
        printf(string, a);
    }
}
/*************DEBUGGING****************/



//Written 100% by Ross
/* TESTED IN MY OWN TESTING SUITE - PASSED TESTS */
//mem_init() creates the struct pool_mgr -> Which refers to the Node structs and alloc_t structs
//  Function returns an "alloc_status" type - ALLOC_OK if it worked, or ALLOC_CALLED_AGAIN if called before mem_free()
alloc_status mem_init() {
    debug("FUNCTION CALL: mem_init() has been called\n");
    // ensure that it's called only once until mem_free
    //Make sure the pool has not yet been instantiated
    //If the variable * pool_store IS NULL (not points to NULL or something; the VALUE of the POINTER IS NULL)
    //  , then we have not yet created the pool
    if(pool_store != NULL){
        debug("FAIL: mem_init() has been called before mem_free(), pool_store is not NULL\n");
        return ALLOC_CALLED_AGAIN;
        //If the pool is anything but null, then mem_init() has been called before mem_free()
    }
    else{
        //We know pool_store is NULL now, proceed with creation of the pool
        debug("PASS: pool_store was NULL, attempting to calloc() for the pool_store\n");
        //pool_store is assigned a memory chunk here
        //  IS THIS A DOUBLE POINTER?????
        pool_store = (pool_mgr_pt *)calloc(MEM_POOL_STORE_INIT_CAPACITY, sizeof(pool_mgr_pt));
        //calloc returns NULL if it fails
        //This allocates MEM_POOL_STORE_INIT_CAPACITY number of pool_mgr_t structs
        //MEM_POOL_STORE_INIT_CAPACITY is == 20, so pool_store goes from pool_store[0]-pool_store[19]
        if(pool_store != NULL){
            //If the calloc succeeded, the value of pool_store should NO LONGER BE NULL (it is now some memory address)
            //Allocate the pool store with initial capacity before returning
            pool_store_capacity = MEM_POOL_STORE_INIT_CAPACITY;  //this capacity is 20
            //Set pool_store_size to initial value of 0
            pool_store_size = 0;
            debug("PASS: pool_store struct has been successfully allocated with calloc()\n");
            return ALLOC_OK; //Operation succeeded on all checks, return ALLOC_OK status
        }
    }
    // note: holds pointers only, other functions to allocate/deallocate
    debug("FAIL: Allocation of memory in mem_init() failed, calloc() has failed\n");
    return ALLOC_FAIL; //If the pool_store pointer was NULL, but the calloc failed, return ALLOC_FAIL status
}



//Written 100% by Ross
/* TESTED IN MY OWN TESTING SUITE - PASSED TESTS */
//mem_free() is called to release the pool_store list created by mem_init()
//This releases ALL of the pools in pool_store
//  If return status is ALLOC_NOT_FREED, or ALLOC_FAIL then this failed.  ALLOC_OK if it succeeded
alloc_status mem_free() {
    debug("FUNCTION CALL: mem_free() has been called\n");
    
    // ensure that it's called only once for each mem_init
    //check and make sure that pool_store is not NULL -> if it is, init() has not been called first
    if(pool_store == NULL){
        //if the pool_store is NULL, then there is nothing to mem_free(); return fail
        debug("FAIL: pool_store is NULL, mem_free must be called only after mem_init()\n");
        return ALLOC_CALLED_AGAIN;
    }
    if(pool_store != NULL){ //If pool_store is not NULL, then it exists
        // make sure all pool managers have been de-allocated
        //loop through the pool managers, and call mem_pool_close() on them
        //Need to compare to pool_store_capacity, to ensure all structs are deallocated
        debug2("PASS: pool_store_capacity = %u\n", pool_store_capacity);
        for(int i = 0; i < pool_store_size; i++){  //iterates through pool_store
            if(pool_store[i] != NULL) {
                alloc_status sts;
                //free each pool, regardless of if it has data in it or not
                //pool_store[i] is a value, and must be cast back to pointer type before being passed to mem_pool_close
                debug2("    Attempting to deallocate pool_store[%d]\n", i);
                //close the pool, and capture the status
                sts = mem_pool_close(&pool_store[i]->pool);
                if(sts != ALLOC_OK){
                    return ALLOC_FAIL;
                }
            }
        }
        debug("PASS: pool_store has been successfully de-allocated\n");
        // can free the pool store array now
        free(pool_store);  //frees the memory allocated to the pool_store list
    
        // update static variables
        //We have deleted everything in the pool_store, set it back to NULL
        pool_store = NULL; 
        pool_store_capacity = 0;
        pool_store_size = 0;
        
        //If we made it here, everything must have went well
        if(pool_store == NULL){
            //pool_store SHOULD BE NULL NOW
            return ALLOC_OK;
        }
        else{
            //mem_free failed
            return ALLOC_NOT_FREED;
        }

    }
}


//Written by Ross/Jed/Chris
/* TESTED IN MY OWN TESTING SUITE - CURRENTLY PASSING TESTS - NEEDS ADDITIONAL TESTS */
//mem_pool_open creates a pool_t struct (heap), given a size and a fit policy
//This function returns a pool_pt, which is a pointer to the newly created pool
//The return of a NULL pointer means the function failed
pool_pt mem_pool_open(size_t size, alloc_policy policy) {
    debug("FUNCTION CALL: mem_pool_open has been called\n");
    
    //Make sure that the pool store is allocated
    if(pool_store == NULL){
        //If the pool_store pointer is a NULL pointer, then init() has not been successfully called
        //return NULL
        debug("FAIL: mem_pool_open() has failed; pool_store is NULL, make sure init() has been called\n");
        return NULL;  //FAIL
    }
        //Implicitly call _mem_resize_pool_store - that method handles logic for
        //  checking if the pool_store needs to be resized or not.
        //  expand the pool store, if necessary -> _mem_resize_pool_store implements this logic
    alloc_status status = _mem_resize_pool_store();
    if(status != ALLOC_OK){
        debug("FAIL: _mem_resize_pool_store() call has returned a status other than ALLOC_OK\n");
            //If the resize failed, then we want to return a NULL pointer to indicate failure
        return NULL;
    }
    //pool_mgr_t
    // allocate a new mem pool mgr
    pool_mgr_pt new_pool_mgr_pt = (pool_mgr_pt)malloc(sizeof(pool_mgr_t));

    // check success, on error return null
    if(new_pool_mgr_pt == NULL){
        debug("FAIL: calloc of the pool_mgr_t has failed in mem_pool_open()\n");
        return NULL;
    }

    //pool.mem
    // allocate a new memory pool
        //This is a char * pointer called mem, and is saved in the pool_t struct
    new_pool_mgr_pt->pool.mem = (char *)malloc(size);
    
    //check success of .pool.mem malloc
    //  on error deallocate mem and return null
    if(NULL == new_pool_mgr_pt->pool.mem){
        //if we got here, something went wrong and .pool.mem never got allocated
        debug("FAIL: malloc of pool_mgr_t.pool.mem has failed in mem_pool_open()\n");
        //Deallocate our malloc
        free(new_pool_mgr_pt->pool.mem);
        return NULL;
    }

    //NODE_HEAP
    // allocate a new node heap and assign it to our pool_mgr_t struct
    new_pool_mgr_pt->node_heap = (node_pt)calloc(MEM_NODE_HEAP_INIT_CAPACITY, sizeof(node_t));
    // check success, on error deallocate mgr/pool and return null
    if(NULL == (*new_pool_mgr_pt).node_heap){
        //If we got here, something went wrong
        debug("FAIL: calloc() of the pool_mgr_t->node_heap has failed\n");
        //If the node_heap pointer still points to null and not a valid mem space,
        //  deallocate mgr/pool and return null
        free((*new_pool_mgr_pt).pool.mem);  //free the pool.mem
        free(new_pool_mgr_pt);              //free the newly created pool_mgr_t
        //  then this is a failure and return NULL
        return NULL;
    }
    
    //GAP_IX
    //Allocate a new gap index
    new_pool_mgr_pt->gap_ix = (gap_pt)calloc(MEM_GAP_IX_INIT_CAPACITY, sizeof(gap_t));
    //Initialize all of the gap sizes to 0
    for(int i = 0; i < MEM_GAP_IX_INIT_CAPACITY; ++i){
        new_pool_mgr_pt->gap_ix[i].size = 0;
    }
    // check success, on error deallocate mgr/pool/heap and return null
    if(NULL == (*new_pool_mgr_pt).gap_ix){
        debug("FAIL: calloc of gap_ix has failed in mem_pool_open()\n");
        //free() everything calloc'd up to this point
        free((*new_pool_mgr_pt).pool.mem);  //free the pool.mem
        free((*new_pool_mgr_pt).node_heap); //free the node_heap
        free(new_pool_mgr_pt);  //free the newly created pool_mgr_t
        return NULL;
    }
    
    //Assign all the pointers and update meta data:

    //FILL VARIABLES OF NEW NODE_T
    // alloc_record, used, allocated, *next, *prev
    new_pool_mgr_pt->node_heap[0].next = NULL;
    new_pool_mgr_pt->node_heap[0].prev = NULL;
    new_pool_mgr_pt->node_heap[0].used = 0;
    new_pool_mgr_pt->node_heap[0].allocated = 0;
    new_pool_mgr_pt->node_heap[0].alloc_record.mem = new_pool_mgr_pt->pool.mem;
    new_pool_mgr_pt->node_heap[0].alloc_record.size = size;

    //Initialize top node of gap index
    new_pool_mgr_pt->gap_ix[0].size = new_pool_mgr_pt->node_heap->alloc_record.size;
    new_pool_mgr_pt->gap_ix[0].node = new_pool_mgr_pt->node_heap;
    new_pool_mgr_pt->gap_ix_capacity = MEM_GAP_IX_INIT_CAPACITY;

    //Assign vars to the new pool_t
    //Assign the passed in policy to our new structs pool's alloc_policy
    (*new_pool_mgr_pt).pool.policy = policy;
    //Assign the total_size var in our pool
    (*new_pool_mgr_pt).pool.total_size = size;
    //Assign the alloc_size var in our pool
    //alloc_size starts at zero, as this pool is empty
    (*new_pool_mgr_pt).pool.alloc_size = 0;
    //Assign num_allocs; start at 0
    (*new_pool_mgr_pt).pool.num_allocs = 0;
    //Assign the num_gaps var in the pool
    //num_gaps starts at 1, as the entire pool at this point is a gap
    (*new_pool_mgr_pt).pool.num_gaps = 1;
    //POOL_MGR_T VARIABLES 4,5,6
    //initialize pool mgr variables
    // unsigned total_nodes;
    (*new_pool_mgr_pt).total_nodes = MEM_NODE_HEAP_INIT_CAPACITY;
    // unsigned used_nodes;
    (*new_pool_mgr_pt).used_nodes = 1;


    //   link pool mgr to pool store
    int openSpot = 0;
    while(NULL !=pool_store[openSpot]){
        ++openSpot;
    }
    pool_store[openSpot] = new_pool_mgr_pt;
    pool_store_size++;
        // return the address of the mgr, cast to (pool_pt)
    return (pool_pt)new_pool_mgr_pt;
}



//Written by Ross/Jed
alloc_status mem_pool_close(pool_pt pool) {
    debug("FUNCTION CALL: mem_pool_close() has been called\n");
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt local_pool_manager_pt = (pool_mgr_pt)pool;
    
    // check if this pool is allocated
    if(NULL == local_pool_manager_pt){
        debug("FAIL: The pool_pt passed into mem_pool_close == NULL");
        //If the pool pointer is NULL, then something has gone wrong
        return ALLOC_NOT_FREED; //FAIL
    }
    // check if pool has only one gap
    if(local_pool_manager_pt->pool.num_gaps != 1) {
        return ALLOC_NOT_FREED; //FAIL
    }

    // check if it has zero allocations
    if(local_pool_manager_pt->pool.num_allocs != 0) {
        return ALLOC_NOT_FREED; //FAIL
    }

    // free memory pool
    free(local_pool_manager_pt->pool.mem);
    //free(pool);
    // free node heap
    free(local_pool_manager_pt->node_heap);
    // free gap index
    free(local_pool_manager_pt->gap_ix);

    int insert = 0;
    while(pool_store[insert] != local_pool_manager_pt){
        ++insert;
    }
    // find mgr in pool store and set to null
    pool_store[insert] = NULL;
    
    // note: don't decrement pool_store_size, because it only grows
    // free mgr
    free(local_pool_manager_pt);
    return ALLOC_OK;
}


//Written by Chris/Jedediah/Ross
alloc_pt mem_new_alloc(pool_pt pool, size_t size) {
    debug("FUNCTION CALL: mem_new_alloc() has been called\n");
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt local_pool_mgr_pt = (pool_mgr_pt)pool;

    // check if any gaps, return null if none
    if (local_pool_mgr_pt->gap_ix[0].node == NULL){
        return NULL;
    }
    // expand heap node, if necessary, quit on error
    alloc_status status = _mem_resize_node_heap(local_pool_mgr_pt);
    if( status != ALLOC_OK){
        debug("FAIL: alloc_record returned by _mem_resize_node_heap != ALLOC_OK");
        return NULL;
    }
    // check used nodes fewer than total nodes, quit on error
    if(local_pool_mgr_pt->used_nodes>= local_pool_mgr_pt->total_nodes){
        debug("FAIL: Used nodes > total nodes.  Logical error has happened somewhere!");
        return NULL;
    }

    // get a node for allocation:
    node_pt new_node_pt;
    int i = 0;

    // if FIRST_FIT, then find the first sufficient node in the node heap
    if(local_pool_mgr_pt->pool.policy == FIRST_FIT) {
        node_pt this_node = local_pool_mgr_pt->node_heap;
        while ((local_pool_mgr_pt->node_heap[i].allocated != 0)
               || (local_pool_mgr_pt->node_heap[i].alloc_record.size < size)
                  && i < local_pool_mgr_pt->total_nodes) {
            ++i;
        }

        if ( i == local_pool_mgr_pt->total_nodes) {
            return NULL;
        }

        new_node_pt = &local_pool_mgr_pt->node_heap[i];
    }
    // if BEST_FIT, then find the first sufficient node in the gap index
    else if(local_pool_mgr_pt->pool.policy == BEST_FIT){
        if(local_pool_mgr_pt->pool.num_gaps > 0 ){
            //Check and see that we have not iterated past the number of gaps
            //The first gap which is >= to size IS THE BEST FIT, because the gap_ix is sorted!
            while( i < local_pool_mgr_pt->pool.num_gaps && local_pool_mgr_pt->gap_ix[i+1].size >= size){
                if( local_pool_mgr_pt->gap_ix[i].size == size){
                    break;
                }
                ++i;
            }
        }
        else{
            return NULL;
        }
        // check if node found
        new_node_pt = local_pool_mgr_pt->gap_ix[i].node;
    }


    // update metadata (num_allocs, alloc_size)
    local_pool_mgr_pt->pool.num_allocs++;
    local_pool_mgr_pt->pool.alloc_size += size;

    // calculate the size of the remaining gap, if any
    int size_of_gap = 0;
    //We are looking at the size of the alloc_record FOR THIS SPECIFIC NODE, not the whole pool
    if(new_node_pt->alloc_record.size - size > 0){
        size_of_gap = new_node_pt->alloc_record.size - size;
    }
    // remove node from gap index
    status = _mem_remove_from_gap_ix(local_pool_mgr_pt, size, new_node_pt);
    //Check and make sure this worked
    if( status != ALLOC_OK){
        //Someting went wrong
        debug("FAIL: Alloc status passed back from _mem_remove_from_gap_ix was a failure condition\n");
        return NULL;
    }
    // convert gap_node to an allocation node of given size
    //An allocated and used node has used = 1, allocated = 1
    new_node_pt->allocated = 1;
    new_node_pt->used = 1;
    new_node_pt->alloc_record.size = size;

    // adjust node heap:
    //   if remaining gap, need a new node
    //This new node is a gap of the size of the remaining open pool space
    if(size_of_gap > 0) {
        int s = 0;
        //   find an unused one in the node heap
        while (local_pool_mgr_pt->node_heap[s].used != 0) {
            s++;
        }
        node_pt new_gap_created = &local_pool_mgr_pt->node_heap[s];
        //   make sure one was found
        if (new_gap_created != NULL) {
            //   initialize it to a gap node
            new_gap_created->used = 1;
            new_gap_created->allocated = 0;
            //This sets the new gap node to be equal to the size of our "remainder" from allocating the
            // last alloc
            new_gap_created->alloc_record.size = size_of_gap;
        }

        //   update metadata (used_nodes)
        local_pool_mgr_pt->used_nodes++;
        //_mem_resize_node_heap will have incremented total_nodes for us, so no need to do that here

        //   update linked list (new node right after the node for allocation)
        new_gap_created->next = new_node_pt->next;

        if (new_node_pt->next != NULL) {
            new_node_pt->next->prev = new_gap_created;
        }
        new_node_pt->next = new_gap_created;
        new_gap_created->prev = new_node_pt;
        //   add to gap index
        _mem_add_to_gap_ix(local_pool_mgr_pt, size_of_gap, new_gap_created);
    }

    // return allocation record by casting the node to (alloc_pt)
    return (alloc_pt) new_node_pt;
}



//Code by Chris/Jedediah
alloc_status mem_del_alloc(pool_pt pool, alloc_pt alloc) {
    debug("FUNCTION CALL: mem_del_alloc() has been called\n");

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt manager = (pool_mgr_pt) pool;

    node_pt delete_node;

    // get node from alloc by casting the pointer to (node_pt)
    node_pt node = (node_pt) alloc;

    // find the node in the node heap
    for(int i = 0; i < manager -> total_nodes; ++i) {
        if (node == &manager -> node_heap[i]) {
            // this is node-to-delete
            delete_node = &manager -> node_heap[i];
            break;
        }
    }

    // make sure it's found
    if(delete_node == NULL) {
        return ALLOC_NOT_FREED;
    }

    // convert to gap node
    delete_node -> allocated = 0;

    // update metadata (num_allocs, alloc_size)
    manager -> pool.num_allocs--;
    manager -> pool.alloc_size -= delete_node -> alloc_record.size;

    // if the next node in the list is also a gap, merge into node-to-delete
    node_pt node_to_merge = NULL;
    if (delete_node -> next != NULL && delete_node -> next -> used == 1 && delete_node -> next -> allocated == 0 ) {

        node_to_merge = delete_node->next;

        //   remove the next node from gap index
        _mem_remove_from_gap_ix(manager, node_to_merge->alloc_record.size, node_to_merge);

        //   check success

        //   add the size to the node-to-delete
        delete_node->alloc_record.size = delete_node->alloc_record.size + delete_node->next->alloc_record.size;

        //   update node as unused
        node_to_merge->used = 0;

        //   update metadata (used nodes)
        manager->used_nodes--;

        //   update linked list:

        if (node_to_merge->next) {
            node_to_merge->next->prev = delete_node;
            delete_node->next = node_to_merge->next;
        } else {
            delete_node->next = NULL;
        }
        node_to_merge->next = NULL;
        node_to_merge->prev = NULL;
        node_to_merge->alloc_record.mem = NULL;
        node_to_merge->alloc_record.size = 0;
    }

    // this merged node-to-delete might need to be added to the gap index
    _mem_add_to_gap_ix(manager, delete_node ->alloc_record.size, delete_node);

    // but one more thing to check...
    // if the previous node in the list is also a gap, merge into previous!
    node_pt previous_node;

    if(delete_node -> prev != NULL && delete_node -> prev -> allocated == 0 && delete_node -> prev -> used == 1) {
        previous_node = delete_node->prev;

        //   remove the previous node from gap index
        _mem_remove_from_gap_ix(manager, previous_node->alloc_record.size, previous_node);
        _mem_remove_from_gap_ix(manager, delete_node->alloc_record.size, delete_node);

        //   check success
        //   add the size of node-to-delete to the previous
        previous_node->alloc_record.size = delete_node->alloc_record.size + delete_node->prev->alloc_record.size;

        // set the char * mem to NULL and updated the allocated record size
        delete_node->alloc_record.mem = NULL;
        delete_node->alloc_record.size = 0;

        //   update node-to-delete as unused
        delete_node->used = 0;
        delete_node->allocated = 0;

        //   update metadata (used_nodes)
        manager->used_nodes--;

        //   update linked list

        if (delete_node->next) {
            previous_node->next = delete_node->next;
            delete_node->next->prev = previous_node;
        } else {
            previous_node->next = NULL;
        }
        delete_node->next = NULL;
        delete_node->prev = NULL;

        //   change the node to add to the previous node!
        // add the resulting node to the gap index

        _mem_add_to_gap_ix(manager, previous_node->alloc_record.size, previous_node);
        // check success
    }
    return ALLOC_OK;
}


//Written 100% by Ross
void mem_inspect_pool(pool_pt pool,
                      pool_segment_pt *segments,
                      unsigned *num_segments) {
    debug("FUNCTION CALL: mem_inspect_pool() has been called\n");
    // get the mgr from the pool
    pool_mgr_pt local_pool_mgr_pt = (pool_mgr_pt)pool;
    // allocate the segments array
        //There are "used_nodes" number of segments to be generated - one for each used_node of the pool_mgr_t
    pool_segment_pt segments_array = (pool_segment_pt)calloc(local_pool_mgr_pt->used_nodes, sizeof(pool_segment_t));
    // check if successful
        //A fail will be indicated by the segments_array pointer being == NULL
        //This will only happen if the calloc failed
    if(segments_array == NULL){
        return;  //Cannot return anything, as this method has no return type
    }
    // loop through the node heap
        //Check and make sure node_heap[0] is the head of the list
    if(local_pool_mgr_pt->node_heap[0].prev != NULL){
        debug("FAIL: mem_inspect_pool() : node_heap[0].prev is not NULL - It is not the head of the list!");
    }
    //local_pool_mgr_pt->node_heap[0] is the first element
    //Iterates through until .next == NULL, which means we reached the end of the list
    //MAKE SURE YOU LOOP THROUGH THE LINKED LIST AND NOT THE NODE_HEAP ARRAY
    //BECAUSE - the linked list is in order, and the node_heap is NOT
    int i = 0;
        //Yea yea, you don't have to explicitly check for NULL here
    while(local_pool_mgr_pt->node_heap[i].next != NULL){
        //Assign the allocated property to the segments array for each allocated node in node_heap
        //for each node, write the size and allocated in the segments
        segments_array[i].allocated = local_pool_mgr_pt->node_heap[i].alloc_record.size;
        segments_array[i].size = local_pool_mgr_pt->node_heap[i].allocated;
        ++i;
    }
    
    // "return" the values:
        //Code given by Ivo in the default file below:
    *segments = segments_array;
    *num_segments = local_pool_mgr_pt->used_nodes;
     return;
}



/***********************************/
/*                                 */
/* Definitions of static functions */
/*                                 */
/***********************************/

//Written 100% by Ross
/* TESTED IN MY OWN TESTING SUITE - PASSED */
//This method is called from mem_pool_open()
static alloc_status _mem_resize_pool_store() {
    debug("FUNCTION CALL: _mem_resize_pool_store() has been called\n");
    
    //Check if the pool_store needs to be resized
    //  "necessary" to resize when size/cap > 0.75
    if (((float)pool_store_size / (float)pool_store_capacity) > MEM_POOL_STORE_FILL_FACTOR){
        debug("     attempting to resize pool_store; size/cap ratio has been reached\n");
        
        pool_store = (pool_mgr_pt *)realloc(pool_store, (sizeof(pool_mgr_t)*pool_store_capacity + 1));
        //Verify the realloc worked
        if(pool_store == NULL){
            debug("FAIL: realloc of pool_store was catastrophic!!!\n");
            return ALLOC_FAIL;
        }
        //double check that pool_store isnt NULL after realloc
        assert(pool_store);
        //Update capacity variable
        pool_store_capacity++;
        debug("PASS: pool_store successfully resized - returning ALLOC_OK\n");
        return ALLOC_OK;
    }
    
    debug("PASS: pool_store did not need to be resized - returning ALLOC_OK\n");
    return ALLOC_OK;
}




//Written 100% by Ross
static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr_ptr) {
    debug("FUNCTION CALL: _mem_resize_node_heap() has been called\n");
    //Check if the node_heap needs to be resized
    //  "necessary" to resize when size/cap > 0.75
    if(((float)pool_mgr_ptr->used_nodes / (float)pool_mgr_ptr->total_nodes) > MEM_NODE_HEAP_FILL_FACTOR){
        //Reallocate more nodes to the node_heap, by the size of the
        //Perform the realloc straight to the node_heap pointer
        pool_mgr_ptr->node_heap = (node_pt)realloc(pool_mgr_ptr->node_heap,
            MEM_NODE_HEAP_EXPAND_FACTOR * pool_mgr_ptr->total_nodes * sizeof(node_t));
        //Check and see if the realloc failed
        if (NULL == pool_mgr_ptr->node_heap){
            debug("FAIL: realloc in _mem_resize_node_heap was catastrophic!!");
            return ALLOC_FAIL;
        }
        //Make sure to update the number of nodes!  This is a prop of the pool_mgr_t
        pool_mgr_ptr->total_nodes *= MEM_NODE_HEAP_EXPAND_FACTOR;
        debug("     node_heap has been resized\n");
        return ALLOC_OK;
    }
    debug("     node_heap did not need to be resized\n");
    return ALLOC_OK;
}




//Code by Ryan and Ross
static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr) {
    debug("FUNCTION CALL: _mem_resize_gap_ix() has been called\n");
    float expandFactor = (float)MEM_GAP_IX_EXPAND_FACTOR * (float)pool_mgr->gap_ix_capacity;
    //Does gap_ix need to be resized?
    if((((float)pool_mgr->pool.num_gaps)/(pool_mgr->gap_ix_capacity)) > MEM_GAP_IX_FILL_FACTOR){
        //resize if needed
        //Perform realloc to increase size of gap_ix
        pool_mgr->gap_ix = realloc(pool_mgr->gap_ix, expandFactor * sizeof(gap_t));
        //update metadata
        //  Expand the gap_ix_capacity by the same factor as the realloc
        pool_mgr->gap_ix_capacity *= MEM_GAP_IX_EXPAND_FACTOR;
        //Check and make sure it worked
        if(NULL == pool_mgr->gap_ix){
            debug("FAIL: realloc of gap_ix was catastrophic!!");
            return ALLOC_FAIL;
        }
        return ALLOC_OK;
    }
    return ALLOC_OK;
}




//Code by Ryan
static alloc_status _mem_add_to_gap_ix(pool_mgr_pt pool_mgr, size_t size, node_pt node) {
    debug("FUNCTION CALL: _mem_add_to_gap_ix() has been called\n");
    // expand the gap index, if necessary (call the function)
    _mem_resize_gap_ix(pool_mgr);
    // add the entry at the end
    //
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].node = node;
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].size = size;
    // update metadata (num_gaps)
    pool_mgr->pool.num_gaps++;
    // sort the gap index (call the function)
    alloc_status sortStatus = _mem_sort_gap_ix(pool_mgr);
    // check success
    if(sortStatus == ALLOC_OK){
        return ALLOC_OK;
    }
    return ALLOC_FAIL;
}




static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                            size_t size,
                                            node_pt node) {
    debug("FUNCTION CALL: _mem_remove_from_gap_ix() has been called\n");
    // find the position of the node in the gap index
    int position = 0;
    //Flag to determine if a match has been found
    int flag = 0;
    for(position; position < pool_mgr->gap_ix_capacity; position++){
        //If we find the node....
        if(pool_mgr->gap_ix[position].node == node){
            flag = 1;
            break;
        }
    }
    if(flag == 0){
        debug("FAIL: flag == 0 in _mem_remove_from_gap_ix");
        return ALLOC_FAIL;
    }
    // loop from there to the end of the array:
    while(position < pool_mgr->pool.num_gaps){
        //    pull the entries (i.e. copy over) one position up
        //    this effectively deletes the chosen node
        pool_mgr->gap_ix[position] = pool_mgr->gap_ix[position + 1];
        position++;
    }
    // update metadata (num_gaps)
    pool_mgr->pool.num_gaps --;
    // zero out the element at position num_gaps!
    //This final gap_t is a copy of the second to last gap_t, so we need to NULL it out
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].size = 0;
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps].node = NULL;

    //*****DO WE WANT TO SORT THE GAP_IX HERE?*******
    //Don't think we need to....

    return ALLOC_OK;
}




//Code by ??????
// note: only called by _mem_add_to_gap_ix, which appends a single entry
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr) {
    debug("FUNCTION CALL: _mem_sort_gap_ix() has been called\n");
    for(int i = 0; i < pool_mgr->pool.num_gaps; ++i){
        int swapped = 0;
        for(int j = 0; i < pool_mgr->pool.num_gaps; ++i){
            if(pool_mgr->gap_ix[i].size < pool_mgr->gap_ix[i+1].size){
                gap_t swap = pool_mgr->gap_ix[i];
                pool_mgr->gap_ix[i] = pool_mgr->gap_ix[i+1];
                pool_mgr->gap_ix[i+1] = swap;
                swapped = 1;

            }
        }
        if(swapped == 0) break;
    }

    return ALLOC_OK;

//    //If the number of gaps is either 0 or 1, there is no reason to sort the list
//    if (pool_mgr->pool.num_gaps == 1 || pool_mgr->pool.num_gaps == 0){
//        return ALLOC_OK;
//    }
//
//    // the new entry is at the end, so "bubble it up"
//    // loop from num_gaps - 1 until but not including 0:
//    for (int i = pool_mgr->pool.num_gaps - 1; i > 0; i--) {
//        //    if the size of the current entry is less than the previous (i - 1)
//        if(pool_mgr->gap_ix[i].size <= pool_mgr->gap_ix[i-1].size){
//            //    or if the sizes are the same but the current entry points to a
//            //    node with a lower address of pool allocation address (mem)
//            //Check if it is the same size as the previous....
//            if(pool_mgr->gap_ix[i-1].size == pool_mgr->gap_ix[i].size){
//                //if it is the exact same size, NOW we need to sort by mem address
//                if(pool_mgr->gap_ix[i].node > pool_mgr->gap_ix[i-1].node){
//                    break;
//                }
//            }
//            //swap them (by copying) (remember to use a temporary variable)
//            node_pt node_pt_temp = pool_mgr->gap_ix[i-1].node;
//            size_t size_t_temp = pool_mgr->gap_ix[i-1].size;
//            //move the gap_t "up"
//            pool_mgr->gap_ix[i-1] = pool_mgr->gap_ix[i];
//            //Assign the temp node to the spot in the gap_ix
//            pool_mgr->gap_ix[i].node = node_pt_temp;
//            pool_mgr->gap_ix[i].size = size_t_temp;
//
//        }
//    }
//    return ALLOC_OK;
}
