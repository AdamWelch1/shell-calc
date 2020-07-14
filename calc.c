#!/usr/bin/tcc -run

/*
	This is a console calculator application that evaluates mathematical expressions.
	Pretty handy, I'd say ^_^
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <math.h>

#include <termios.h>
#include <unistd.h>

// Sets how far back your expression history goes.
#define EXPR_HIST_SIZE		500

void generateExpressions(uint32_t count, uint32_t maxLen, char *outBuf);
double evaluate(char *expr, uint32_t depth); // depth tracks recursion depth
void hexDump(const uint8_t *buf, uint32_t bufLen);
void addHist(const char *buf);
void setCurHistExpr(const char *buf);
bool histBack(char *buf);
bool histFwd(char *buf);
void histReset();

bool errorFlag = false;
bool debugMode = false;
char *exprHistory[EXPR_HIST_SIZE + 1];
uint32_t exprHistIndex = 0;
uint32_t exprHistCount = 0;
bool clearInput = false;


void terminalSetup(bool reset)
{
	struct termios tios;
	memset(&tios, 0, sizeof(tios));
	int tcRet = tcgetattr(STDIN_FILENO, &tios);
	
	if(reset)
	{
		tios.c_lflag |= (ICANON | ECHO);
		tios.c_cc[VTIME] = 0;
		tios.c_cc[VMIN] = 1;
		
	}
	else
	{
	
		tios.c_lflag &= ~(ICANON | ECHO);
		tios.c_cc[VTIME] = 0;
		tios.c_cc[VMIN] = 0;
	}
	
	tcRet = tcsetattr(STDIN_FILENO, TCSANOW, &tios);
}

void cleanExit(bool doAbort)
{
	terminalSetup(true); // Restore terminal settings
	
	for(uint32_t i = 0; i < EXPR_HIST_SIZE + 1; i++)
	{
		if(exprHistory[i] != 0)
			free(exprHistory[i]);
	}
	
	if(doAbort)
		abort();
}

void sigint_handler(int sig)
{
	clearInput = true;
}

int main(int argc, char **argv)
{
	if(argc == 1)
	{
		char *ptr = argv[0] + strlen(argv[0]) - 1;
		
		while(*ptr != '/' && ptr != argv[0])
			ptr--;
			
		if(*ptr == '/') ptr++;
		
		printf("Usage: %s [-c -d] [expression]\n", ptr);
		printf("This is a simplistic expression calculator that's very easy to use from the shell.\n");
		printf("It can take values in Base 10, 16, or 8. It has some built in constants and\n");
		printf("functions, and one can easily add more functions or constants. Expression inputs\n");
		printf("are evaluated according to the order of operations: PE(MD)(AS).\n\n");
		printf("\t-d\tEnable debug output\n");
		printf("\t-c\tPrint supported constants & functions\n");
		printf("\t-i\tInput mode. Reads expression input from the terminal\n");
		
		printf("\nSupported operators:\n\n");
		printf("\t^ - Exponent\n");
		printf("\t* - Multiply\n");
		printf("\t/ - Divide\n");
		printf("\t%% - Modulus\n");
		printf("\t+ - Addition\n");
		printf("\t- - Subtraction\n\n");
		return -1;
	}
	
	memset(exprHistory, 0, (EXPR_HIST_SIZE + 1)*sizeof(char *));
	bool inputMode = false;
	int argStart = 1;
	for(int i = 0; i < argc; i++)
	{
		if(strcmp(argv[i], "-d") == 0)
		{
			argStart++;
			debugMode = true;
		}
		
		if(strcmp(argv[i], "-i") == 0)
			inputMode = true;
			
		if(strcmp(argv[i], "-c") == 0)
		{
			argStart++;
			
			printf("\t%-6s\t%-15.10f\t%s\n", "pi", 3.1415926535897932384626433,
				   "The ratio of a circle's circumference to its diameter");
			printf("\t%-6s\t%-15.10f\t%s\n", "e", 2.7182818284590452353602874, "Euler's number, base of the natural logarithm");
			
			printf("\n");
			printf("\t%-7s\t%s\n", "sin()", "Sine function");
			printf("\t%-7s\t%s\n", "cos()", "Cosine function");
			printf("\t%-7s\t%s\n", "sqrt()", "Square-root function");
			// printf("\t%-10s\t%s\n",)
			
			printf("\n");
		}
	}
	
	if(argStart >= argc)
		return 0;
		
	char expr[4096] = {0};
	memset(expr, 0, 4096);
	uint32_t bufSpace = 4095;
	uint32_t exprIndex = 0;
	
	if(inputMode)
	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sigint_handler;
		
		sigaction(SIGINT, &sa, 0);
	}
	
	if(inputMode)
	{
		terminalSetup(false);
		printf("Running in input mode. Type 'quit' or 'qq' to exit\n");
		printf("You can use up/down arrow keys to navigate expression history.\n");
		printf("Ctrl-C will clear the current input.\n\n");
		
		while(true)
		{
			errorFlag = false;
			memset(expr, 0, 4096);
			exprIndex = 0;
			
			printf("Enter expression> ");
			fflush(stdout);
			
			bool didPrint = false;
			char temporalBuf = 0;
			while(true)
			{
				// Have Ctrl-C clear the console input, it seems to be standard
				// behavior in the linux terminal...
				if(clearInput == true)
				{
					for(uint32_t i = 0; i < exprIndex; i++)
						printf("\b \b"), expr[i] = 0;
						
					exprIndex = 0;
					clearInput = false;
					
					histReset();
					fflush(stdout);
				}
				
				int readRes = read(STDIN_FILENO, &temporalBuf, 1);
				int ch = temporalBuf;
				
				if(readRes < 1)
				{
					if(didPrint)
					{
						didPrint = false;
						fflush(stdout);
						continue;
					}
					
					usleep(1000 * 10);
					continue;
				}
				
				// Escape sequence
				if(ch == 0x1B)
				{
					usleep(1000 * 20);
					
					int ch1 = getchar();
					int ch2 = getchar();
					
					if(ch1 == EOF || ch2 == EOF)
						continue;
						
					uint32_t oldLen = exprIndex;
					
					if(ch1 == 0x5B && ch2 == 0x41) // Up arrow
						didPrint = histBack(expr);
						
					if(ch1 == 0x5B && ch2 == 0x42) // Down arrow
						didPrint = histFwd(expr);
						
					for(uint32_t i = 0; i < exprIndex && didPrint; i++)
						printf("\b \b");
						
					if(didPrint)
					{
						exprIndex = strlen(expr);
						printf("%s", expr);
					}
					
					continue;
				}
				
				// Printable characters
				if((ch >= 0x20 && ch < 0x7F) || ch == '\n')
				{
					if(exprIndex >= 4093 && ch != '\n')
					{
						printf("%c", (char) ch);
						printf("\n*****Input buffer full. Forcing a flush*****");
						fflush(stdout);
						
						expr[exprIndex++] = ch;
						ch = '\n'; // Simulate pressing 'enter'
					}
					
					histReset();
					if(ch == '\n' && exprIndex > 1)
						addHist(expr);
						
					didPrint = true;
					printf("%c", ch);
					expr[exprIndex++] = ch;
					
					if(ch == '\n')
					{
						fflush(stdout);
						expr[exprIndex++] = 0;
						break;
					}
				}
				
				// Backspace
				if(ch == 0x7F && exprIndex > 0)
				{
					histReset();
					printf("\b \b");
					
					expr[--exprIndex] = 0;
					didPrint = true;
					
					continue;
				}
			}
			
			int32_t exprLen = strlen(expr) - 1;
			while(expr[exprLen] == '\n')
			{
				expr[exprLen--] = 0;
				
				if(exprLen < 0)
					break;
			}
			
			if(strlen(expr) <= 0)
				continue;
				
			if(strcmp(expr, "quit") == 0 || strcmp(expr, "qq") == 0)
			{
				printf("Goodbye!\n");
				fflush(stdout);
				cleanExit(false);
				
				return 0;
			}
			
			uint32_t tbIndex = 0;
			char tmpBuf[4096];
			memset(tmpBuf, 0, 4096);
			
			for(uint32_t i = 0; i < exprIndex; i++)
			{
				if(expr[i] != ' ')
					tmpBuf[tbIndex++] = expr[i];
			}
			
			memcpy(expr, tmpBuf, 4096);
			exprIndex = tbIndex;
			
			if(debugMode)
				printf("Evaluating expression: %s\n", expr);
				
			// setCurHistExpr(expr);
			double result = evaluate(expr, 0);
			
			if(errorFlag)
			{
				printf("\n");
				continue;
			}
			
			if(result == floor(result))
				printf("Base 10: %lld\nBase 16: %X\n", (int64_t) result, (int64_t) result);
			else
				printf("%.10f\n", result);
		}
	}
	
	
	// Evaluate a single expression and exit
	
	char *ptr = expr;
	for(int i = argStart; i < argc; i++) // Combine separate args into one string
	{
		if(strlen(argv[i]) > bufSpace)
		{
			printf("Expression is too long. What are you feeding me dude?\?!!?\n");
			cleanExit(false);
			return -1;
		}
		
		strcat(ptr, argv[i]);
		ptr += strlen(argv[i]);
		bufSpace -= strlen(argv[i]);
	}
	
	// Remove the spaces
	uint32_t tbIndex = 0;
	char tmpBuf[4096];
	memset(tmpBuf, 0, 4096);
	exprIndex = strlen(expr);
	
	for(uint32_t i = 0; i < exprIndex; i++)
	{
		if(expr[i] != ' ')
			tmpBuf[tbIndex++] = expr[i];
	}
	
	memcpy(expr, tmpBuf, 4096);
	exprIndex = tbIndex;
	
	// And now, evaluate the expression
	if(debugMode)
		printf("Evaluating expression: %s\n", expr);
	fflush(stdout);
	
	double result = evaluate(expr, 0);
	
	if(errorFlag == false)
	{
		if(result == floor(result))
			printf("Base 10: %d\nBase 16: %X\n", (int32_t) result, (int32_t) result);
		else
			printf("%.10f\n", result);
	}
	
	cleanExit(false);
	return 0;
}

bool isNumeric(char c)
{
	const char nums[] = "0123456789xX-.";
	
	bool ret = false;
	for(int i = 0; i < strlen(nums); i++)
	{
		if(c == nums[i])
		{
			ret = true;
			break;
		}
	}
	
	return ret;
}

bool isOper(char c)
{
	const char opers[] = "+-*/^%";
	
	bool ret = false;
	for(int i = 0; i < strlen(opers); i++)
	{
		if(c == opers[i])
		{
			ret = true;
			break;
		}
	}
	
	return ret;
}

