/**************************************************************/
/* CS/COE 1541				 			
   compile with gcc -o pipeline five_stage+.c			
   and execute using							
   ./pipeline  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	0  
***************************************************************/

#include <string.h>
#include <stdio.h>
#include<stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "CPU.h"
#include "cache.h" 




// to keep cache statistics
unsigned int I_accesses = 0;
unsigned int I_misses = 0;
unsigned int D_accesses = 0;
unsigned int D_misses = 0;
unsigned int L2_accesses = 0;
unsigned int L2_misses = 0;



int main(int argc, char **argv)
{
  // pointer to new instruction
  struct instruction *tr_entry;
  struct write_buffer_t *WB = (struct write_buffer_t *)calloc(1, sizeof(struct write_buffer_t));
  WB->valid = 0 ;
  //different stages of the pipeline
  struct instruction PCregister, IF_ID, ID_EX, EX_MEM, MEM_WB; 

  int	 PCregister_Valid, IF_ID_Valid, ID_EX_Valid, EX_MEM_Valid, MEM_WB_Valid;
  //initially, the interstage buffer do not contain valid instructions
  PCregister_Valid = 0; IF_ID_Valid = 0; ID_EX_Valid = 0; EX_MEM_Valid = 0; MEM_WB_Valid = 0;
  

  size_t size;
  char *trace_file_name;
  int trace_view_on = 0;
  int flush_counter = 4; //5 stage pipeline, so we have to execute 4 instructions once trace is done
  
  unsigned int cycle_number = 0;
unsigned int count = 0;
  if (argc == 1) {
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
    exit(0);
  }
    

  trace_file_name = argv[1];
  //atoi converts a string to an integer
  if (argc == 3) trace_view_on = atoi(argv[2]) ;

  // here you should extract the cache parameters from the configuration file 
  // read in the coniguration file and set these variables to the values in the configuration file
  // make a struct maybe?
  unsigned int I_size = 2;
  unsigned int I_assoc = 4;
  unsigned int D_size = 1;
  unsigned int D_assoc = 4;
  unsigned int L2_size = 0;
  unsigned int L2_assoc = 0;
  unsigned int Bsize = 16;
  unsigned int L2_access_time = 0;
  unsigned int mem_access_time = 60;
  unsigned int warmup_instructions;
  unsigned int latency;
  int startBuffer=-1;
  int endBuffer=-1;

  FILE* file = fopen ("cache_config.txt", "r");
  
  fscanf (file, "%d", &I_size);
  //printf("%d ", I_size);
  fscanf (file, "%d", &I_assoc);
  //printf("%d ", I_assoc);
  fscanf (file, "%d", &D_size);
  fscanf (file, "%d", &D_assoc);
  fscanf (file, "%d", &L2_size);
  fscanf (file, "%d", &L2_assoc);
  fscanf (file, "%d", &Bsize);
  fscanf (file, "%d", &L2_access_time);
  fscanf (file, "%d", &mem_access_time);
  fscanf (file, "%d", &warmup_instructions);
  //printf("%d ", warmup_instructions);
  fclose (file);      


  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

  trace_fd = fopen(trace_file_name, "rb");

  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }
  trace_init();
  struct cache_t *I_cache, *D_cache, *L2_cache;
  I_cache = cache_create(I_size, Bsize, I_assoc, L2_access_time);
  D_cache = cache_create(D_size, Bsize, D_assoc, L2_access_time);
  L2_cache = cache_create(L2_size, Bsize, L2_assoc, mem_access_time);
  while(1) {
    size = trace_get_item(&tr_entry); /* put the instruction into a buffer */
    if (!size && flush_counter==0) {       /* no more instructions (instructions) to simulate */
      if(WB->endBuffer > cycle_number){
        cycle_number=WB->endBuffer;
      }
      L2_accesses = I_misses + D_misses;
      printf("+ Simulation terminates at cycle : %u\n", (cycle_number-warmup_instructions));
  	  printf("I-cache: %u accesses, %u misses, miss rate = %5.2f \n", I_accesses, I_misses, (float)I_misses/I_accesses);
  	  printf("D-cache: %u Reads, %u Read misses, Read miss rate = %5.2f \n", D_accesses, D_misses, (float)D_misses / D_accesses);
      printf("L2-cache: %u Reads, %u Read misses, Read miss rate = %5.2f \n", L2_accesses, L2_misses, (float)L2_misses / L2_accesses);
      break;
    }
    else{              /* move the pipeline forward */
      cycle_number++;
      count++;
      /* move instructions one stage ahead */
      MEM_WB = EX_MEM;
	  MEM_WB_Valid = EX_MEM_Valid;
	  if (EX_MEM.type == ti_LOAD && EX_MEM_Valid == 1)
	  {   // stall the pipe if data fetch (read) returns a miss
		  latency = cache_access(D_cache, L2_cache, EX_MEM.Addr, 0, WB,cycle_number); /* simulate data cache access */

      if(cycle_number>=warmup_instructions){
  		  cycle_number = cycle_number + latency;
  		  D_accesses++;
  		  if (latency > 0) D_misses++;
        if(latency> (2*L2_access_time)){
          L2_misses++;
        }
      }
	  };
	  if (EX_MEM.type == ti_STORE && EX_MEM_Valid == 1)
	  {	  // stall the pipe if data fetch (write) returns a miss
		  latency = cache_access(D_cache, L2_cache, EX_MEM.Addr, 1, WB,cycle_number); /* simulate data cache access */
      if(cycle_number>=warmup_instructions){
    		  cycle_number = cycle_number + latency;
    		  D_accesses++;
    		  if (latency > 0) D_misses++;
          if(latency> (2*L2_access_time)){
            L2_misses++;
          }
      }
	  };
      EX_MEM = ID_EX;
	  EX_MEM_Valid = ID_EX_Valid;
      ID_EX = IF_ID;
	  ID_EX_Valid = IF_ID_Valid;
      IF_ID = PCregister;
	  IF_ID_Valid = PCregister_Valid;
	  // stall the pipe if instruction fetch returns a miss 
	  if (PCregister_Valid == 1) {
		  latency = cache_access(I_cache, L2_cache, PCregister.PC, 0, WB,cycle_number); /* simulate instruction fetch */
      if(cycle_number>=warmup_instructions){
  		  I_accesses++;
  		  if (latency > 0)
  		  {
  			  cycle_number = cycle_number + latency;
  			  I_misses++;
  		  }
        if(latency> (2*L2_access_time)){
          L2_misses++;
        }
      }
	  }

	  if (!size){    /* if no more instructions in trace, reduce flush_counter */
		  flush_counter--; 
		  PCregister_Valid = 0;
	  }
	  else {										/* copy trace entry into the pipeline */
		  memcpy(&PCregister, tr_entry, sizeof(PCregister));
		  PCregister_Valid = 1;
	  }

      //printf("==============================================================================\n");
    }  


    if (trace_view_on && count>=5) {/* print the instruction exiting the pipeline if trace_view_on=1 */
      switch(MEM_WB.type) {
        case ti_NOP:
          printf("[cycle %d] NOP:\n",cycle_number) ;
          break;
        case ti_RTYPE: /* registers are translated for printing by subtracting offset  */
          printf("[cycle %d] RTYPE:",cycle_number) ;
		  printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", MEM_WB.PC, MEM_WB.sReg_a, MEM_WB.sReg_b, MEM_WB.dReg);
          break;
        case ti_ITYPE:
          printf("[cycle %d] ITYPE:",cycle_number) ;
		  printf(" (PC: %d)(sReg_a: %d)(dReg: %d)(addr: %d)\n", MEM_WB.PC, MEM_WB.sReg_a, MEM_WB.dReg, MEM_WB.Addr);
          break;
        case ti_LOAD:
          printf("[cycle %d] LOAD:",cycle_number) ;      
		  printf(" (PC: %d)(sReg_a: %d)(dReg: %d)(addr: %d)\n", MEM_WB.PC, MEM_WB.sReg_a, MEM_WB.dReg, MEM_WB.Addr);
          break;
        case ti_STORE:
          printf("[cycle %d] STORE:",cycle_number) ;      
		  printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(addr: %d)\n", MEM_WB.PC, MEM_WB.sReg_a, MEM_WB.sReg_b, MEM_WB.Addr);
          break;
        case ti_BRANCH:
          printf("[cycle %d] BRANCH:",cycle_number) ;
		  printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(addr: %d)\n", MEM_WB.PC, MEM_WB.sReg_a, MEM_WB.sReg_b, MEM_WB.Addr);
          break;
        case ti_JTYPE:
          printf("[cycle %d] JTYPE:",cycle_number) ;
		  printf(" (PC: %d)(addr: %d)\n", MEM_WB.PC, MEM_WB.Addr);
          break;
        case ti_SPECIAL:
          printf("[cycle %d] SPECIAL:\n",cycle_number) ;      	
          break;
        case ti_JRTYPE:
          printf("[cycle %d] JRTYPE:",cycle_number) ;
		  printf(" (PC: %d) (sReg_a: %d)(addr: %d)\n", MEM_WB.PC, MEM_WB.dReg, MEM_WB.Addr);
          break;
      }
    }
  }

  trace_uninit();

  exit(0);
}


