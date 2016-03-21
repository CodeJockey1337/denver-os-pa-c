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
static unsigned debug_flag = 1;
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
        return ALLOC_NOT_FREED;
    }
    if(pool_store != NULL){ //If pool_store is not NULL, then it exists
        debug("PASS: pool_store has been found, attempting to close all mem pools\n");
        // make sure all pool managers have been de-allocated
        //loop through the pool managers, and call mem_pool_close() on them
        //Need to compare to pool_store_capacity, to ensure all structs are deallocated
        debug2("PASS: pool_store_capacity = %u\n", pool_store_capacity);
        for(int i=0; i < pool_store_capacity; (i++)){  //iterates through pool_store
            if(pool_store[i] != NULL) {
                alloc_status sts;
                //free each pool, regardless of if it has data in it or not
                //pool_store[i] is a value, and must be cast back to pointer type before being passed to mem_pool_close
                debug2("    Attempting to deallocate pool_store[%d]\n", i);
                sts = mem_pool_close((pool_pt) pool_store[i]);  //close the pool, and capture the status
                //assert(sts == ALLOC_OK); //make sure each de-allocation happened successfully
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
        return ALLOC_OK;
    }
}


//Written 100% by Ross
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
    debug("PASS: An initialized pool_store has been found\n");
    
    //If we made it here, things are going well
    //Implicitly call _mem_resize_pool_store - that method handles logic for 
    //  checking if the pool_store needs to be resized or not.
    //  expand the pool store, if necessary -> _mem_resize_pool_store implements this logic
    //Calling _mem_resize_pool_store from the if statement below
    debug("     attempt to resize the pool_store\n");
    if(_mem_resize_pool_store() != ALLOC_OK){  //This if statement has major side-effects
        debug("FAIL: _mem_resize_pool_store() call has returned a status other than ALLOC_OK\n");
        //If the resize failed, then we want to return a NULL pointer to indicate failure
        return NULL;  //FAIL
    }
    
    // allocate a new mem pool mgr
    debug("     Attempting to calloc() a new pool_mgr_t\n");
    pool_mgr_pt new_pool_mgr_pt = (pool_mgr_pt)calloc(1, sizeof(pool_mgr_t));
    // check success, on error return null
    if(new_pool_mgr_pt == NULL){
        debug("FAIL: calloc of the pool_mgr_t has failed in mem_pool_open()\n");
        //If the new pointer we just tried to calloc is NULL, then something went wrong
        //If the calloc failed, return NULL to indicate failure
        return NULL; //FAIL
    }
    debug("PASS: calloc() of the pool_mgr_t in mem_pool_open() successful\n");
    
    //POOL_MGR_T VARIABLE 1 - POOL
    // allocate a new memory pool
        //Line below callocs space for the new pool, and saves it to the pool
        //  variable of the our newly created pool_mgr_t
    debug("     attempting to calloc() .pool and pool.mem\n");
    (*new_pool_mgr_pt).pool = *(pool_pt)calloc(1, sizeof(pool_t));
        //Line below callocs space for the mem array in pool_t
        //This is a char * pointer called mem, and is saved in the pool_t struct
    (*new_pool_mgr_pt).pool.mem = (char *)calloc(1, sizeof(size));
    
    //check success of .pool and .pool.mem callocs
    //  on error deallocate mgr and return null
    if(NULL == (*new_pool_mgr_pt).pool.mem){ //IS THIS RIGHT????.(*mem)????
        //if we got here, something went wrong and .pool and .pool.mem never got allocated
        debug("FAIL: calloc() of pool_mgr_t.pool or pool_mgr_t.pool.mem has failed in mem_pool_open()\n");
        //Deallocate the pool_mgr only - the other 2 were not made
        free(new_pool_mgr_pt);       //free the newly created pool_mgr_t
        debug("     pool_mgr_t created in mem_pool_open() has been freed\n");
        return NULL; //FAIL
    }
    debug("PASS: new pool_mgr_t has been created\n");
    
    //Assign vars to the new pool_t
    debug("     attempting to assign vars to pool_mgr_t.pool\n");
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
    debug("PASS: values have been assigned to vars of pool_mgr_t.pool\n");
    
    //If we got here, things are going well
    //POOL_MGR_T VARIABLE 2 - NODE_HEAP
    // allocate a new node heap and assign it to our pool_mgr_t struct
    debug("     Attempting to calloc() space for node_heap in mem_pool_open()\n");
    (*new_pool_mgr_pt).node_heap = (node_pt)calloc(MEM_NODE_HEAP_INIT_CAPACITY, sizeof(node_t));
    // check success, on error deallocate mgr/pool and return null
    if(NULL == (*new_pool_mgr_pt).node_heap){  //IS THIS RIGHT???
        //If we got here, something went wrong
        debug("FAIL: calloc() of the pool_mgr_t->node_heap has failed\n");
        //If the node_heap pointer still points to null and not a valid mem space,
        //  deallocate mgr/pool and return null
        free((*new_pool_mgr_pt).pool.mem);  //free the pool.mem
        free(new_pool_mgr_pt);              //free the newly created pool_mgr_t
        //  then this is a failure and return NULL
        return NULL; //FAIL
    }
    debug("PASS: calloc() for node_heap successful\n");
    
    //POOL_MGR_T VARIABLE 3 - GAP_IX
    //Allocate a new gap index
    //If we got here, things are going well
    debug("     attempting to calloc() space for gap_ix (gap list)\n");
    (*new_pool_mgr_pt).gap_ix = (gap_pt)calloc(MEM_GAP_IX_INIT_CAPACITY, sizeof(gap_t));
    // check success, on error deallocate mgr/pool/heap and return null
    if(NULL == (*new_pool_mgr_pt).gap_ix){
        //If we got here, something has gone wrong
        debug("FAIL: calloc of gap_ix has failed in mem_pool_open()\n");
        //free() everything calloc'd up to this point
        free((*new_pool_mgr_pt).node_heap); //free the node_heap
        free((*new_pool_mgr_pt).pool.mem);  //free the pool.mem
        free(new_pool_mgr_pt);  //free the newly created pool_mgr_t
        return NULL; //FAIL
    }
    debug("PASS: calloc() for gap_ix successful\n");
    
    //Assign all the pointers and update meta data:
    //Initialize top node of node heap
    debug("     attempting to calloc() the first node of the node_heap\n");
    (*new_pool_mgr_pt).node_heap[0] = *(node_pt)calloc(1, sizeof(node_t));
     debug("PASS: calloc of first node_t in node_heap successful\n");
    
    //FILL VARIABLES OF NEW NODE STRUCT
        // alloc_record, used, allocated, *next, *prev
        //ALLOC_RECORD NOT INITIALIZED HERE
    debug("     attempting to set used, allocated, next and prev on the first node\n");
        //assign .next and .prev to NULL
    (*new_pool_mgr_pt).node_heap[0].used = 0;
    (*new_pool_mgr_pt).node_heap[0].allocated = 0;
    (*new_pool_mgr_pt).node_heap[0].next = NULL;
    (*new_pool_mgr_pt).node_heap[0].prev = NULL;
    
    //Initialize top node of gap index
    debug("     attempting to calloc the top gap_t of gap_ix\n");
    (*new_pool_mgr_pt).gap_ix[0] = *(gap_pt)calloc(1, sizeof(gap_t));
    debug("PASS: calloc of first node_t in node_heap successful\n");
    
    //FILL VARIABLES OF NEW GAP STRUCT
    debug("     attempting to fill vars of first gap_t in gap_ix\n");
    (*new_pool_mgr_pt).gap_ix[0].size = size;
    (*new_pool_mgr_pt).gap_ix[0].node = (*new_pool_mgr_pt).node_heap;
    //POOL_MGR_T VARIABLES 4,5,6
    //initialize pool mgr variables
        // unsigned total_nodes;
    (*new_pool_mgr_pt).total_nodes = MEM_NODE_HEAP_INIT_CAPACITY;
        // unsigned used_nodes;
    (*new_pool_mgr_pt).used_nodes = 1;
        // unsigned gap_ix_capacity;
    (*new_pool_mgr_pt).gap_ix_capacity = 1;
        //   link pool mgr to pool store
    debug("     attempting to assign the completed pool_mgr_t to the pool_store\n");
    //According to the line below, pool_store[n] is a pointer, not a struct
    //We are adding it to location "pool_store_size", as the first one
    //  is pool_store[0] when pool_store_size == 0
    pool_store[pool_store_size] = new_pool_mgr_pt;
    // return the address of the mgr, cast to (pool_pt)
    debug("     attempting to return the pointer to the new pool_mgr_t\n");
    return (pool_pt)new_pool_mgr_pt;
}