bool isAlpha(char c)
{
	const char alpha[] = "abcdefghijklmnopqrstuvwxyz";
	
	bool ret = false;
	for(int i = 0; i < strlen(alpha); i++)
	{
		if(c == alpha[i])
		{
			ret = true;
			break;
		}
	}
	
	return ret;
}

double getConst(const char *varStr)
{
	if(strcmp(varStr, "pi") == 0)
		return 3.1415926535897932384626433;
		
	if(strcmp(varStr, "e") == 0)
		return 2.7182818284590452353602874;
		
	return 0.0;
}

double doFunc(const char *funcStr, double arg)
{
	if(strcmp(funcStr, "sin") == 0)
		return sin(arg);
		
	if(strcmp(funcStr, "cos") == 0)
		return cos(arg);
		
	if(strcmp(funcStr, "sqrt") == 0)
		return sqrt(arg);
		
	printf("Unsupported function: '%s'\n", funcStr);
	fflush(stdout);
	// abort();
	errorFlag = true;
	
	return 0.0;
}

double evaluate(char *expr, uint32_t depth)
{
	if(strlen(expr) < 1)
	{
		printf("evaluate() called with an empty expression or subexpression\n");
		fflush(stdout);
		errorFlag = true;
		return 0.0;
	}
	
	if(depth > 1000)
	{
		printf("Welcome to infinite-recursion hell ^_^\n");
		fflush(stdout);
		cleanExit(true);
	}
	double result = 0.0;
	uint32_t exprLen = strlen(expr);
	const char *exprEnd = (expr + exprLen);
	
	char operation = 0;
	char *ptr = expr;
	
	char operators[50];
	double tokens[50];
	uint32_t numTokens = 0;
	
	for(int i = 0; i < 50; i++)
		tokens[i] = 0.0, operators[i] = 0;
		
	// Extract the numeric tokens (constant values) and operators
	// and put them into tokens[] and operators[]
	uint32_t loopCount = 0;
	while(ptr < exprEnd)
	{
		uint32_t numTokensAtStart = numTokens;
		
		loopCount++;
		
		if(loopCount > 10000) // Infinite loop, abort
		{
			printf("Look out! Runaway loop!\n");
			printf("Aborting program; It seems to be stuck in an infinite loop\n");
			fflush(stdout);
			cleanExit(true);
		}
		
		if(numTokens >= 50) // Too many tokens, abort
		{
			printf("Too many tokens in expression!\n");
			fflush(stdout);
			// abort();
			errorFlag = true;
			return 0.0;
		}
		
		// Check for a variable or function
		if(isAlpha(*ptr))
		{
			char vfStr[256];
			memset(vfStr, 0, 256);
			
			uint32_t idx = 0;
			char *tmp = ptr;
			while(tmp < exprEnd && isAlpha(*tmp)) vfStr[idx++] = *tmp++;
			
			ptr = tmp;
			
			// A function will be followed by a parenthesis
			if(*tmp == '(')
			{
				tmp++;
				char funcArg[1024];
				memset(funcArg, 0, 1024);
				
				idx = 0;
				uint32_t pLvl = 1;
				for(; tmp < exprEnd && pLvl > 0; tmp++)
				{
					if(*tmp == '(') pLvl++;
					if(*tmp == ')') pLvl--;
					
					if(pLvl != 0)
						funcArg[idx++] = *tmp;
				}
				
				if(pLvl != 0)
				{
					printf("Function found without closing parenthesis\n");
					fflush(stdout);
					// abort();
					errorFlag = true;
					return 0.0;
				}
				
				ptr = tmp; // Set ptr to next character after ')'
				
				double argVal = evaluate(funcArg, depth + 1);
				
				if(errorFlag)
					return 0.0;
					
				tokens[numTokens++] = doFunc(vfStr, argVal);
				
				if(errorFlag)
					return 0.0;
					
			}
			else     // It's not a function, so it must be a variable
			{
			
				double constVal = getConst(vfStr);
				
				if(constVal == 0.0)
				{
					printf("Unrecognized variable name: '%s'\n", vfStr);
					
					if(strcmp(vfStr, "q") == 0)
						printf("Perhaps you meant 'qq' or 'quit'?\n");
						
					fflush(stdout);
					// abort();
					errorFlag = true;
					return 0.0;
				}
				
				tokens[numTokens++] = constVal;
			}
			
			if(ptr < exprEnd && !isOper(*ptr))
			{
				printf("Function/variable followed with an unrecognized operator: '%c'\n", *ptr);
				fflush(stdout);
				// abort();
				errorFlag = true;
				return 0.0;
			}
			
			if(ptr >= exprEnd || *ptr == 0)
				continue;
		}
		
		// Check if we're at the beginning of a subexpression
		if(ptr < exprEnd && *ptr == '(')
		{
			char *tmp = ptr + 1;
			int32_t pLvl = 1;
			
			while(pLvl > 0 && tmp < exprEnd)
			{
				if(*tmp == '(') pLvl++;
				if(*tmp == ')') pLvl--;
				
				if(pLvl == 0)
					break;
					
				tmp++;
			}
			
			if(pLvl != 0)
			{
				printf("Expression found without closing parenthesis\n");
				fflush(stdout);
				// abort();
				errorFlag = true;
				return 0.0;
			}
			
			char subExpr[4096];
			memset(subExpr, 0, 4096);
			
			// Don't include the parenthesis
			ptr += 1;
			tmp -= 1;
			
			uint32_t i = 0;
			while(ptr <= tmp)
				subExpr[i++] = *ptr++;
				
			// Jump past the closing parenthesis
			ptr += 1;
			
			// Evaluate the subexpression
			tokens[numTokens++] = evaluate(subExpr, (depth + 1));
			
			if(errorFlag)
				return 0.0;
				
			if(ptr >= exprEnd || *ptr == 0)
				continue;
				
		}
		else if(ptr < exprEnd && isNumeric(*ptr))     // Look for numerical tokens
		{
		
			// Check if we've got a float
			bool isFloat = false;
			char *tmp = ptr;
			while(tmp < exprEnd && isNumeric(*tmp))
			{
				if(*tmp == '.') isFloat = true;
				tmp++;
			}
			
			// If the number was followed by a minus sign, back up one character
			if(*(tmp - 1) == '-') tmp--;
			
			if(isFloat) // Parse float
			{
				double t = strtod(ptr, &ptr);
				tokens[numTokens++] = t;
			}
			else  	// Parse int
			{
				int64_t t = strtoll(ptr, &ptr, 0);
				tokens[numTokens++] = t;
			}
			
		}
		
		if(numTokens - numTokensAtStart == 0)
		{
			printf("Invalid expression; No numerical tokens found while tokenizing the expression.\n");
			fflush(stdout);
			errorFlag = true;
			return 0.0;
		}
		
		// Grab the operator following the token if there is one
		if(ptr < exprEnd)
		{
			if(!isOper(*ptr))
			{
				printf("Numeric constant followed by non-operator character '%c'\n", *ptr);
				fflush(stdout);
				// abort();
				errorFlag = true;
				return 0.0;
			}
			
			if(numTokens < 1)
			{
				printf("Found an operator, but no tokens (numeric values).");
				printf("You do know how math works... Right?\n");
				fflush(stdout);
				
				errorFlag = true;
				return 0.0;
			}
			
			operators[numTokens - 1] = *ptr++;
		}
	}
	
	if(debugMode)
	{
		printf("evaluate(%s):\n", expr);
		printf("Recursion depth: %u\n\n", depth);
		
		printf("\tEquation rebuilt from tokens/opers:\n\t\t");
		for(uint32_t i = 0; i < numTokens; i++)
		{
			if(i > 0)
				printf(" %c ", operators[i - 1]);
				
			printf("%.6f", tokens[i]);
		}
		printf("\n\n");
		fflush(stdout);
	}
	
	// Now process the expression
	uint32_t evalPhase = 0; // 0 = ^ 1 = */% 2 = +-
	
	if(debugMode)
		printf("\tnumTokens = %u\n\n", numTokens);
		
	while(evalPhase < 3)
	{
		// printf("\tPhase %u:\n\n", evalPhase);
		for(uint32_t i = 0; i < numTokens - 1; i++)
		{
			const double t1 = tokens[i];
			const double t2 = tokens[i + 1];
			const char oper = operators[i];
			
			if(evalPhase == 0 && oper != '^')
				continue;
				
			if(evalPhase == 1 && oper != '*' && oper != '/' && oper != '%')
				continue;
				
			if(evalPhase == 2 && oper != '+' && oper != '-')
			{
				printf("Found invalid operator in last phase of evaluation O_o\n");
				fflush(stdout);
				// abort();
				errorFlag = true;
				return 0.0;
			}
			
			
			switch(oper)
			{
				case '^':
				{
					if(debugMode)
						printf("\tCalc: %.6f %c %.6f\n", t1, oper, t2);
						
					double r = pow(t1, t2);
					tokens[i] = r;
					
					for(uint32_t j = i + 1; j < numTokens - 1; j++)
						tokens[j] = tokens[j + 1];
						
					for(uint32_t j = i; j < numTokens - 2; j++)
						operators[j] = operators[j + 1];
						
					numTokens--;
				}
				break;
				
				case '*':
				case '/':
				case '%':
				{
					if(debugMode)
						printf("\tCalc: %.6f %c %.6f\n", t1, oper, t2);
						
					double r = 0.0;
					if(oper == '*')
						r = t1 * t2;
						
					if(oper == '/')
						r = t1 / t2;
						
					if(oper == '%')
						r = fmod(t1, t2);
						
					tokens[i] = r;
					
					for(uint32_t j = i + 1; j < numTokens - 1; j++)
						tokens[j] = tokens[j + 1];
						
					for(uint32_t j = i; j < numTokens - 2; j++)
						operators[j] = operators[j + 1];
						
					numTokens--;
				}
				break;
				
				case '+':
				case '-':
				{
					if(debugMode)
						printf("\t\tCalc: %.6f %c %.6f\n", t1, oper, t2);
						
					double r = 0;
					if(oper == '+')
						r = t1 + t2;
						
					if(oper == '-')
						r = t1 - t2;
						
					tokens[i] = r;
					
					for(uint32_t j = i + 1; j < numTokens - 1; j++)
						tokens[j] = tokens[j + 1];
						
					for(uint32_t j = i; j < numTokens - 2; j++)
						operators[j] = operators[j + 1];
						
					numTokens--;
				}
				break;
				
				default:
				{
					printf("Somehow, a non-operator character got into operators list...\n");
					fflush(stdout);
					// abort();
					errorFlag = true;
					return 0.0;
				}
			}
			
			i--;
		}
		
		evalPhase++;
	}
	
	if(debugMode)
		printf("\n\tFinal result: %.10f\n\n", tokens[0]);
		
	result = tokens[0];
	return result;
}

