#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <netinet/in.h>
#include "emulator.h"

#define XSTR(x) STR(x)		//can be used for MAX_ARG_LEN in sscanf
#define STR(x) #x

// These are some macro variables and function to simplify translation between 
// memory addresses and array indices.
#define ADDR_TEXT    0x00400000 //where the .text area starts in which the program lives
#define ADDR_DATA    0x10010000 //where the .text area starts in which the program lives
#define TEXT_POS(a)  ((a==ADDR_TEXT)?(0):(a - ADDR_TEXT)/4) //can be used to access text[]
#define TEXT_ADDR_POS(j)  (j*4 + ADDR_TEXT)                      //convert text index to address
#define DATA_POS(a)  ((a==ADDR_DATA)?(0):(a - ADDR_DATA)/4) //can be used to access data_str[]
#define DATA_ADDR_POS(j)  (j*4 + ADDR_DATA)                      //convert data index to address



const char *register_str[] = {"$zero",
                              "$at", "$v0", "$v1",
                              "$a0", "$a1", "$a2", "$a3",
                              "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
                              "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
                              "$t8", "$t9",
                              "$k0", "$k1",
                              "$gp",
                              "$sp", "$fp", "$ra"};

/* These variables are used to store the string representation of the code by the 
 * load_program function */

/* Space for the instructions of the assembler program */
char prog_str[MAX_PROG_LEN][MAX_LINE_LEN];
int prog_len = 0;
/* Array to carry the data section of the assembly program */
char data_str[MAX_PROG_LEN][MAX_LINE_LEN];
int data_len = 0;

/* These arrays should store the content of the text and data sections of the program 
 * affer the make_bytecode function. */
unsigned int data[MAX_PROG_LEN] = {0}; // the text memory with our instructions
unsigned int text[MAX_PROG_LEN] = {0}; // the text memory with our instructions

/* Elements for running the emulator. */
unsigned int registers[MAX_REGISTER] = {0}; // the registers
unsigned int pc = 0;                        // the program counter

/* function to create bytecode for instruction nop
   conversion result is passed in bytecode
   function always returns 0 (conversion OK) */
typedef int (*opcode_function)(unsigned int, unsigned int*, char*, char*, char*, char*);

int add_imi(unsigned int *bytecode, short int imi){
	if (imi<-32768 || imi>32767) return (-1);
	*bytecode|= (0xFFFF & imi);
	return(0);
}

int add_sht(unsigned int *bytecode, int sht){
	if (sht<0 || sht>31) return(-1);
	*bytecode|= (0x1F & sht) << 6;
	return(0);
}

int add_reg(unsigned int *bytecode, char *reg, int pos){
	int i;
	for(i=0;i<MAX_REGISTER;i++){
		if(!strcmp(reg,register_str[i])){
		*bytecode |= (i << pos);
			return(0);
		}
	}
	return(-1);
}

int add_addr(unsigned int *bytecode, int addr){
    *bytecode |= ((addr>>2) & 0x3FFFFF);
    return 0;
}

int add_lbl(unsigned int offset, unsigned int *bytecode, char *label){
	char l[MAX_ARG_LEN+1];
	int j=0;
	while(j<prog_len){
		memset(l,0,MAX_ARG_LEN+1);
		sscanf(&prog_str[j][0],"%" XSTR(MAX_ARG_LEN) "[^:]:", l);
		if (label!=NULL && !strcmp(l, label)) return(add_imi( bytecode, j-(offset+1)) );
		j++;
	}
	return (-1);
}

int add_text_addr(unsigned int *bytecode, char *label){
	char l[MAX_ARG_LEN+1];
	int j=0;
	while(j<prog_len){
		memset(l,0,MAX_ARG_LEN+1);
		sscanf(&prog_str[j][0],"%" XSTR(MAX_ARG_LEN) "[^:]:", l);
		if (label!=NULL && !strcmp(l, label)) return(add_addr( bytecode, TEXT_ADDR_POS(j)));
		j++;
	}
	return (-1);
}

int opcode_nop(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0;
	return (0);
}

int opcode_add(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x20; 				// op,shamt,funct
	if (add_reg(bytecode,arg1,11)<0) return (-1); 	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source1 register
	if (add_reg(bytecode,arg3,16)<0) return (-1);	// source2 register
	return (0);
}

int opcode_addi(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x20000000; 				// op
	if (add_reg(bytecode,arg1,16)<0) return (-1);	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source1 register
	if (add_imi(bytecode,(short int)strtol(arg3, NULL, 0))) return (-1);	// constant
	return (0);
}

int opcode_andi(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x30000000; 				// op
	if (add_reg(bytecode,arg1,16)<0) return (-1); 	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source1 register
	if (add_imi(bytecode,(short int)strtol(arg3, NULL, 0))) return (-1);	// constant
	return (0);
}

