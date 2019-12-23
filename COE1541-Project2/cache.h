/* This file contains a rough implementation of an L1 cache in the absence of an L2 cache */
#include <stdlib.h>
#include <stdio.h>

struct write_buffer_t {
  int startBuffer;				
  int endBuffer;			
  int valid;
}; 

struct cache_blk_t { // note that no actual data will be stored in the cache 
  unsigned long tag;
  unsigned long L2_tag;
  int valid ;
  char dirty;
  unsigned LRU;	//to be used to build the LRU stack for the blocks in a cache set
				// the block with the largest value of LRU is the least recently used
} cache_blk_t;

struct cache_t {
	// The cache is represented by a 2-D array of blocks. 
	// The first dimension of the 2D array is "nsets" which is the number of sets (entries)
	// The second dimension is "assoc", which is the number of blocks in each set.
  int nsets;					// number of sets
  int blocksize;				// block size
  int assoc;					// associativity
  int mem_latency;				// the latency of reading or writing to memory
  struct cache_blk_t **blocks;	// a pointer to the array of cache blocks
};

struct cache_t * cache_create(int size, int blocksize, int assoc, int mem_latency)
{
  int i,j, nblocks , nsets ;
  struct cache_t *C = (struct cache_t *)calloc(1, sizeof(struct cache_t));
		
  nblocks = size *1024 / blocksize ;// number of blocks in the cache
  nsets = nblocks / assoc ;			// number of sets (entries) in the cache
  C->blocksize = blocksize ;
  C->nsets = nsets  ; 
  C->assoc = assoc;
  C->mem_latency = mem_latency;

  C->blocks= (struct cache_blk_t **)calloc(nsets, sizeof(struct cache_blk_t *));

  for(i = 0; i < nsets; i++) {
		C->blocks[i] = (struct cache_blk_t *)calloc(assoc, sizeof(struct cache_blk_t));
		for (j = 0; j < assoc; j++) C->blocks[i][j].valid = 0; /* blocks are initially invalid */
	}
  return C;
}
//------------------------------

int updateLRU(struct cache_t *cp ,int index, int way)
// maintain the following property:
// the Least recently used block has the largest LRU value and 
// the most recently used block has the smallest LRU value
{
int k ;
for (k=0 ; k< cp->assoc ; k++) {
    if(cp->blocks[index][k].LRU < cp->blocks[index][way].LRU) {
        cp->blocks[index][k].LRU = cp->blocks[index][k].LRU + 1 ;
    }
}
cp->blocks[index][way].LRU = 0;  /* blocks[index][way] is the most recently used */
return 0;
}
void L2_set_dirty(unsigned long L2_tag, struct cache_t *L2, int L2_index){
  int i;
  for (i = 0; i < L2->assoc; i++) {	/* look for the requested block */
      if (L2->blocks[L2_index][i].tag == L2_tag && L2->blocks[L2_index][i].valid == 1) {
          L2->blocks[L2_index][i].dirty = 1;
      }
  }
}

int L2_cache_access(struct cache_t *L2, unsigned long address, int access_type /*0 for read, 1 for write*/, struct write_buffer_t *WB,unsigned int cycle_number){
  int i,latency ;
  int block_address ;
  int index ;
  int tag ;
  int way ;
  int max ;
  block_address = (address / L2->blocksize);
  tag = block_address / L2->nsets;
  index = block_address - (tag * L2->nsets) ;

  latency = 0;
  for (i = 0; i < L2->assoc; i++) {	/* look for the requested block */
      if (L2->blocks[index][i].tag == tag && L2->blocks[index][i].valid == 1) {
        updateLRU(L2, index, i) ;
        //if address is found and write function then set dirty bit to 1 and "put data back in L1"
        if (access_type == 1){
            L2->blocks[index][i].dirty = 1 ;
        }
        return(latency);					/* a cache hit */
      }
  }
  /* a cache miss */
  for (way=0 ; way< L2->assoc ; way++){		/* look for an invalid entry */
      if (L2->blocks[index][way].valid == 0) {
    	    latency = latency + L2->mem_latency;	/* account for reading the block from memory*/
          L2->blocks[index][way].valid = 1 ;
          L2->blocks[index][way].tag = tag ;
    	    L2->blocks[index][way].LRU = way;
    	    updateLRU(L2, index, way); 
    	    L2->blocks[index][way].dirty = 0;
          if(access_type == 1) {
            L2->blocks[index][way].dirty = 1 ;
          }
    	    return(latency);				/* an invalid entry is available*/
      }
  }

    max = L2->blocks[index][0].LRU ;	/* find the LRU block - the block with the largest LRU field */
    way = 0 ;
    for (i=1 ; i< L2->assoc ; i++)  
    if (L2->blocks[index][i].LRU > max) {
      max = L2->blocks[index][i].LRU ;
      way = i ;
    }
  //L2->blocks[index][way] is the LRU block.
  if (L2->blocks[index][way].dirty == 1) { 
  	if(WB->valid){
      if(cycle_number< WB->endBuffer){
        latency = latency + WB->endBuffer - cycle_number;
        cycle_number = WB->endBuffer;
        WB->valid = 1;
        WB->startBuffer = cycle_number + L2->mem_latency;
        WB->endBuffer = WB->startBuffer + L2->mem_latency;
      }else{
        WB->valid = 1;
        WB->startBuffer = cycle_number + L2->mem_latency;
        WB->endBuffer = WB->startBuffer + L2->mem_latency;
      }
    }else{
      WB->valid = 1;
      WB->startBuffer = cycle_number + L2->mem_latency;
      WB->endBuffer = WB->startBuffer + L2->mem_latency;
    }
    
  }else{
    if(WB->valid){
        if(cycle_number< WB->endBuffer){
          latency = latency + WB->endBuffer - cycle_number;
          WB->valid = 0;
        }else{
          WB->valid = 0;
        }
      }
  }
  
  latency = latency + L2->mem_latency;		/* for reading the block from memory*/
  L2->blocks[index][way].tag = tag ;
  updateLRU(L2, index, way) ;
  L2->blocks[index][i].dirty = access_type ;
  return(latency) ;
}