bool isPrintable(uint8_t byte) { return (byte > 32 && byte < 127); }
void hexDump(const uint8_t *buf, uint32_t bufLen)
{
	const uint32_t hexBytesWidth = 16;
	const uint32_t hexLineWidth = 3 * hexBytesWidth + 1 + 4;
	
	uint32_t numLines = bufLen / hexBytesWidth;
	numLines += (bufLen % hexBytesWidth > 0) ? 1 : 0;
	
	for(uint32_t line = 0; line < numLines; line++)
	{
		const uint8_t *lineBuf = buf + (line * hexBytesWidth);
		uint32_t lineMaxLen = bufLen - (line * hexBytesWidth);
		
		if(lineMaxLen >= hexBytesWidth) // We have hexBytesWidth bytes to print
			lineMaxLen = hexBytesWidth;
			
		uint32_t printCount = 0;
		for(uint32_t i = 0; i < lineMaxLen; i++)
		{
			if(i > 0)
				printCount += printf(" ");
				
			if(i == hexBytesWidth / 2)
				printCount += printf(" ");
				
			printCount += printf("%.2X", lineBuf[i]);
		}
		
		while(printCount < hexLineWidth)
			printCount += printf(" ");
			
		printf("|");
		printCount = 0;
		for(uint32_t i = 0; i < lineMaxLen; i++)
		{
			if(isPrintable(lineBuf[i]))
				printCount += printf("%c", lineBuf[i]);
			else
				printCount += printf(".");
		}
		
		while(printCount < hexBytesWidth)
			printCount += printf(" ");
		printf("|\n");
	}
	
	printf("\n");
}