//Written 100% by Ross
alloc_status mem_pool_close(pool_pt pool) {
    debug("FUNCTION CALL: mem_pool_close() has been called\n");
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt local_pool_manager_pt = (pool_mgr_pt)pool;
    
    // check if this pool is allocated
    if(NULL == pool){
        debug("FAIL: The pool_pt passed into mem_pool_close == NULL");
        //If the pool pointer is NULL, then something has gone wrong
        return ALLOC_NOT_FREED; //FAIL
    }
    // check if pool has only one gap
    if(pool->num_gaps == 1){
        // check if it has zero allocations
        if(pool->num_allocs == 0){
            // free memory pool
            free(local_pool_manager_pt->pool.mem);
            //free(pool);
            // free node heap
            free(local_pool_manager_pt->node_heap);
            // free gap index
            free(local_pool_manager_pt->gap_ix);
            
        }
        else{
            return ALLOC_NOT_FREED; //FAIL
        }
    }
    else{
        return ALLOC_NOT_FREED; //FAIL
    }
    int i = 0;
    while(pool_store[i] != local_pool_manager_pt){
        ++i;
    }
    // find mgr in pool store and set to null
    pool_store[i] = NULL;
    
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
    if (pool->num_gaps == 0){
        return NULL;
    }
    // expand heap node, if necessary, quit on error
    alloc_status status = _mem_resize_node_heap(local_pool_mgr_pt);
    if( status != ALLOC_OK){
        debug("FAIL: alloc_record returned by _mem_resize_node_heap != ALLOC_OK");
        exit(1);  //FAIL
    }
    // check used nodes fewer than total nodes, quit on error
    if(local_pool_mgr_pt->total_nodes <= local_pool_mgr_pt->used_nodes){
        debug("FAIL: Used nodes > total nodes.  Logical error has happened somewhere!");
        exit(1); //FAIL
    }

    // get a node for allocation:
    node_pt new_node_pt;
    int i = 0;

    // if FIRST_FIT, then find the first sufficient node in the node heap
    if(pool->policy == FIRST_FIT) {
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
    if(pool->policy == BEST_FIT){
        if(pool->num_gaps > 0 ){
            //Check and see that we have not iterated past the number of gaps
            //The first gap which is >= to size IS THE BEST FIT, because the gap_ix is sorted!
            for(int j=0; j < pool->num_gaps; j++) {
                if( local_pool_mgr_pt->gap_ix[j].size >= size){
                    new_node_pt = local_pool_mgr_pt->gap_ix[j].node;
                    break;
                }
            }
        }
    }
    else{
        //Neither best fit not first fit have worked, something went wrong
        debug("FAIL: FIRST_FIT or BEST_FIT have failed in mem_new_alloc\n");
        return NULL;
    }

    // check if node found
    if(new_node_pt == NULL){
        debug("FAIL: mem_new_alloc failed to assign a node to the new_node_pt (it is NULL)\n");
        return NULL;
    }
    // update metadata (num_allocs, alloc_size)
    pool->num_allocs++;
    pool->alloc_size += size;
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
        if (new_gap_created == NULL) {
            debug("FAIL: newly created gap_t was NULL\n");
            return NULL;
        }
        //   initialize it to a gap node
        new_gap_created->used = 1;
        new_gap_created->allocated = 0;
        //This sets the new gap node to be equal to the size of our "remainder" from allocating the
        // last alloc
        new_gap_created->alloc_record.size = size_of_gap;

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
        //   check if successful?
        //****TODO******??
    }
    // return allocation record by casting the node to (alloc_pt)
    return (alloc_pt) new_node_pt;
}