int opcode_bne(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x14000000; 				// op
	if (add_reg(bytecode,arg1,21)<0) return (-1); 	// register1
	if (add_reg(bytecode,arg2,16)<0) return (-1);	// register2
	if (add_lbl(offset,bytecode,arg3)) return (-1); // jump
	return (0);
}

int opcode_srl(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x2; 					// op
	if (add_reg(bytecode,arg1,11)<0) return (-1);   // destination register
	if (add_reg(bytecode,arg2,16)<0) return (-1);   // source1 register
	if (add_sht(bytecode,atoi(arg3))<0) return (-1);// shift
	return(0);
}

int opcode_sll(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0; 					// op
	if (add_reg(bytecode,arg1,11)<0) return (-1);	// destination register
	if (add_reg(bytecode,arg2,16)<0) return (-1); 	// source1 register
	if (add_sht(bytecode,atoi(arg3))<0) return (-1);// shift
	return(0);
}

int opcode_lui(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x3c000000; 					// op
	if (add_reg(bytecode,arg1,16)<0) return (-1);	// destination register
	if (add_imi(bytecode,(short int)strtol(arg2, NULL, 0))) return (-1);	// constant
	return(0);
}

int opcode_lw(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x8c000000; 					// op
	if (add_reg(bytecode,arg1,16)<0) return (-1);	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source register
	if (add_imi(bytecode,(short int)strtol(arg3, NULL, 0))) return (-1);	// constant
	return(0);
}

int opcode_sw(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0xAC000000; 					// op
	if (add_reg(bytecode,arg1,16)<0) return (-1);	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source register
	if (add_imi(bytecode,(short int)strtol(arg3, NULL, 0))) return (-1);	// constant
	return(0);
}

int opcode_lhu(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0x94000000; 					// op
	if (add_reg(bytecode,arg1,16)<0) return (-1);	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source register
	if (add_imi(bytecode,(short int)strtol(arg3, NULL, 0))) return (-1);	// constant
	return(0);
}

int opcode_sh(unsigned int offset, unsigned int *bytecode, char *opcode, char *arg1, char *arg2, char *arg3 ){
	*bytecode=0xA4000000; 					// op
	if (add_reg(bytecode,arg1,16)<0) return (-1);	// destination register
	if (add_reg(bytecode,arg2,21)<0) return (-1);	// source register
	if (add_imi(bytecode,(short int)strtol(arg3, NULL, 0))) return (-1);	// constant
	return(0);
}


/* These arrays can be used to make your make_bytecode code more readable. You can use the array opcode_funct to store pointers 
 * to instruction serialization functions. You can then use the array opcode_str in order to match instruction 
 * names to function pointers. */
const char *opcode_str[] = {"nop", "add", "addi", "andi", "bne", "srl", "sll", "lui", "lw", "sw", "lhu", "sh"};
opcode_function opcode_func[] = {&opcode_nop, &opcode_add, &opcode_addi, &opcode_andi, &opcode_bne, &opcode_srl, &opcode_sll, &opcode_lui, &opcode_lw, &opcode_sw, &opcode_lhu, &opcode_sh};

/* a function to print the state of the machine */
int print_registers() {
  int i;
  printf("registers:\n");
  for (i = 0; i < MAX_REGISTER; i++) {
    printf(" %d: %d (0x%08x)\n", i, registers[i], registers[i]);
  }
  printf(" Program Counter: 0x%08x\n", pc);
  return (0);
}

/* a function to print the content of the memory in hex */
int print_memory(int start, int len) {
	int i, end = start + len;
  printf("Memory region (0x%08x):\n\t\t", start);
  for (i = 0; i < 8; i++) 
	printf("\tValue(+%02x)", (i*4));
  printf("\n");
  for (; start < end; start += 0x20) {
	printf("0x%08x\t", start);
  	for (i = 0; i < 8; i++) 
		printf("\t0x%08x", ntohl(data[DATA_POS(start + i*4)]));
	printf("\n");
  }
  return (0);
}

// process r type instruction
// bytecode - instruction to process
// does not return any value
void r_type(unsigned int bytecode){
	unsigned int functCode = bytecode & 0x3F;  // extract funct
	unsigned int rs = (bytecode >> 21) & 0x1F; // extract rs register
	unsigned int rt = (bytecode >> 16) & 0x1F; // extract rt register
	unsigned int rd = (bytecode >> 11) & 0x1F; // extract destination register
	unsigned int shamt = (bytecode >> 6) & 0x1F; // extract shift amount
	switch ( functCode ){
		case 0x20: // add
			registers[rd] = registers[rs] + registers[rt];
			break;
		case 0x00: // sll
			registers[rd] = registers[rt] << shamt;
			break;
		case 0x02: // srl
			registers[rd] = registers[rt] >> shamt;
			break;
	}
}