void addHist(const char *buf)
{
	// We need to drop the oldest entry to make room for a new one
	if(exprHistCount == EXPR_HIST_SIZE)
	{
		// We'll re-use the buffer we're popping off the back
		memset(exprHistory[0], 0, 4096);
		char *reuseBuf = exprHistory[0];
		for(uint32_t i = 0; i < EXPR_HIST_SIZE - 1; i++)
			exprHistory[i] = exprHistory[i + 1];
			
		exprHistory[EXPR_HIST_SIZE - 1] = reuseBuf;
		exprHistCount = EXPR_HIST_SIZE - 1;
	}
	
	// Allocate a buffer if needed
	if(exprHistory[exprHistCount] == 0)
		exprHistory[exprHistCount] = (char *) malloc(4096);
		
	// Copy 'buf' into the history buffer
	memset(exprHistory[exprHistCount], 0, 4096);
	strncpy(exprHistory[exprHistCount], buf, 4095);
	exprHistCount++;
}

void setCurHistExpr(const char *buf)
{
	if(exprHistory[EXPR_HIST_SIZE] == 0)
		exprHistory[EXPR_HIST_SIZE] = (char *) malloc(4096);
		
	memset(exprHistory[EXPR_HIST_SIZE], 0, 4096);
	strncpy(exprHistory[EXPR_HIST_SIZE], buf, 4095);
}