int cache_access(struct cache_t *cp, struct cache_t *L2, unsigned long address, int access_type /*0 for read, 1 for write*/, struct write_buffer_t *WB,unsigned int cycle_number)
{
int i,latency ;
int block_address ;
int index ;
int tag ;
int L2_index ;
int L2_tag ;
int way ;
int max ;

block_address = (address / cp->blocksize);
tag = block_address / cp->nsets;
index = block_address - (tag * cp->nsets) ;

block_address = (address / L2->blocksize);
L2_tag = block_address / L2->nsets;
L2_index = block_address - (L2_tag * L2->nsets) ;
latency = 0;
for (i = 0; i < cp->assoc; i++) {	/* look for the requested block */
  if (cp->blocks[index][i].tag == tag && cp->blocks[index][i].valid == 1) {
    updateLRU(cp, index, i) ;
    if (access_type == 1) {
      cp->blocks[index][i].dirty = 1 ;
    }
    return(latency);					/* a cache hit */
  }
}
/* a cache miss */
for (way=0 ; way< cp->assoc ; way++){		/* look for an invalid entry */
    if (cp->blocks[index][way].valid == 0) {
    	  latency = latency + cp->mem_latency;	/* account for reading the block from memory*/
        cycle_number = cycle_number + cp->mem_latency;
      
    		latency += L2_cache_access(L2, address, access_type,WB,cycle_number);
    		/* should instead read from L2, in case you have an L2 */
        cp->blocks[index][way].valid = 1 ;
        cp->blocks[index][way].tag = tag ;
        cp->blocks[index][way].L2_tag = L2_tag;
    	  cp->blocks[index][way].LRU = way;
    	  updateLRU(cp, index, way); 
    	  cp->blocks[index][way].dirty = 0;
        if(access_type == 1){
          cp->blocks[index][way].dirty = 1 ;
        }
  	    return(latency);				/* an invalid entry is available*/
    }
}
//if eviction
 max = cp->blocks[index][0].LRU ;	/* find the LRU block - the block with the largest LRU field */
 way = 0 ;
 for (i=1 ; i< cp->assoc ; i++){  
    if (cp->blocks[index][i].LRU > max) {
      max = cp->blocks[index][i].LRU ;
      way = i ;
    }
  }
  //cp->blocks[index][way] is the LRU block.
  if (cp->blocks[index][way].dirty == 1) { 
  	latency = latency + cp->mem_latency;	/* for writing back the evicted block */
    cycle_number = cycle_number + cp->mem_latency;
    L2_set_dirty(L2_tag, L2, L2_index);
  }
  latency = latency + cp->mem_latency;		/* for reading the block from memory*/
  cycle_number = cycle_number + cp->mem_latency;
  latency += L2_cache_access(L2, address, access_type,WB, cycle_number);				/* should instead write to and/or read from L2, in case you have an L2 */
  cp->blocks[index][way].tag = tag ;
  cp->blocks[index][way].L2_tag = L2_tag ;
  updateLRU(cp, index, way) ;
  //way not i
  cp->blocks[index][way].dirty = access_type ;
  return(latency) ;
}

