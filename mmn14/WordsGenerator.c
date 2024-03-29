#include <stdio.h>

#include "WordsGenerator.h"
#include "GeneralUtils.h"
#include "OperationData.h"

/*
	this method checks if the op type is regist
*/
int isRegist(Op* op) {
	return op->operands == 2 && op->src.type == regist && op->dst.type == regist;
}

/*
	this method handles the special case of two register operands sharing a single memory word
*/
int generateWordForRegisters(char src_reg[3], char dst_reg[3])
{
	int word = src_reg[1] - REGISTER_MIN;
	word <<= REGISTER_BIT_WIDTH;
	word += (dst_reg[1] - REGISTER_MIN);
	word <<= ARE_BIT_WIDTH;
	word += ARE_ABSOLUTE;
	return word;
}

/*
	this method calculate a word based on an operand anf filecontext when the operand is represent memory
*/
int handleMemoryOperandType(Operand *operand, FileContext *fileContext) {
	int word;
	Symbol* symbol;

	symbol = getSymbolFromFileContext(operand->data.label, fileContext);
	if (symbol != NULL)
	{
		word = symbol->location;
		word = (word << ARE_BIT_WIDTH) + ARE_RELOCATABLE;
	}
	else
	{
		word = 0;
		word = (word << ARE_BIT_WIDTH) + ARE_EXTERNAL;
	}
	return word;
}




/*
	general method for building a word from an Op struct
*/
int buildWordFromOp(Op* op)
{
	int destinationAddress, sourceAddress, op1Address = 0,op2Address = 0;

	if (op->operands > 0) { /* dest exist */
		destinationAddress = getAddressType(op->dst.type);
		if (destinationAddress == ADDR_JUMP)
		{
			op1Address = getAddressTypeJumpOp(op->dst.data.jump_data.op1Type);
			op2Address = getAddressTypeJumpOp(op->dst.data.jump_data.op2Type);
		}

	}
	else {
		destinationAddress = 0;
	}

	if (op->operands == 2) { /* src exist */
		sourceAddress = getAddressType(op->src.type);
	}
	else {
		sourceAddress = 0;
	}

	return calcWord(op, destinationAddress, sourceAddress, op1Address,op2Address);
}

/*
	this method calculate a word based on the Op and the src\dest address
*/
int calcWord(Op* op, int destinationAddress, int sourceAddress, int op1Address, int op2Address) {
	int word = 0;
	word = (word << ADDRESSING_BIT_WIDTH) + op1Address; /* insert param1 addressing - just if jump */
	word = (word << ADDRESSING_BIT_WIDTH) + op2Address; /* insert param2 addressing - just if jump */
	word = (word << OPCODE_BIT_WIDTH) + op->op_code;
	word = (word << ADDRESSING_BIT_WIDTH) + sourceAddress;
	word = (word << ADDRESSING_BIT_WIDTH) + destinationAddress;
	word = (word << ARE_BIT_WIDTH) + ARE_ABSOLUTE;

	return word;
}

/*
	this method generates a word from an operand and a filecontext only Address related
*/
void generateWordWithOperandAddressing(int* words, FileContext* FileContext, Operand* operand, OperandType type, int* wordsIndex)
{
	if (operand->type == jump) { /*if jump - call to func for jump */
		generateWordWithOperandAddressingForJump(words, FileContext, operand, type, wordsIndex);
		return;
	}

	generateWordWithOperandAddressingForNotJump(words, FileContext, operand, type, wordsIndex);
}

/*
	this method generates a word from an operand and a filecontext only not jump related
*/
void generateWordWithOperandAddressingForNotJump(int* words, FileContext* FileContext, Operand* operand, OperandType type, int* wordsIndex) {
	words[*wordsIndex] = generateWordForNonJumpOp(operand, FileContext, type);
	(*wordsIndex)++;
}

/*
	creating memory word for non jump
*/
int generateWordForNonJumpOp(Operand* operand, FileContext* FileContext, OperandType type)
{
	if (operand->type == immediate) {
		return handleImmediateOperandType(operand);
	}
	else if (operand->type == regist) {
		return handleRegistOperandType(operand, type);
	}
	else if (operand->type == memory) {
		return handleMemoryOperandType(operand, FileContext);
	}

	return 0;
}

/*
	this method generates a word from an operand and a filecontext only jump related
*/
void generateWordWithOperandAddressingForJump(int* words, FileContext* FileContext, Operand* operand, OperandType type, int* wordsIndex) {
	int word;
	words[*wordsIndex] = generateJumpOperandWord(operand, FileContext);
	(*wordsIndex)++;
	if (operand->data.jump_data.op1Type == isRegister &&
		operand->data.jump_data.op2Type == isRegister) /* both params are register */
	{
		word = operand->data.jump_data.register1;
		word = (word << REGISTER_BIT_WIDTH) + operand->data.jump_data.register2;
		words[*wordsIndex] = (word << ARE_BIT_WIDTH) + ARE_ABSOLUTE; /* A,E,R */
	}
	else
	{
		words[*wordsIndex] = generateWordForJumpParameter1(operand, FileContext); /* generate param1 word */
		(*wordsIndex)++;
		words[*wordsIndex] = generateWordForJumpParameter2(operand, FileContext); /* generate param2 word*/
	}
	(*wordsIndex)++;
}
/*
	this method generates a word from parameter1 in jump
*/
int generateWordForJumpParameter1(Operand *op, FileContext* FileContext)
{
	int word = 0;
	if (op->data.jump_data.op1Type == isRegister) { /* param1 is regitser */
		word = op->data.jump_data.register1;
		word = (word << (REGISTER_BIT_WIDTH + ARE_BIT_WIDTH)) + ARE_ABSOLUTE; /* A,E,R */
	} else if(op->data.jump_data.op1Type == isLabel) {
		word = handleJumpMemoryParameterType(op->data.jump_data.op1Label,FileContext);
	} else if (op->data.jump_data.op1Type == isNumber) {
		word = op->data.jump_data.num1;
		word = (word << ARE_BIT_WIDTH) + ARE_ABSOLUTE; /* A,E,R */
	}
	return word;
}