bool histBack(char *buf)
{
	if(exprHistCount == 0)
		return false;
		
	if(exprHistIndex == EXPR_HIST_SIZE)
		exprHistIndex = exprHistCount;
		
	if(exprHistIndex == exprHistCount)
		setCurHistExpr(buf);
		
	if(exprHistIndex > 0)
	{
		memset(buf, 0, 4096);
		strncpy(buf, exprHistory[--exprHistIndex], 4095);
		
		return true;
	}
	
	return false;
}

bool histFwd(char *buf)
{
	if(exprHistCount == 0)
		return false;
		
	if(exprHistIndex < exprHistCount - 1)
	{
		memset(buf, 0, 4096);
		strncpy(buf, exprHistory[++exprHistIndex], 4095);
		
		return true;
	}
	
	if(exprHistIndex == exprHistCount - 1 && exprHistIndex != EXPR_HIST_SIZE)
	{
		memset(buf, 0, 4096);
		strncpy(buf, exprHistory[EXPR_HIST_SIZE], 4095);
		exprHistIndex = EXPR_HIST_SIZE;
		
		return true;
	}
	
	return false;
}

void histReset() // Moves the exprHistIndex to the end of the list & clears the current 'expr' buffer
{
	exprHistIndex = EXPR_HIST_SIZE;
	
	if(exprHistory[EXPR_HIST_SIZE] != 0)
		memset(exprHistory[EXPR_HIST_SIZE], 0, 4096);
}