//Code by Chris/Jedediah
alloc_status mem_del_alloc(pool_pt pool, alloc_pt alloc) {
    debug("FUNCTION CALL: mem_del_alloc() has been called\n");
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    // get node from alloc by casting the pointer to (node_pt)
    // find the node in the node heap
    // this is node-to-delete
    // make sure it's found
    // convert to gap node
    // update metadata (num_allocs, alloc_size)
    // if the next node in the list is also a gap, merge into node-to-delete
    //   remove the next node from gap index
    //   check success
    //   add the size to the node-to-delete
    //   update node as unused
    //   update metadata (used nodes)
    //   update linked list:
    /*
                    if (next->next) {
                        next->next->prev = node_to_del;
                        node_to_del->next = next->next;
                    } else {
                        node_to_del->next = NULL;
                    }
                    next->next = NULL;
                    next->prev = NULL;
     */

    // this merged node-to-delete might need to be added to the gap index
    // but one more thing to check...
    // if the previous node in the list is also a gap, merge into previous!
    //   remove the previous node from gap index
    //   check success
    //   add the size of node-to-delete to the previous
    //   update node-to-delete as unused
    //   update metadata (used_nodes)
    //   update linked list
    /*
                    if (node_to_del->next) {
                        prev->next = node_to_del->next;
                        node_to_del->next->prev = prev;
                    } else {
                        prev->next = NULL;
                    }
                    node_to_del->next = NULL;
                    node_to_del->prev = NULL;
     */
    //   change the node to add to the previous node!
    // add the resulting node to the gap index
    // check success
    debug("FUNCTION NOT YET IMPLEMENTED - RETURNING ALLOC_FAIL\n");
    return ALLOC_FAIL;
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
        debug("     attempting to resize node_heap; used/total ratio has been reached\n");
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
        debug("PASS: node_heap successfully resized - returning ALLOC_OK\n");
        return ALLOC_OK;
    }
    debug("PASS: node_heap did not need to be resized - returning ALLOC_OK\n");
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