// process i type instruction
// bytecode - instruction to process
// opcode - code of the operation
void i_type(unsigned int bytecode, unsigned int opcode){
	unsigned int rs = (bytecode >> 21) & 0x1F; // extract rs
	unsigned int rt = (bytecode >> 16) & 0x1F; // extarct rt
	short int imm_offset = (bytecode & 0xFFFF); // immediate or offset
	int address; // address where to store or read from
	switch ( opcode ){
		case 0x08: // addi
			registers[rt] = registers[rs] + imm_offset;
			break;
		case 0x0C: // andi
			registers[rt] = registers[rs] & (unsigned short)imm_offset;
			break;
		case 0x05: // bne
			if( registers[rs] != registers[rt] ) {
				pc+= imm_offset * 4;
			}
			break;
		case 0x25: ; // lhu
			address = (imm_offset + registers[rs]); // read from
			if ( address % 4 == 0){ // check if left or right 16 bits (lower or higher)
				registers[rt] = (unsigned short)ntohl(data[DATA_POS((address))]); 
			}
			else{
				registers[rt] = (unsigned short)(ntohl(data[DATA_POS((address))]) >> 16); 
			}
			break;
		case 0x0F: // lui
			registers[rt] = imm_offset<<16;
			break;
		case 0x23: ; // lw
			address = (imm_offset + registers[rs]); // read from
			registers[rt] = ntohl(data[DATA_POS((address))]);
			break;
		case 0x29: ; // sh
			address = (imm_offset + registers[rs]); // store to
			if ( address % 4 == 0 ){ // check if left or right 16 bits (lower or higher)
				data[DATA_POS(address)] = htonl((ntohl(data[DATA_POS(address)]) & 0xFFFF0000) | (short)(registers[rt]));
			}
			else{
				data[DATA_POS(address)] = htonl((registers[rt] << 16) | (short)ntohl(data[DATA_POS(address)]));
			}
			break;
		case 0x2B: ; // sw
			address = (imm_offset + registers[rs]); // store to
			data[DATA_POS(address)] = htonl(registers[rt]);
			break;
	}
}

/* function to execute bytecode */
int exec_bytecode() {
  printf("EXECUTING PROGRAM ...\n");
  pc = ADDR_TEXT; // set program counter to the start of our program

  //registers[29] = 0x7fffeffc; // stack pointer default in MIPS

  unsigned int bytecode = text[TEXT_POS(pc)];
  unsigned int opcode;
  // go through all the instructions
  // increment program counter by 4
  for( ; TEXT_POS(pc) < prog_len; pc+=4, bytecode = text[TEXT_POS(pc)] ){
	if ( bytecode != 0 ){ 
		opcode = bytecode >> 26; // get opcode
		if (opcode == 0){ // r type have opcode = zero
			r_type(bytecode);
		}
		else{
			i_type(bytecode, opcode);
		}
	}
	else{	// if nop instruction - terminate the program
		pc+=4;
		break;
	}
  }

  // print out the state of registers and memory at the end of execution
  print_registers();
  print_memory(ADDR_DATA, 0x40);

  printf("... DONE!\n");
  return (0);
}