enum exprPart
{
	SUBEXPR = 0,
	NUMBER = 1,
	OPERATOR = 2
};

void genExpressionPart(enum exprPart part, char *out, uint8_t *randPool, uint32_t *randUsed, uint32_t *thisNestLvl);
void getRandBlock(uint8_t *out, uint32_t size);

// This will attempt to generate sensible math expressions very close to
// the length given in maxLen and not exceeding the max.
// The outBuf variable will be filled in such that each expression
// can be found at position x*maxLen
void generateExpressions(uint32_t count, uint32_t maxLen, char *outBuf)
{
	uint8_t randPool[1024 * 64];
	getRandBlock(randPool, (1024 * 64));
	
	uint32_t rGenUsed = 0;
	uint8_t *randPtr = randPool;
	
	char tempStr[2048] = {0};
	memset(tempStr, 0, 2048);
	
	for(uint32_t i = 0; i < count; i++)
	{
		// Make sure we have enough data in our randPool
		if(randPtr - randPool > (65536 - 2048))
		{
			getRandBlock(randPool, (1024 * 64));
			rGenUsed = 0;
			randPtr = randPool;
		}
		
		// The first thing in our expression must be either
		// a number or a subexpression, so we divide by 2 here, not 3
		uint8_t randChoice = *randPtr % 2;
		randPtr++;
		
		// Expressions will probably be too large in this case, so force Numeric generation
		if(maxLen - strlen(tempStr) < 8)
			randChoice = 1;
			
		// Generate the first element in our expression
		char exprStr[256];
		bool retryFail = true;
		for(uint32_t retry = 0; retry < 10; retry++)
		{
			memset(exprStr, 0, 256);
			genExpressionPart(randChoice, exprStr, randPtr, &rGenUsed, 0);
			
			randPtr += rGenUsed;
			rGenUsed = 0;
			
			// We're good as long as we have enough space for a null-terminator
			if((strlen(exprStr) + strlen(tempStr)) < maxLen - 1)
			{
				retryFail = false;
				break;
			}
		}
		
		if(!retryFail)
			strcat(tempStr, exprStr);
			
		assert(!retryFail && strcmp("Failed to fill the requested 'maxLen' buffer. Try a larger buffer.\n", "") != 0);
		
		for(uint32_t genCount = 0; genCount < 20; genCount++)
		{
			// We can't be exact on matching the max length.
			// In fact, if we tried, we'd probably
			// get stuck in this loop for a long time...
			if(maxLen - strlen(tempStr) < 3)
				break;
				
			// Make sure we have enough data in our randPool
			if(randPtr - randPool > (65536 - 2048))
			{
				getRandBlock(randPool, (1024 * 64));
				rGenUsed = 0;
				randPtr = randPool;
			}
			
			// Operators must go between EVERYTHING...
			// So generate one before our next expression/number
			char operStr[5] = {0};
			genExpressionPart(OPERATOR, operStr, randPtr, &rGenUsed, 0);
			randPtr += rGenUsed;
			rGenUsed = 0;
			
			// Ok, try a few times to generate data that fits our buffer
			retryFail = true;
			for(uint32_t retry = 0; retry < 10; retry++)
			{
				memset(exprStr, 0, 256);
				genExpressionPart(randChoice, exprStr, randPtr, &rGenUsed, 0);
				randPtr += rGenUsed;
				rGenUsed = 0;
				
				// We're good as long as we have enough space for a null-terminator
				if((strlen(exprStr) + strlen(tempStr)) < maxLen - 1)
				{
					retryFail = false;
					break;
				}
			}
			
			// If we failed, we took up too much buffer
			if(!retryFail)
			{
				strcat(tempStr, operStr);
				strcat(tempStr, exprStr);
			}
		}
		
		// assert(!retryFail && strcmp("Failed to fill the requested 'maxLen' buffer. Try a larger buffer.\n", "") != 0);
		
		if(strlen(tempStr) >= maxLen)
		{
			printf("tempStr = (%u) %s\n", strlen(tempStr), tempStr);
			fflush(stdout);
		}
		assert(strlen(tempStr) < maxLen);
		
		// printf("Generated expression: %s\n", tempStr);
		strncpy(outBuf + (maxLen * i), tempStr, strlen(tempStr));
		memset(tempStr, 0, 2048);
		fflush(stdout);
	}
}