//Code by Ryan
// note: only called by _mem_add_to_gap_ix, which appends a single entry
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr) {
    debug("FUNCTION CALL: _mem_sort_gap_ix() has been called\n");
    // the new entry is at the end, so "bubble it up"
    // loop from num_gaps - 1 until but not including 0:
    //    if the size of the current entry is less than the previous (u - 1)
    //       swap them (by copying) (remember to use a temporary variable)

    // the new entry is at the end, so "bubble it up"
    // loop from num_gaps - 1 until but not including 0:
    //    if the size of the current entry is less than the previous (u - 1)
    //    or if the sizes are the same but the current entry points to a
    //    node with a lower address of pool allocation address (mem)
    //       swap them (by copying) (remember to use a temporary variable)

//    for(int j = pool_mgr->pool.num_gaps - 1; i > 0; i--){
//        if(pool_mgr->gap_ix[i].node->alloc_record.size < pool_mgr->gap_ix[i - 1].node->alloc_record.size
//           || pool_mgr->gap_ix[i].node->alloc_record.mem < pool_mgr->gap_ix[i - 1].node->alloc_record.mem
//              && pool_mgr->gap_ix[i].node->alloc_record.size == pool_mgr->gap_ix[i - 1].node->alloc_record.size){
//            _gap tGap;
//            tGap = pool_mgr->gap_ix[j - 1];
//            pool_mgr->gap_ix[i - 1] = pool_mgr->gap_ix[i];
//            pool_mgr->gap_ix[i] = tGap;
//        }
//    }
    return ALLOC_OK;
}
