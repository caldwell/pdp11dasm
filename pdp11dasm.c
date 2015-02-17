
/*
 * PDP11Dasm PDP-11 Disassembler - Copyright (C) 2004 by
 * Jeffery L. Post
 * theposts@pacbell.net
 *
 * Version 0.0.3 - 02/01/04
 *
 * If you find bugs, please notify the author at the above email address.
 * [TODO] The decoding of floating point opcodes is not correct.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<string.h>
#include	<sys/types.h>
#include	<sys/stat.h>

#ifndef FALSE
#define	FALSE	0
#endif
#ifndef TRUE
#define	TRUE	1
#endif

#define	VERSION			0
#define	MAJOR_REV		0
#define	MINOR_REV		3

#define	byte				unsigned char
#define	word				unsigned short
#define	bool				int

#define	MODEREG_MASK	0x3f	// mode and register mask
#define	OFFSET_MASK		0xff	// pc relative offset from instruction

#define	FN_LEN			128	// max file name length + 1
#define	LINE_LEN			128	// max length of output line + 1

// Control file codes

#define	CTL_NONE			0x00	// executable code
#define	CTL_DATA			0x01	// binary word
#define	CTL_ASCII		0x02	// ascii data

// Prototypes

void	usage(void);
int	decode(int adrs);
int	group0(int adrs);
int	group1(int adrs);
int	group2(int adrs);
int	group3(int adrs);
int	group4(int adrs);
int	group5(int adrs);
int	group6(int adrs);
int	group7(int adrs);
int	group8(int adrs);
int	group9(int adrs);
int	groupa(int adrs);
int	groupb(int adrs);
int	groupc(int adrs);
int	groupd(int adrs);
int	groupe(int adrs);
int	groupf(int adrs);
int	doOperand(int adrs, int modereg);
char	*get_adrs(char *text, word *val);

char	pdpFileName[FN_LEN];		// input file
char	ctlFileName[FN_LEN];		// control file name
char	disFileName[FN_LEN];		// disassembly file name

char	outLine[128];				// disassembled output line
char	temp[128];
FILE	*fp = NULL;					// file to be disassembled
FILE	*ctlfp = NULL;				// control file
FILE	*disFile = NULL;			// disassembled file

struct stat	fstatus;
int	pc;							// current program counter
int	breakLine;					// if true, add a blank comment line to output
word	*program;					// the program data
byte	*flags;						// disassembly flags from the control file

//////////////////////////////////
//
//			The Main Program
//
//////////////////////////////////

int main(int argc, char *argv[])
{
	int	i, max;
	word	start, stop;
	char	*text, func, chr;

	if (argc < 2)
	{
		usage();
		return 1;
	}

	pdpFileName[0] = '\0';

	for (i=1; i<argc; i++)
	{
		chr = *argv[i];

		if (chr == '-')			// option
		{
			text = argv[i];
			text++;

			if (*text == '-')
				text++;

			chr = toupper(*text);

			if (chr == 'V')		// version
			{
				printf("\npdp11dasm version %d.%d.%d\n\n", VERSION, MAJOR_REV, MINOR_REV);
				return 0;
			}
			else if (chr == 'H')	// help
			{
				usage();
				return 0;
			}
		}
		else							// must be the file name
		{
			strcpy(pdpFileName, argv[i]);
		}
	}

	if (!pdpFileName[0])
	{
		usage();
		return 0;
	}

	stat(pdpFileName, &fstatus);
	max = (int) fstatus.st_size;
	program = (word *) malloc(max + 10);	// larger than actual program to prevent segfaults on lookahead

	if (!program)
	{
		printf("Can't allocate memory for program (%d bytes)\n", max);
		return -1;
	}

	strcpy(disFileName, pdpFileName);
	strcat(disFileName, ".das");
	strcpy(ctlFileName, pdpFileName);
	strcat(ctlFileName, ".ctl");
	ctlfp = fopen(ctlFileName, "r");

	flags = (byte *) malloc(max + 10);

	if (!flags)
	{
		printf("Can't allocate memory for program flags (%d bytes)\n", max);
		return -1;
	}

	for (i=0; i < max / 2; i++)
		flags[i] = CTL_NONE;

	if (ctlfp)		// read control file and set program flags
	{
		while (!feof(ctlfp))
		{
			start = stop = 0;
			temp[0] = '\0';

			if (fgets(temp, 127, ctlfp))
			{
				text = (char *) &temp[1];

				while (isgraph(*text))				// skip remaining chars in 1st word
					text++;

				text = get_adrs(text, &start);	// get octal address of start

				while (1)
				{
					chr = *text++;

					if (chr != ' ' && chr != '\t')	// skip whitespace
						break;
				}

				if (chr == '\n' || chr == ';')	// if only one numeric...
					--text;								// back up to newline

				func = chr;								// save operator
				text = get_adrs(text, &stop);		// get octal address of end

				if (func == '+')						// check for valid operator
					stop += (start - 1);
				else if (func == '-' && !stop)
					stop = start;

				if (start < max && stop < max)
				{
					switch (toupper(temp[0]))
					{
						case 'A':				// ascii text
							do
							{
								flags[start++] = CTL_ASCII;
							} while (start <= stop);
							break;

						case 'D':				// data
							do
							{
								flags[start++] = CTL_DATA;
							} while (start <= stop);
							break;

						case ';':				// comment
							break;

						default:					// ignore anything else
//							fprintf(stderr, "Invalid control code: %s\n", temp);
							break;
					}
				}
				else
					printf("Invalid address in %s\n", temp);
			}
			else
				break;
		}

		fclose(ctlfp);
	}

	fp = fopen(pdpFileName, "rb");		// open file to be disassembled

	if (!fp)
	{
		printf("Can't open file '%s'\n", pdpFileName);
		free(program);
		free(flags);

		return -1;
	}

	fread(program, sizeof(byte), max, fp);
	fclose(fp);

	disFile = fopen(disFileName, "w");	// open output file

	if (!disFile)
	{
		printf("Can't open output file\n");
		free(program);
		free(flags);

		return -1;
	}

	max /= 2;

	fprintf(disFile, ";\n; pdp11dasm version %d.%d.%d\n; disassembly of %s\n;",
		VERSION, MAJOR_REV, MINOR_REV, pdpFileName);

	for (pc=0; pc<max; )			// do the disassembly
	{
		pc = decode(pc);
	}

	fprintf(disFile, "\n");
	fclose(disFile);
	free(program);
	free(flags);
	return 0;
}								//  End of Main

void usage(void)
{
	printf("\nusage: pdp11dasm [options] file\n\n"
			"'file' is the PDP-11 binary file to disassemble.\n"
			"Output will be written to 'file.das'.\n"
			"Options:\n"
			"  -v --version   Show version and exit.\n"
			"  -h --help      Display this message and exit.\n\n");
}

// Output Ascii codes in comment field for 16 bit word

void printAscii(word data)
{
	char	chr;

	chr = data & 0x7f;

	if (chr < ' ' || chr > '~')
		chr = '.';

	fprintf(disFile, "%c", chr);

	chr = (data >> 8) & 0x7f;

	if (chr < ' ' || chr > '~')
		chr = '.';

	fprintf(disFile, "%c", chr);
}

// Output a string of up to three words (6 bytes) (DEFB)

int doString(int adrs)
{
	int	i, count = 1;
	bool	prt, even;
	char	chr;
	char	tmp[16];

	if (flags[(adrs + 1) * 2] == CTL_ASCII)
	{
		count++;

		if (flags[(adrs + 2) * 2] == CTL_ASCII)
			count++;
	}

	sprintf(outLine, "\tdefb\t");
	prt = FALSE;

	for (i=0; i<count; i++)
	{
		for (even=0; even<2; even++)
		{
			if (!even)
				chr = program[adrs] & 0x7f;
			else
				chr = (program[adrs] >> 8) & 0x7f;

			if (isprint(chr))					// is printable
			{
				if (!i && !even)				// if first character
					strcat(outLine, "'");	// start with '
				else if (!prt)
					strcat(outLine, ",'");

				prt = TRUE;
				sprintf(tmp, "%c", chr);
			}
			else								// not printable, do octal
			{
				if (prt)						// end previous ascii string, if any
					strcat(outLine, "',");
				else if (i || even)
					strcat(outLine, ",");

				prt = FALSE;
				sprintf(tmp, "%o", chr);
			}

			strcat(outLine, tmp);
		}

		adrs++;
	}

	if (prt)
		strcat(outLine, "'");

	if (flags[adrs * 2] == CTL_NONE)
		breakLine = TRUE;

	return adrs;
}

// Output up to 3 words of data (DEFW)

int doData(int adrs)
{
	int	i, count = 1;
	char	tmp[16];

	if (flags[(adrs + 1) * 2] == CTL_DATA)
	{
		count++;

		if (flags[(adrs + 2) * 2] == CTL_DATA)
			count++;
	}

	sprintf(outLine, "\tdefw\t");

	for (i=0; i<count; i++)
	{
		if (i)
			strcat(outLine, ",");

		sprintf(tmp, "%o", program[adrs]);
		strcat(outLine, tmp);
		adrs++;
	}

	if (flags[adrs * 2] == CTL_NONE)
		breakLine = TRUE;

	return adrs;
}

// Decode the current opcode

int decode(int adrs)
{
	word	code, opcode;
	int	i, pos, start = adrs;

	breakLine = FALSE;
	code = program[adrs];
	opcode = code & 0xf000;
	opcode >>= 12;
	fprintf(disFile, "\n%06o: %06o", adrs * 2, code);
	outLine[0] = '\0';

	switch (flags[adrs * 2])
	{
		case CTL_NONE:
			switch (opcode)
			{
				case 0x00:
					adrs = group0(adrs);
					break;

				case 0x01:
					adrs = group1(adrs);
					break;

				case 0x02:
					adrs = group2(adrs);
					break;

				case 0x03:
					adrs = group3(adrs);
					break;

				case 0x04:
					adrs = group4(adrs);
					break;

				case 0x05:
					adrs = group5(adrs);
					break;

				case 0x06:
					adrs = group6(adrs);
					break;

				case 0x07:
					adrs = group7(adrs);
					break;

				case 0x08:
					adrs = group8(adrs);
					break;

				case 0x09:
					adrs = group9(adrs);
					break;

				case 0x0a:
					adrs = groupa(adrs);
					break;

				case 0x0b:
					adrs = groupb(adrs);
					break;

				case 0x0c:
					adrs = groupc(adrs);
					break;

				case 0x0d:
					adrs = groupd(adrs);
					break;

				case 0x0e:
					adrs = groupe(adrs);
					break;

				case 0x0f:
					adrs = groupf(adrs);
					break;
			}
			break;

		case CTL_DATA:
			adrs = doData(adrs);
			break;

		case CTL_ASCII:
			adrs = doString(adrs);
			break;

		default:
			printf("\nUnknown code 0x%02x at address %06o\n", flags[adrs], adrs);
			adrs++;
			break;
	}

	switch (adrs - start)
	{
		case 1:
			fprintf(disFile, "              ");
			break;

		case 2:
			fprintf(disFile, " %06o       ", program[start + 1]);
			break;

		case 3:
			fprintf(disFile, " %06o %06o", program[start + 1], program[start + 2]);
			break;
	}

	fprintf(disFile, "%s", outLine);		// pos = # characters printed starting at col 32

	i = 1;
	pos = 32;

	while (outLine[i])
	{
		if (outLine[i] != '\t')
			pos++;
		else do
			pos++;
		while (pos & 7);

		i++;
	}

	while (pos < 64)
	{
		fprintf(disFile, "\t");
		pos += 8;
		pos &= 0xf8;
	}

	fprintf(disFile, "; ");

	for (i=start; i<adrs; i++)
		printAscii(program[i]);

	if (breakLine)
		fprintf(disFile, "\n;");

	return adrs;
}

// Invalid opcode

void invalid(void)
{
	sprintf(outLine, "\tinvalid opcode");
}

// jsr opcodes

int jsr(int adrs)
{
	int	reg;

	reg = (program[adrs] >> 6) & 7;
	sprintf(outLine, "\tjsr\tr%d,", reg);
	adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
	return adrs;
}

void doOffset(int adrs, int offset)
{
	int	dst;

	adrs++;

	if (offset > 0x7f)		// negative offset
		dst = 2 * adrs - 2 * (0x100 - offset);
	else
		dst = 2 * adrs + 2 * offset;

	sprintf(temp, "%o", dst);
	strcat(outLine, temp);
}

// miscellaneous group 0 opcodes
// halt, wait, rti, bpt, iot, reset, rtt, mfpt, jmp, rts, spl, nop, swab, br

int misc0(int adrs)
{
	int	code;

	code = (program[adrs] >> 6) & 7;

	switch (code)
	{
		case 0:		// halt, wait, rti, bpt, iot, reset, rtt, mfpt
			switch (program[adrs])
			{
				case 0:
					sprintf(outLine, "\thalt");

					if (program[adrs + 1])		// if not followed by another halt,
						breakLine = TRUE;			// do a blank line
					break;

				case 1:
					sprintf(outLine, "\twait");
					break;

				case 2:
					sprintf(outLine, "\trti");
					breakLine = TRUE;
					break;

				case 3:
					sprintf(outLine, "\tbpt");
					break;

				case 4:
					sprintf(outLine, "\tiot");
					break;

				case 5:
					sprintf(outLine, "\treset");
					break;

				case 6:
					sprintf(outLine, "\trtt");
					breakLine = TRUE;
					break;

				case 7:
					sprintf(outLine, "\tmfpt");
					break;

				default:
					invalid();
					break;
			}
			break;

		case 1:		// jmp
			sprintf(outLine, "\tjmp\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			breakLine = TRUE;
			break;

		case 2:		// rts, spl, nop, cond codes & unimplemented
			if (program[adrs] < 000210)
			{
				sprintf(outLine, "\trts\tr%d", program[adrs] & 7);
				breakLine = TRUE;
			}
			else if (program[adrs] < 000230)
				invalid();
			else if (program[adrs] < 000240)		// spl
				sprintf(outLine, "\tspl\t%d", program[adrs] & 7);
			else
			{
				if (program[adrs] == 0240)
					sprintf(outLine, "\tnop");
				else switch (program[adrs])		// condition codes
				{
					case 0241:
						sprintf(outLine, "\tclc");
						break;

					case 0242:
						sprintf(outLine, "\tclv");
						break;

					case 0244:
						sprintf(outLine, "\tclz");
						break;

					case 0250:
						sprintf(outLine, "\tcln");
						break;

					case 0257:
						sprintf(outLine, "\tccc");
						break;

					case 0261:
						sprintf(outLine, "\tsec");
						break;

					case 0262:
						sprintf(outLine, "\tsev");
						break;

					case 0264:
						sprintf(outLine, "\tsez");
						break;

					case 0270:
						sprintf(outLine, "\tsen");
						break;

					case 0277:
						sprintf(outLine, "\tscc");
						break;

					default:
						invalid();
						break;
				}
			}
			break;

		case 3:		// swab
			sprintf(outLine, "\tswab\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			break;

		case 4:		// br
		case 5:
		case 6:
		case 7:
			sprintf(outLine, "\tbr\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			breakLine = TRUE;
			break;
	}

	return adrs;
}

// miscellaneous opcodes

int group0(int adrs)
{
	int	code;
	int	skipOperand = FALSE;

	code = program[adrs] >> 9;
	code &= 7;

	switch (code)
	{
		case 0:		// halt, wait, rti, bpt, iot, reset, rtt, jmp, rts, spl, nop, swab, br
			adrs = misc0(adrs);
			break;

		case 1:		// bne, beq
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tbeq\t");
			else
				sprintf(outLine, "\tbne\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 2:		// bge, blt
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tblt\t");
			else
				sprintf(outLine, "\tbge\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 3:		// bgt, ble
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tble\t");
			else
				sprintf(outLine, "\tbgt\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 4:		// jsr
			adrs = jsr(adrs);
			break;

		case 5:		// clr, com, inc, dec, neg, adc, sbc, tst
			code = (program[adrs] >> 6) & 7;

			switch (code)
			{
				case 0:
					sprintf(outLine, "\tclr\t");
					break;

				case 1:
					sprintf(outLine, "\tcom\t");
					break;

				case 2:
					sprintf(outLine, "\tinc\t");
					break;

				case 3:
					sprintf(outLine, "\tdec\t");
					break;

				case 4:
					sprintf(outLine, "\tneg\t");
					break;

				case 5:
					sprintf(outLine, "\tadc\t");
					break;

				case 6:
					sprintf(outLine, "\tsbc\t");
					break;

				case 7:
					sprintf(outLine, "\ttst\t");
					break;
			}

			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			break;

		case 6:		// ror, rol, asr, asl, mark, mfpi, mtpi, sxt
			code = (program[adrs] >> 6) & 7;

			switch (code)
			{
				case 0:
					sprintf(outLine, "\tror\t");
					break;

				case 1:
					sprintf(outLine, "\trol\t");
					break;

				case 2:
					sprintf(outLine, "\tasr\t");
					break;

				case 3:
					sprintf(outLine, "\tasl\t");
					break;

				case 4:
					sprintf(outLine, "\tmark\t%o", program[adrs] & 0x3f);
					skipOperand = TRUE;
					break;

				case 5:
					sprintf(outLine, "\tmfpi\t");
					break;

				case 6:
					sprintf(outLine, "\tmtpi\t");
					break;

				case 7:
					sprintf(outLine, "\tsxt\t");
					break;
			}

			if (!skipOperand)
				adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			break;

		case 7:		// invalid opcodes
			invalid();
			break;
	}

	return adrs + 1;
}

// mov opcodes

int group1(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tmov\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// cmp opcodes

int group2(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tcmp\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// bit opcodes

int group3(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tbit\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// bic opcodes

int group4(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tbic\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// bis opcodes

int group5(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tbis\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// add opcodes

int group6(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tadd\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// register opcodes

int group7(int adrs)
{
	int	code, reg, offset;

	code = (program[adrs] >> 9) & 7;
	reg = (program[adrs] >> 6) & 7;

	switch (code)
	{
		case 0:		// mul
			sprintf(outLine, "\tmul\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",r%d", reg);
			strcat(outLine, temp);
			break;

		case 1:		// div
			sprintf(outLine, "\tdiv\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",r%d", reg);
			strcat(outLine, temp);
			break;

		case 2:		// ash
			sprintf(outLine, "\tash\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",r%d", reg);
			strcat(outLine, temp);
			break;

		case 3:		// ashc
			sprintf(outLine, "\tashc\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",r%d", reg);
			strcat(outLine, temp);
			break;

		case 4:		// xor
			sprintf(outLine, "\txor\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",r%d", reg);
			strcat(outLine, temp);
			break;

		case 5:		// fadd, fskub, fmul, fdiv, and unimplemented
			if (program[adrs] >= 075040)
				invalid();
			else
			{
				code = (program[adrs] >> 3) & 3;

				switch (code)
				{
					case 0:
						sprintf(outLine, "\tfadd\tr%d", program[adrs] & 7);
						break;

					case 1:
						sprintf(outLine, "\tfsub\tr%d", program[adrs] & 7);
						break;

					case 2:
						sprintf(outLine, "\tfmul\tr%d", program[adrs] & 7);
						break;

					case 3:
						sprintf(outLine, "\tfdiv\tr%d", program[adrs] & 7);
						break;
				}
			}
			break;

		case 6:		// unimplemented
			invalid();
			break;

		case 7:		// sob
			offset = 2 * (adrs + 1) - 2 * (program[adrs] & 0x3f);
			sprintf(outLine, "\tsob\tr%d,%o", reg, offset);
			break;
	}

	return adrs + 1;
}

// branch opcodes + emt, trap, clrb, comb, incb, decb, negb, adcb, sbcb, tstb, rorb, rolb, asrb, aslb, mfpd, mtpd

int group8(int adrs)
{
	int	code;
	int	skipOperand = FALSE;

	code = program[adrs] >> 9;
	code &= 7;

	switch (code)
	{
		case 0:		// bpl, bmi
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tbmi\t");
			else
				sprintf(outLine, "\tbpl\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 1:		// bhi, blos
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tblos\t");
			else
				sprintf(outLine, "\tbhi\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 2:		// bvc, bvs
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tbvs\t");
			else
				sprintf(outLine, "\tbvc\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 3:		// bcc, bhis, bcs, blo
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tbcs\t");
			else
				sprintf(outLine, "\tbcc\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 4:		// emt, trap
			if (program[adrs] < 0104400)
				sprintf(outLine, "\temt");
			else
				sprintf(outLine, "\ttrap");

			sprintf(temp, "\t%o", program[adrs] & 0xff);
			strcat(outLine, temp);
			break;

		case 5:		// clrb, comb, incb, decb, negb, adcb, sbcb, tstb
			code = (program[adrs] >> 6) & 7;

			switch (code)
			{
				case 0:
					sprintf(outLine, "\tclrb\t");
					break;

				case 1:
					sprintf(outLine, "\tcomb\t");
					break;

				case 2:
					sprintf(outLine, "\tincb\t");
					break;

				case 3:
					sprintf(outLine, "\tdecb\t");
					break;

				case 4:
					sprintf(outLine, "\tnegb\t");
					break;

				case 5:
					sprintf(outLine, "\tadcb\t");
					break;

				case 6:
					sprintf(outLine, "\tsbcb\t");
					break;

				case 7:
					sprintf(outLine, "\ttstb\t");
					break;
			}

			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			break;

		case 6:		// rorb, rolb, asrb, aslb, mfpd, mtpd
			code = (program[adrs] >> 6) & 7;

			switch (code)
			{
				case 0:
					sprintf(outLine, "\trorb\t");
					break;

				case 1:
					sprintf(outLine, "\trolb\t");
					break;

				case 2:
					sprintf(outLine, "\tasrb\t");
					break;

				case 3:
					sprintf(outLine, "\taslb\t");
					break;

				case 4:
					invalid();
					skipOperand = TRUE;
					break;

				case 5:
					sprintf(outLine, "\tmfpd\t");
					break;

				case 6:
					sprintf(outLine, "\tmtpd\t");
					break;

				case 7:
					invalid();
					skipOperand = TRUE;
					break;
			}

			if (!skipOperand)
				adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			break;

		case 7:		// invalid opcodes
			invalid();
			break;
	}

	return adrs + 1;
}

// movb opcodes

int group9(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tmovb\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// cmpb opcodes

int groupa(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tcmpb\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// bitb opcodes

int groupb(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tbitb\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// bicb opcodes

int groupc(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tbicb\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// bisb opcodes

int groupd(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tbisb\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// sub opcodes

int groupe(int adrs)
{
	int	modereg;

	sprintf(outLine, "\tsub\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// Output floating point instruction operands
//
// [TODO] Get accurate documentation on floating point instructions
// and fix this function.

int doFPOperand(int adrs)
{
	int	mode, reg, acc;

	acc = (program[adrs] >> 6) & 3;
	mode = (program[adrs] >> 3) & 7;
	reg = program[adrs] & 7;

	if (!mode)
	{
		sprintf(temp, "\tf%d,", acc);		// mode != 0, restricted to AC0-AC3
		strcat(outLine, temp);
		adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
	}
	else		// actually, this makes no sense at all; there are only 2 bits for AC,
	{			// so it's not possible to access AC4-AC5
		sprintf(temp, "\tf%d,", acc);
		strcat(outLine, temp);
		adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
	}

	return adrs;
}

// floating point opcodes

int groupf(int adrs)
{
	int	code;

	code = (program[adrs] >> 8) & 0x0f;

	switch (code)
	{
		case 0x00:	// setf, setd, seti, setl, ldfps, stfps, stst, cfcc
			switch (program[adrs])
			{
				case 0170000:
					sprintf(outLine, "\tcfcc");
					break;

				case 0170001:
					sprintf(outLine, "\tsetf");
					break;

				case 0170002:
					sprintf(outLine, "\tseti");
					break;

				case 0170011:
					sprintf(outLine, "\tsetd");
					break;

				case 0170012:
					sprintf(outLine, "\tsetl");
					break;

				default:
					code = (program[adrs] >> 6) & 3;

					switch (code)
					{
						case 0:
							invalid();
							break;

						case 1:
							sprintf(outLine, "\tldfps\t");
							adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
							break;

						case 2:
							sprintf(outLine, "\tstfps\t");
							adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
							break;

						case 3:
							sprintf(outLine, "\tstst\t");
							adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
							break;
					}
					break;
			}
			break;

		case 0x01:	// negf, negdm, clrf, clrd, absf, absd, tstf, tstd
			code = (program[adrs] >> 6) & 3;

			switch (code)
			{
				case 0:
					sprintf(outLine, "\tclrf\t");
					adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
					break;

				case 1:
					sprintf(outLine, "\ttstf\t");
					adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
					break;

				case 2:
					sprintf(outLine, "\tabsf\t");
					adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
					break;

				case 3:
					sprintf(outLine, "\tnegf\t");
					adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
					break;
			}
			break;

		case 0x02:	// mulf, muld
			sprintf(outLine, "\tmult");
			adrs = doFPOperand(adrs);
			break;

		case 0x03:	// modf, modd
			sprintf(outLine, "\tmodf");
			adrs = doFPOperand(adrs);
			break;

		case 0x04:	// addf, addd
			sprintf(outLine, "\taddf");
			adrs = doFPOperand(adrs);
			break;

		case 0x05:	// ldf, ldd
			sprintf(outLine, "\tldf");
			adrs = doFPOperand(adrs);
			break;

		case 0x06:	// subf, subd
			sprintf(outLine, "\tsubf");
			adrs = doFPOperand(adrs);
			break;

		case 0x07:	// cmpf, cmpd
			sprintf(outLine, "\tcmpf");
			adrs = doFPOperand(adrs);
			break;

		case 0x08:	// stf, std
			sprintf(outLine, "\tstf");
			adrs = doFPOperand(adrs);
			break;

		case 0x09:	// divf, divd
			sprintf(outLine, "\tdivf");
			adrs = doFPOperand(adrs);
			break;

		case 0x0a:	// stexp
			sprintf(outLine, "\tstexp");
			adrs = doFPOperand(adrs);
			break;

		case 0x0b:	// stcfi, stcfl, stcdi, stcdl
			break;

		case 0x0c:	// stcfd, stcdf
			sprintf(outLine, "\tstcdf");
			adrs = doFPOperand(adrs);
			break;

		case 0x0d:	// ldexp
			sprintf(outLine, "\tldexp");
			adrs = doFPOperand(adrs);
			break;

		case 0x0e:	// ldcif, ldcid, ldclf, ldcld
			sprintf(outLine, "\tldcif");
			adrs = doFPOperand(adrs);
			break;

		case 0x0f:	// ldcdf, ldcfd
			sprintf(outLine, "\tldcdf");
			adrs = doFPOperand(adrs);
			break;
	}

	return adrs + 1;
}

// Output operand

int doOperand(int adrs, int modereg)
{
	int	mode, reg, dest;

	mode = modereg >> 3;
	reg = modereg & 7;

	switch (mode)
	{
		case 0:			// direct
			sprintf(temp, "r%d", reg);
			break;

		case 1:			// register deferred
			sprintf(temp, "(r%d)", reg);
			break;

		case 2:			// auto-increment or immediate (r7)
			if (reg == 7)
			{
				adrs++;
				sprintf(temp, "#%o", program[adrs]);
			}
			else
				sprintf(temp, "(r%d)+", reg);
			break;

		case 3:			// auto-increment deferred or absolute (r7)
			if (reg == 7)
			{
				adrs++;
				sprintf(temp, "@#%o", program[adrs]);
			}
			else
				sprintf(temp, "@(r%d)+", reg);
			break;

		case 4:			// auto-decrement
			sprintf(temp, "-(r%d)", reg);
			break;

		case 5:			// auto-decrement deferred
			sprintf(temp, "@-(r%d)", reg);
			break;

		case 6:			// index or relative (r7)
			adrs++;

			if (reg == 7)
			{
				dest = program[adrs] + 2 * adrs + 2;

				if (dest > 0177776)
					dest -= 0200000;

				sprintf(temp, "%o", dest);
			}
			else
				sprintf(temp, "%o(r%d)", program[adrs], reg);
			break;

		case 7:			// index deferred or relative deferred (r7)
			adrs++;

			if (reg == 7)
			{
				dest = program[adrs] + 2 * adrs + 2;

				if (dest > 0177776)
					dest -= 0200000;

				sprintf(temp, "@%o", dest);
			}
			else
				sprintf(temp, "@%o(r%d)", program[adrs], reg);
			break;
	}

	strcat(outLine, temp);
	return adrs;
}

bool isoctal(char c)
{
	if (c >= '0' && c <= '7')
		return TRUE;

	return FALSE;
}

//	Get octal number from line in control file.
//	Return updated character pointer.

char *get_adrs(char *text, word *val)
{
	word	result, start;
	char	c;

	result = start = 0;
	c = toupper(*text);

	while (c)
	{
		if (c == ';')			// beginning of comment, ignore all else
			break;

		if (c == '\n')			// necessary because isspace() includes \n
			break;

		if (isspace(c))		// skip leading whitespace
		{
			text++;
			if (start)			// if result already begun...
				break;
		}
		else if (!isoctal(c))	// done if not octal character
			break;
		else
		{
			start = 1;			// flag beginning of result conversion
			result <<= 3;
			result |= ((word) c & 7);
			text++;
		}

		c = toupper(*text);
	}

	*val = result;				// pass number back to caller
	return(text);				// and return updated text pointer
}

// end of pdp11dasm.c