/*
	this method generates a word from parameter2 in jump
*/
int generateWordForJumpParameter2(Operand *op, FileContext* FileContext)
{
	int word = 0;
	if (op->data.jump_data.op2Type == isRegister) { /* param2 is register */
		word = op->data.jump_data.register2;
		word = (word << ARE_BIT_WIDTH) + ARE_ABSOLUTE; /* A,E,R */
	} else if(op->data.jump_data.op2Type == isLabel) {
		word = handleJumpMemoryParameterType(op->data.jump_data.op2Label , FileContext); /* generate word for label*/
	} else if (op->data.jump_data.op2Type == isNumber) {
		word = op->data.jump_data.num2;
		word = (word << ARE_BIT_WIDTH) + ARE_ABSOLUTE; /* A,E,R */
	}
	return word;
}
/*
	this method calculate a word based on an parameter anf filecontext when the parameter is represent memory
*/
int handleJumpMemoryParameterType(char *label, FileContext *fileContext) {
	int word;
	Symbol* symbol;

	symbol = getSymbolFromFileContext(label, fileContext);
	if (symbol != NULL)
	{  /* label is exist in symbol list - return label location */
		word = symbol->location;
		word = (word << ARE_BIT_WIDTH) + ARE_RELOCATABLE;
	}
	else  /* label is external */
	{
		word = 0;
		word = (word << ARE_BIT_WIDTH) + ARE_EXTERNAL;
	}
	return word;
}

/*
	this method calculate a word based on an operand anf filecontext when the operand is represent Immediate
*/
int handleImmediateOperandType(Operand *operand) {
	int word;
	word = operand->data.number;
	word = (word << ARE_BIT_WIDTH) + ARE_ABSOLUTE; /* A,E,R */
	return word;
}

/*
	this method calculate a word based on an operand anf filecontext when the operand is represent regist
*/
int handleRegistOperandType(Operand *operand, OperandType type) {
	int word;
	word = operand->data.reg[1] - REGISTER_MIN;
	if (type == dst)
		word = (word << ARE_BIT_WIDTH) + ARE_ABSOLUTE; /* A,E,R */
	else
		word = (word << (REGISTER_BIT_WIDTH + ARE_BIT_WIDTH)) + ARE_ABSOLUTE;
	return word;
}


/*
	this method generates machine codes for words in the FileContext
*/
void generateWordsInMemory(FileContext* FileContext, int* words)
{
	int i, j, wordIndex = 0;
	for (i = 0; i < FileContext->operationData->operationsCounter; i++) /* generate operation words */
	{
		Op* op = &FileContext->operationData->operationsTable[i];
		words[wordIndex++] = buildWordFromOp(op); /* build the first word */
		if (op->operands == 0) {
			continue;
		}
		else if (isRegist(op)) { /* register address */
			words[wordIndex++] = generateWordForRegisters(op->src.data.reg, op->dst.data.reg);
			continue;
		}
		else if (op->operands == 2) {
			generateWordWithOperandAddressing(words, FileContext, &op->src, src, &wordIndex);
			generateWordWithOperandAddressing(words, FileContext, &op->dst, dst, &wordIndex);
		}
		else if (op->operands == 1) { /* just 1 operand - generate dest */
			generateWordWithOperandAddressing(words, FileContext, &op->dst, dst, &wordIndex);
			continue;
		}
	}

	for (j = 0; j < FileContext->data_count; ) /* generate data words */
	{
		words[wordIndex++] = FileContext->data_table[j++];
	}
}


/*
	this method generates jump operand word based on the operand and the filecontext
*/
int generateJumpOperandWord(Operand* operand, FileContext* FileContext)
{
	Symbol* symbol = getSymbolFromFileContext(operand->data.jump_data.label, FileContext);

	return symbol == NULL ? calcWordForNullSymbolWhenJumpOp() : calcWordForNotNullSymbolWhenJumpOp(symbol);
}

/*
	This method calculate word based on the fact that we work on a null Symbol when jump operation
 - external label
*/
int calcWordForNullSymbolWhenJumpOp() {
	int word = 0;
	word = (word << ARE_BIT_WIDTH) + ARE_EXTERNAL; /* A,E,R */

	return word;
}

/*
	This method calculate word based on the fact that we work on a non null Symbol when jump operation
*/
int calcWordForNotNullSymbolWhenJumpOp(Symbol *symbol) {
	int word;
	word = symbol->location;
	word = (word << ARE_BIT_WIDTH) + ARE_RELOCATABLE;

	return word;
}