/* function to create bytecode */
int make_bytecode() {
  unsigned int
      bytecode; // holds the bytecode for each converted program instruction
  int i, j = 0;    // instruction counter (equivalent to program line)

  char label[MAX_ARG_LEN + 1];
  char opcode[MAX_ARG_LEN + 1];
  char arg1[MAX_ARG_LEN + 1];
  char arg2[MAX_ARG_LEN + 1];
  char arg3[MAX_ARG_LEN + 1];
	char data_line[MAX_LINE_LEN], *val;

  printf("\nASSEMBLING PROGRAM ...\n");
  printf("DATA AREA:\n");

  // First need to parse the data section and store the data in array data.
  i = 0;
  while (j < data_len) {
      if (sscanf(
              &data_str[j][0],
              "%" XSTR(MAX_ARG_LEN) "[^:]: .word %[^\n]",
              label, data_line) < 2) { // parse the line with label
        printf("parse data error line %d %s %s\n", j, label, data_line);
        return (-1);
      }

      // parsing data entry values
      val = strtok(data_line, " ");
      while (val != NULL) {
	      *(data + (i++)) = htonl(strtol(val, NULL, 0));
	      printf("%08x: %08x\n", ADDR_DATA + i*4, ntohl(*(data + i - 1)));
	      val = strtok(NULL, " ");
      };
      j++;
   } 

	j = 0;
  printf("TEXT AREA:\n");
  while (j < prog_len) {
  	// These variables will store the elements of each instruction. 
    memset(label, 0, sizeof(label));
    memset(opcode, 0, sizeof(opcode));
    memset(arg1, 0, sizeof(arg1));
    memset(arg2, 0, sizeof(arg2));
    memset(arg3, 0, sizeof(arg3));

    bytecode = 0;

    //First, parse and remove label. 
    if ((val = strchr(&prog_str[j][0], ':')) != NULL) { // check if the line contains a label
			*val = '\0';
			strcpy(label, &prog_str[j][0]);
			val++;
		} else {
			val = &prog_str[j][0];
		}

    // Parse a load/store instruction using an appropriate pattern match
    // We match a 2 strings up to MAX_ARG_LEN characters, a string of chars and a string
    // that should not contain a closing parentheses.
    // If we do not read 4 arguments, the instruction is not load/store
		if (sscanf(val,"%" XSTR(MAX_ARG_LEN) "s %" XSTR(MAX_ARG_LEN) "s %[A-Fa-f0-9-](%[^)]",
          opcode, arg1, arg3, arg2) < 4) { 
      // Reset the last two arguments to empty strings
      memset(arg2, 0, sizeof(arg2));
    	memset(arg3, 0, sizeof(arg3));

    	// Scan using attern for any other supported instruction
    	// The pattern scans for 4 string of max length MAX_ARG_LEN. 
    	// The scanning will return the number matched fields and our instructions 
    	// need at least two arguments.
      if (sscanf(val, "%" XSTR(MAX_ARG_LEN) "s %" XSTR(
            	MAX_ARG_LEN) "s %" XSTR(MAX_ARG_LEN) "s %" XSTR(MAX_ARG_LEN) "s",
          	opcode, arg1, arg2, arg3) < 1) { // parse the line with label
      	printf("parse error line %d\n", j);
      	return (-1);
      }
    }

    for (i=0; i<MAX_OPCODE; i++){
        if (!strcmp(opcode, opcode_str[i]) && ((*opcode_func[i]) != NULL))
        {
            if ((*opcode_func[i])(j, &bytecode, opcode, arg1, arg2, arg3) < 0)
            {
                printf("ERROR: line %d opcode error (assembly: %s %s %s %s)\n", j, opcode, arg1, arg2, arg3);
                return (-1);
            }
            else
            {
                printf("0x%08x 0x%08x\n", ADDR_TEXT + 4 * j, bytecode);
                text[j] = bytecode;
                break;
            }
        }
        if (i == (MAX_OPCODE - 1))
        {
            printf("ERROR: line %d unknown opcode\n", j);
            return (-1);
        }
    }

    j++;
  }
  printf("... DONE!\n");
  return (0);
}

// A helper function to trim whitespace from front and back of a string
char *trimwhitespace(char *str) {
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

/* loading the program into memory */
int load_program(char *filename) {
  int j = 0;
  FILE *f;
  char line[MAX_LINE_LEN], *str;

  printf("LOADING PROGRAM %s ...\n", filename);

  f = fopen(filename, "r");
  if (f == NULL) {
      printf("ERROR: Cannot open program %s...\n", filename);
      return -1;
  }
	
  // skip any empty lines 
  do {
		if (fgets(&line[0], MAX_LINE_LEN, f) == NULL) {
			fprintf(stderr, "ERROR: Input assembly file is empty\n");
			exit(1);
		}
		str = trimwhitespace(line);
	}	while(!strlen(str));

	// Reading the data section of the file
	if (!strcmp(str, ".data")) {
		while (fgets(&line[0], MAX_LINE_LEN, f) != NULL) {

			// Remove whitespaces
			str = trimwhitespace(line);

			// Skip empty lines
			if(!strlen(str)) continue;

			// terminate loop if beginning of data section found
			if (strcmp(str, ".text") == 0) break;
			strcpy(data_str[data_len++], str);
		}
	}
	
	// Reading the assembly text section

	// Skip the .text macro and get the first instruction
	if(!strcmp(str, ".text")) {
		if (fgets(&line[0], MAX_LINE_LEN, f) == NULL) {
			fprintf(stderr, "ERROR: Input assembly file has no instructions\n");
			exit(1);
		}
	}

	// copy instructions to final prog_str array
	do {
		str = trimwhitespace(line);
		if (!strlen(str)) break;
		strcpy(prog_str[prog_len++], str);
	} while (fgets(&line[0], MAX_LINE_LEN, f) != NULL);

  printf("DATA:\n");
  for (j = 0; j < data_len; j++) {
    printf("%d: %s\n", j, &data_str[j][0]);
  }

  printf("PROGRAM:\n");
  for (j = 0; j < prog_len; j++) {
    printf("%d: %s\n", j, &prog_str[j][0]);
  }

  return (0);
}

