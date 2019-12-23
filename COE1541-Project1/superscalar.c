	/**************************************************************/
/* CS/COE 1541				 			
	 compile with gcc -o five_stage five_stage.c			
	 and execute using							
	 ./five_stage  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	0  
***************************************************************/

#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "CPU.h"
int data_dependency(struct instruction *tr_entry1, struct instruction *tr_entry2){
	switch(tr_entry1->type) {
		case ti_LOAD:
		case ti_RTYPE:
		case ti_ITYPE:
			switch(tr_entry2->type) {
				case ti_STORE:
				case ti_RTYPE:
				case ti_BRANCH:
					if((tr_entry1->dReg == tr_entry2->sReg_a)||(tr_entry1->dReg == tr_entry2->sReg_b)){
						return 1;
					}
					break;
				case ti_JRTYPE:
				case ti_ITYPE:
				case ti_LOAD:
					if(tr_entry1->dReg == tr_entry2->sReg_a){
						return 1;
					}
			}
	}
	return 0;
}
int main(int argc, char **argv)
{
	struct instruction no_op;
	no_op.type = ti_NOP;
	struct instruction *tr_entry1, *tr_entry2 ;
	struct instruction PCregister1, IF_ID_ALU, ID_EX_ALU, EX_MEM_ALU, MEM_WB_ALU, IF_ID_LS, ID_EX_LS, EX_MEM_LS, MEM_WB_LS, PCregister2; 
	size_t size1, size2;
	char *trace_file_name;
	int trace_view_on = 0;
	int flush_counter = 4; //5 stage pipeline, so we have to execute 4 instructions once trace is done
	
	unsigned int cycle_number = 0;

	if (argc == 1) {
		fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
		fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
		exit(0);
	}
		
	trace_file_name = argv[1];
	if (argc == 3) trace_view_on = atoi(argv[2]) ;

	fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

	trace_fd = fopen(trace_file_name, "rb");

	if (!trace_fd) {
		fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
		exit(0);
	}

	trace_init();
	int fill1=0, fill2=1;
	while(1) {
		if(fill2){
			size1 = trace_get_item(&tr_entry1); // put the instruction into a buffer 
			if(size1){
				size2 = trace_get_item(&tr_entry2);
			} 	
		}else if(fill1){
			tr_entry1 = tr_entry2;
			size1 = size2;
			if(size1){
				size2 = trace_get_item(&tr_entry2);
			}
		}
		if(!size2){
			tr_entry2= &no_op;
		}
		
		if (!size1 && flush_counter==0) {       /* no more instructions to simulate */
			printf("+ Simulation terminates at cycle : %u\n", cycle_number);
			break;
		}
		else{              /* move the pipeline forward */
			cycle_number++;
			
			
			//DYNAMIC LW DATA HAZARD SET FILL1 AND FILL2 to 0
			int stall = 0;
			switch(ID_EX_LS.type){
				case ti_LOAD:
					switch(IF_ID_ALU.type) {
							case ti_RTYPE:
							case ti_BRANCH:
								if((ID_EX_LS.dReg == IF_ID_ALU.sReg_a)||(ID_EX_LS.dReg == IF_ID_ALU.sReg_b)){
									stall=1;
								}
								break;
							case ti_JRTYPE:
							case ti_ITYPE:
								if(ID_EX_LS.dReg == IF_ID_ALU.sReg_a){
									stall=1;
								}
					}switch(IF_ID_LS.type) {
							case ti_STORE:
								if((ID_EX_LS.dReg == IF_ID_LS.sReg_a)||(ID_EX_LS.dReg == IF_ID_LS.sReg_b)){
									stall=1;
								}
								break;
							case ti_LOAD:
								if(ID_EX_LS.dReg == IF_ID_LS.sReg_a){
									stall=1;
								}
					}

					
			}
			int control = 0;
			switch(IF_ID_ALU.type){
				case ti_BRANCH:
				case ti_JRTYPE:
				case ti_JTYPE:
					control=1;
			}
			
			/* move instructions one stage ahead */
			if(!stall){
				if(!control){
					MEM_WB_ALU = EX_MEM_ALU;
					EX_MEM_ALU = ID_EX_ALU;
					ID_EX_ALU = IF_ID_ALU;
					IF_ID_ALU = PCregister1;
					MEM_WB_LS = EX_MEM_LS;
					EX_MEM_LS = ID_EX_LS;
					ID_EX_LS = IF_ID_LS;
					IF_ID_LS = PCregister2;
				}else{
					fill1=0;
					fill2=0;
					MEM_WB_ALU = EX_MEM_ALU;
					EX_MEM_ALU = ID_EX_ALU;
					ID_EX_ALU = IF_ID_ALU;
					memcpy(&IF_ID_ALU, &no_op , sizeof(IF_ID_ALU));
					MEM_WB_LS = EX_MEM_LS;
					EX_MEM_LS = ID_EX_LS;
					ID_EX_LS = IF_ID_LS;
					memcpy(&IF_ID_LS, &no_op , sizeof(IF_ID_LS));
				}
			}else{
				fill1=0;
				fill2=0;
				MEM_WB_ALU = EX_MEM_ALU;
				EX_MEM_ALU = ID_EX_ALU;
				memcpy(&ID_EX_ALU, &no_op , sizeof(ID_EX_ALU));
				MEM_WB_LS = EX_MEM_LS;
				EX_MEM_LS = ID_EX_LS;
				memcpy(&ID_EX_LS, &no_op , sizeof(ID_EX_LS));
			}
			if(!size1&&!stall&&!control){    /* if no more instructions in trace, reduce flush_counter */
				flush_counter--;   
			}
			else if(!stall && !control){   /* copy trace entry into IF stage */
				//check type of both if tr_entry1 is ALU PCRegister1 if LS put in PCRegister2
				//check tr_entry2 if it is the same load noop if not put in other register set flags here
				switch(tr_entry1->type) {
					case ti_LOAD:
					case ti_STORE:
						memcpy(&PCregister2, tr_entry1 , sizeof(PCregister2));
						if(!data_dependency(tr_entry1,tr_entry2)){
							switch(tr_entry2->type) {
								case ti_LOAD:
								case ti_STORE:
									fill1=1;
									fill2 =0;
									memcpy(&PCregister1, &no_op, sizeof(PCregister1));
									break;
								default:
									fill2=1;
									fill1=0;
									memcpy(&PCregister1, tr_entry2 , sizeof(PCregister1));	
							}	
						}else{
							memcpy(&PCregister1, &no_op , sizeof(PCregister1));
							fill1 =1;
							fill2 = 0;
						}
						break;
					case ti_BRANCH:
					case ti_JTYPE:
					case ti_JRTYPE:
						memcpy(&PCregister1, tr_entry1 , sizeof(PCregister1));
						fill1=1;
						fill2 =0;
						memcpy(&PCregister2, &no_op , sizeof(PCregister2));
						break;
					default:
						memcpy(&PCregister1, tr_entry1 , sizeof(PCregister1));
						if(!data_dependency(tr_entry1,tr_entry2)){
							switch(tr_entry2->type) {
								case ti_LOAD:
								case ti_STORE:
									fill1=0;
									fill2 =1;
									memcpy(&PCregister2, tr_entry2 , sizeof(PCregister2));
									break;
								default:
									fill2=0;
									fill1=1;
									memcpy(&PCregister2, &no_op , sizeof(PCregister2));	
							}
						}else{
							memcpy(&PCregister2, &no_op , sizeof(PCregister2));
							fill1 =1;
							fill2 = 0;
						}
				}
			}

			//printf("==============================================================================\n");
		}  


		if (trace_view_on && cycle_number>=5) {/* print the instruction exiting the pipeline if trace_view_on=1 */
			switch(MEM_WB_ALU.type) {
				case ti_NOP:
					printf("[cycle %d] NOP:\n",cycle_number) ;
					break;
				case ti_RTYPE: /* registers are translated for printing by subtracting offset  */
					printf("[cycle %d] RTYPE:",cycle_number) ;
		  			printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", MEM_WB_ALU.PC, MEM_WB_ALU.sReg_a, MEM_WB_ALU.sReg_b, MEM_WB_ALU.dReg);
					break;
				case ti_ITYPE:
					printf("[cycle %d] ITYPE:",cycle_number) ;
		  			printf(" (PC: %d)(sReg_a: %d)(dReg: %d)(addr: %d)\n", MEM_WB_ALU.PC, MEM_WB_ALU.sReg_a, MEM_WB_ALU.dReg, MEM_WB_ALU.Addr);
					break;
				case ti_LOAD:
					printf("[cycle %d] LOAD:",cycle_number) ;      
		  			printf(" (PC: %d)(sReg_a: %d)(dReg: %d)(addr: %d)\n", MEM_WB_ALU.PC, MEM_WB_ALU.sReg_a, MEM_WB_ALU.dReg, MEM_WB_ALU.Addr);
					break;
				case ti_STORE:
					printf("[cycle %d] STORE:",cycle_number) ;      
		  			printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(addr: %d)\n", MEM_WB_ALU.PC, MEM_WB_ALU.sReg_a, MEM_WB_ALU.sReg_b, MEM_WB_ALU.Addr);
					break;
				case ti_BRANCH:
					printf("[cycle %d] BRANCH:",cycle_number) ;
		  			printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(addr: %d)\n", MEM_WB_ALU.PC, MEM_WB_ALU.sReg_a, MEM_WB_ALU.sReg_b, MEM_WB_ALU.Addr);
					break;
				case ti_JTYPE:
					printf("[cycle %d] JTYPE:",cycle_number) ;
		  			printf(" (PC: %d)(addr: %d)\n", MEM_WB_ALU.PC, MEM_WB_ALU.Addr);
					break;
				case ti_SPECIAL:
					printf("[cycle %d] SPECIAL:\n",cycle_number) ;      	
					break;
				case ti_JRTYPE:
					printf("[cycle %d] JRTYPE:",cycle_number) ;
		  			printf(" (PC: %d) (sReg_a: %d)(addr: %d)\n", MEM_WB_ALU.PC, MEM_WB_ALU.dReg, MEM_WB_ALU.Addr);
					break;
			}
			switch(MEM_WB_LS.type) {
				case ti_NOP:
					printf("[cycle %d] NOP:\n",cycle_number) ;
					break;
				case ti_RTYPE: /* registers are translated for printing by subtracting offset  */
					printf("[cycle %d] RTYPE:",cycle_number) ;
		  			printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", MEM_WB_LS.PC, MEM_WB_LS.sReg_a, MEM_WB_LS.sReg_b, MEM_WB_LS.dReg);
					break;
				case ti_ITYPE:
					printf("[cycle %d] ITYPE:",cycle_number) ;
		  			printf(" (PC: %d)(sReg_a: %d)(dReg: %d)(addr: %d)\n", MEM_WB_LS.PC, MEM_WB_LS.sReg_a, MEM_WB_LS.dReg, MEM_WB_LS.Addr);
					break;
				case ti_LOAD:
					printf("[cycle %d] LOAD:",cycle_number) ;      
		  			printf(" (PC: %d)(sReg_a: %d)(dReg: %d)(addr: %d)\n", MEM_WB_LS.PC, MEM_WB_LS.sReg_a, MEM_WB_LS.dReg, MEM_WB_LS.Addr);
					break;
				case ti_STORE:
					printf("[cycle %d] STORE:",cycle_number) ;      
		  			printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(addr: %d)\n", MEM_WB_LS.PC, MEM_WB_LS.sReg_a, MEM_WB_LS.sReg_b, MEM_WB_LS.Addr);
					break;
				case ti_BRANCH:
					printf("[cycle %d] BRANCH:",cycle_number) ;
		  			printf(" (PC: %d)(sReg_a: %d)(sReg_b: %d)(addr: %d)\n", MEM_WB_LS.PC, MEM_WB_LS.sReg_a, MEM_WB_LS.sReg_b, MEM_WB_LS.Addr);
					break;
				case ti_JTYPE:
					printf("[cycle %d] JTYPE:",cycle_number) ;
		  			printf(" (PC: %d)(addr: %d)\n", MEM_WB_LS.PC, MEM_WB_LS.Addr);
					break;
				case ti_SPECIAL:
					printf("[cycle %d] SPECIAL:\n",cycle_number) ;      	
					break;
				case ti_JRTYPE:
					printf("[cycle %d] JRTYPE:",cycle_number) ;
		  			printf(" (PC: %d) (sReg_a: %d)(addr: %d)\n", MEM_WB_LS.PC, MEM_WB_LS.dReg, MEM_WB_LS.Addr);
					break;
			}

		}
	}

	trace_uninit();

	exit(0);
}