void genExpressionPart(enum exprPart part, char *out, uint8_t *randPool, uint32_t *randUsed, uint32_t *thisNestLvl)
{
	uint8_t *lRand = randPool;
	
	char localOut[1024] = {0};
	memset(localOut, 0, 256);
	
	switch(part)
	{
		case NUMBER:
		{
			int32_t randVal = 0;
			while(randVal == 0)
			{
				memcpy(&randVal, lRand, 4);
				lRand += 4;
				
				randVal &= 0x0000FFFF;
			}
			
			uint8_t rDecide = *lRand++;
			
			// Choose a whole number
			if(rDecide % 3 == 0)
			{
				int32_t rDiv = 0;
				
				while(rDiv == 0)
				{
					memcpy(&rDiv, lRand, 4);
					lRand += 4;
					
					rDiv &= 0x0000FFFF;
				}
				
				int32_t modRes = 0;
				if(randVal > rDiv)
					modRes = randVal % rDiv;
				else
					modRes = rDiv % randVal;
					
				if(*lRand++ % 3 == 0)
					modRes *= -1;
					
				sprintf(localOut, "%d", modRes);
				strcat(out, localOut);
				*randUsed = (lRand - randPool);
				// printf("GEP Number: %s\n", localOut);
				return;
			}
			
			// Choose a floating point number
			double rVal = randVal;
			rVal /= (1 << 16);
			
			sprintf(localOut, "%.3f", rVal);
			strcat(out, localOut);
			*randUsed = (lRand - randPool);
			// printf("GEP Number: %s\n", localOut);
			return;
		}
		break;
		
		case OPERATOR:
		{
			char opers[] = "+-*/%^";
			uint32_t rVal = 0;
			memcpy(&rVal, lRand, 4);
			lRand += 4;
			
			rVal %= strlen(opers);
			localOut[0] = opers[rVal];
			
			
			strcat(out, localOut);
			*randUsed = (lRand - randPool);
			// printf("GEP Operator: %s\n", localOut);
			return;
		}
		break;
		
		case SUBEXPR:
		{
			uint32_t maxNestingLvl = 1;
			uint32_t curNestingLvl = 0;
			
			if(thisNestLvl != 0)
				curNestingLvl = *thisNestLvl;
				
			uint32_t numItemsToGenerate = (*lRand++) % 2 + 2; // 2-5 items
			strcat(localOut, "(");
			
			for(uint32_t i = 0; i < numItemsToGenerate; i++)
			{
				if(i > 0) // Drop a random operator into our subexpression
				{
					uint32_t randUsed = 0;
					genExpressionPart(OPERATOR, localOut, lRand, &randUsed, 0);
					lRand += randUsed;
				}
				
				// Generate a number as the first item in any subexpression
				// or whenever we hit our subexpression cap
				if(i == 0 || curNestingLvl >= maxNestingLvl)
				{
					uint32_t lrandUsed = 0;
					genExpressionPart(NUMBER, localOut, lRand, &lrandUsed, 0);
					lRand += lrandUsed;
					
					continue;
				} // ends with 'continue'
				
				uint8_t choice = *lRand++;
				choice %= 2;
				
				if(choice == 0) // generate a subexpression
				{
					curNestingLvl++;
					
					uint32_t lrandUsed = 0;
					genExpressionPart(SUBEXPR, localOut, lRand, &lrandUsed, &curNestingLvl);
					lRand += lrandUsed;
					
				}
				else if(choice == 1)     // Generate a number
				{
				
					uint32_t lrandUsed = 0;
					genExpressionPart(NUMBER, localOut, lRand, &lrandUsed, 0);
					lRand += lrandUsed;
				}
			}
			
			if(thisNestLvl != 0)
				*thisNestLvl = curNestingLvl;
				
			strcat(localOut, ")");
			strcat(out, localOut);
			*randUsed = (lRand - randPool);
			// printf("GEP Expression: %s\n", localOut);
			return;
		}
		break;
	}
}

void getRandBlock(uint8_t *out, uint32_t size)
{
	FILE *fp = fopen("/dev/urandom", "rb");
	int32_t readRes = fread(out, 1, size, fp);
	fclose(fp);
	
	assert(readRes == size);
	assert(true);
}
