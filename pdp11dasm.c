
/*
 * PDP11Dasm PDP-11 Disassembler - Copyright (C) 2004 by
 * Jeffery L. Post
 * theposts@pacbell.net
 *
 * Version 0.0.1 - 01/16/04
 *
 * This is a quick-n-dirty hack cranked out in two days just for the fun of
 * it. If you find bugs, please notify the author at the above email address.
 * The decoding of floating point opcodes is probably not correct.
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
#include	<malloc.h>
#include	<sys/types.h>
#include	<sys/stat.h>

#ifndef FALSE
#define	FALSE	0
#endif
#ifndef TRUE
#define	TRUE	1
#endif

#define	FN_LEN			128
#define	byte				unsigned char
#define	word				unsigned short
#define	MODEREG_MASK	0x3f
#define	OFFSET_MASK		0xff
#define	TAB				0x09

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

char	src[FN_LEN], dst[FN_LEN];	// file name buffers
char	ctl[FN_LEN];					// control file name
char	outLine[128];					// disassembled output line
char	temp[128];
FILE	*fp = NULL;						// general purpose file struct
FILE	*disFile = NULL;				// disassembled file
struct stat	fstatus;
int	pc;								// current program counter
int	breakLine;
word	*program;

//////////////////////////////////
//
//			The Main Program
//
//////////////////////////////////

int main(int argc, char *argv[])
{
	int	max;

	if (argc < 2)
	{
		printf("usage: pdp11dasm [file]\n");
		return 1;
	}

	stat(argv[1], &fstatus);
	max = (int) fstatus.st_size;
	program = (word *) malloc(max);

	if (!program)
	{
		printf("Can't allocate memory for program (%d bytes)\n", max);
		return 1;
	}

	fp = fopen(argv[1], "rb");

	if (!fp)
	{
		printf("Can't open file '%s'\n", argv[1]);
		return 1;
	}

	fread(program, sizeof(byte), max, fp);
	max /= 2;

	for (pc=0; pc<max; )
	{
		pc = decode(pc);
	}

	printf("\n");
	fclose(fp);
	return(0);
}								//  End of Main

// Output Ascii for 16 bit word

void printAscii(word data)
{
	char	chr;

	chr = data & 0x7f;

	if (chr < ' ' || chr > '~')
		chr = '.';

	printf("%c", chr);

	chr = (data >> 8) & 0x7f;

	if (chr < ' ' || chr > '~')
		chr = '.';

	printf("%c", chr);
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
	printf("\n%06o: %06o", adrs * 2, code);
	outLine[0] = '\0';

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

	switch (adrs - start)
	{
		case 1:
			printf("              ");
			break;

		case 2:
			printf(" %06o       ", program[start + 1]);
			break;

		case 3:
			printf(" %06o %06o", program[start + 1], program[start + 2]);
			break;
	}

	printf("%s", outLine);		// pos = # characters printed starting at col 32

	i = 1;
	pos = 32;

	while (outLine[i])
	{
		if (outLine[i] != TAB)
			pos++;
		else do
			pos++;
		while (pos & 7);

		i++;
	}

	while (pos < 64)
	{
		printf("\t");
		pos += 8;
		pos &= 0xf8;
	}

	printf("; ");

	for (i=start; i<adrs; i++)
		printAscii(program[i]);

	if (breakLine)
		printf("\n;");

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
	sprintf(outLine, "\tJSR\tR%d,", reg);
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
					sprintf(outLine, "\tHALT");
					breakLine = TRUE;
					break;

				case 1:
					sprintf(outLine, "\tWAIT");
					break;

				case 2:
					sprintf(outLine, "\tRTI");
					breakLine = TRUE;
					break;

				case 3:
					sprintf(outLine, "\tBPT");
					break;

				case 4:
					sprintf(outLine, "\tIOT");
					break;

				case 5:
					sprintf(outLine, "\tRESET");
					break;

				case 6:
					sprintf(outLine, "\tRTT");
					breakLine = TRUE;
					break;

				case 7:
					sprintf(outLine, "\tMFPT");
					break;

				default:
					invalid();
					break;
			}
			break;

		case 1:		// jmp
			sprintf(outLine, "\tJMP\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			breakLine = TRUE;
			break;

		case 2:		// rts, spl, nop, cond codes & unimplemented
			if (program[adrs] < 000210)
			{
				sprintf(outLine, "\tRTS\tR%d", program[adrs] & 7);
				breakLine = TRUE;
			}
			else if (program[adrs] < 000230)
				invalid();
			else if (program[adrs] < 000240)		// spl
				sprintf(outLine, "\tSPL\t%d", program[adrs] & 7);
			else
			{
				if (program[adrs] == 0240)
					sprintf(outLine, "\tNOP");
				else switch (program[adrs])		// condition codes
				{
					case 0241:
						sprintf(outLine, "\tCLC");
						break;

					case 0242:
						sprintf(outLine, "\tCLV");
						break;

					case 0244:
						sprintf(outLine, "\tCLZ");
						break;

					case 0250:
						sprintf(outLine, "\tCLN");
						break;

					case 0257:
						sprintf(outLine, "\tCCC");
						break;

					case 0261:
						sprintf(outLine, "\tSEC");
						break;

					case 0262:
						sprintf(outLine, "\tSEV");
						break;

					case 0264:
						sprintf(outLine, "\tSEZ");
						break;

					case 0270:
						sprintf(outLine, "\tSEN");
						break;

					case 0277:
						sprintf(outLine, "\tSCC");
						break;

					default:
						invalid();
						break;
				}
			}
			break;

		case 3:		// swab
			sprintf(outLine, "\tSWAB\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			break;

		case 4:		// br
		case 5:
		case 6:
		case 7:
			sprintf(outLine, "\tBR\t");
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
				sprintf(outLine, "\tBEQ\t");
			else
				sprintf(outLine, "\tBNE\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 2:		// bge, blt
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tBLT\t");
			else
				sprintf(outLine, "\tBGE\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 3:		// bgt, ble
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tBLE\t");
			else
				sprintf(outLine, "\tBGT\t");
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
					sprintf(outLine, "\tCLR\t");
					break;

				case 1:
					sprintf(outLine, "\tCOM\t");
					break;

				case 2:
					sprintf(outLine, "\tINC\t");
					break;

				case 3:
					sprintf(outLine, "\tDEC\t");
					break;

				case 4:
					sprintf(outLine, "\tNEG\t");
					break;

				case 5:
					sprintf(outLine, "\tADC\t");
					break;

				case 6:
					sprintf(outLine, "\tSBC\t");
					break;

				case 7:
					sprintf(outLine, "\tTST\t");
					break;
			}

			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			break;

		case 6:		// ror, rol, asr, asl, mark, mfpi, mtpi, sxt
			code = (program[adrs] >> 6) & 7;

			switch (code)
			{
				case 0:
					sprintf(outLine, "\tROR\t");
					break;

				case 1:
					sprintf(outLine, "\tROL\t");
					break;

				case 2:
					sprintf(outLine, "\tASR\t");
					break;

				case 3:
					sprintf(outLine, "\tASL\t");
					break;

				case 4:
					sprintf(outLine, "\tMARK\t%o", program[adrs] & 0x3f);
					skipOperand = TRUE;
					break;

				case 5:
					sprintf(outLine, "\tMFPI\t");
					break;

				case 6:
					sprintf(outLine, "\tMTPI\t");
					break;

				case 7:
					sprintf(outLine, "\tSXT\t");
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

	sprintf(outLine, "\tMOV\t");
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

	sprintf(outLine, "\tCMP\t");
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

	sprintf(outLine, "\tBIT\t");
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

	sprintf(outLine, "\tBIC\t");
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

	sprintf(outLine, "\tBIS\t");
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

	sprintf(outLine, "\tADD\t");
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
			sprintf(outLine, "\tMUL\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",R%d", reg);
			strcat(outLine, temp);
			break;

		case 1:		// div
			sprintf(outLine, "\tDIV\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",R%d", reg);
			strcat(outLine, temp);
			break;

		case 2:		// ash
			sprintf(outLine, "\tASH\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",R%d", reg);
			strcat(outLine, temp);
			break;

		case 3:		// ashc
			sprintf(outLine, "\tASHC\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",R%d", reg);
			strcat(outLine, temp);
			break;

		case 4:		// xor
			sprintf(outLine, "\tXOR\t");
			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			sprintf(temp, ",R%d", reg);
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
						sprintf(outLine, "\tFADD\tR%d", program[adrs] & 7);
						break;

					case 1:
						sprintf(outLine, "\tFSUB\tR%d", program[adrs] & 7);
						break;

					case 2:
						sprintf(outLine, "\tFMUL\tR%d", program[adrs] & 7);
						break;

					case 3:
						sprintf(outLine, "\tFMUL\tR%d", program[adrs] & 7);
						break;
				}
			}
			break;

		case 6:		// unimplemented
			invalid();
			break;

		case 7:		// sob
			offset = 2 * (adrs + 1) - 2 * (program[adrs] & 0x3f);
			sprintf(outLine, "\tSOB\tR%d,%o", reg, offset);
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
				sprintf(outLine, "\tBMI\t");
			else
				sprintf(outLine, "\tBPL\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 1:		// bhi, blos
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tBLOS\t");
			else
				sprintf(outLine, "\tBHI\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 2:		// bvc, bvs
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tBVS\t");
			else
				sprintf(outLine, "\tBVC\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 3:		// bcc, bhis, bcs, blo
			if (program[adrs] & 0x100)
				sprintf(outLine, "\tBCS\t");
			else
				sprintf(outLine, "\tBCC\t");
			doOffset(adrs, program[adrs] & OFFSET_MASK);
			break;

		case 4:		// emt, trap
			if (program[adrs] < 0104400)
				sprintf(outLine, "\tEMT");
			else
				sprintf(outLine, "\tTRAP");

			sprintf(temp, "\t%o", program[adrs] & 0xff);
			strcat(outLine, temp);
			break;

		case 5:		// clrb, comb, incb, decb, negb, adcb, sbcb, tstb
			code = (program[adrs] >> 6) & 7;

			switch (code)
			{
				case 0:
					sprintf(outLine, "\tCLRB\t");
					break;

				case 1:
					sprintf(outLine, "\tCOMB\t");
					break;

				case 2:
					sprintf(outLine, "\tINCB\t");
					break;

				case 3:
					sprintf(outLine, "\tDECB\t");
					break;

				case 4:
					sprintf(outLine, "\tNEGB\t");
					break;

				case 5:
					sprintf(outLine, "\tADCB\t");
					break;

				case 6:
					sprintf(outLine, "\tSBCB\t");
					break;

				case 7:
					sprintf(outLine, "\tTSTB\t");
					break;
			}

			adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
			break;

		case 6:		// rorb, rolb, asrb, aslb, mfpd, mtpd
			code = (program[adrs] >> 6) & 7;

			switch (code)
			{
				case 0:
					sprintf(outLine, "\tRORB\t");
					break;

				case 1:
					sprintf(outLine, "\tROLB\t");
					break;

				case 2:
					sprintf(outLine, "\tASRB\t");
					break;

				case 3:
					sprintf(outLine, "\tASLB\t");
					break;

				case 4:
					invalid();
					skipOperand = TRUE;
					break;

				case 5:
					sprintf(outLine, "\tMFPD\t");
					break;

				case 6:
					sprintf(outLine, "\tMTPD\t");
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

	sprintf(outLine, "\tMOVB\t");
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

	sprintf(outLine, "\tCMPB\t");
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

	sprintf(outLine, "\tBITB\t");
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

	sprintf(outLine, "\tBICB\t");
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

	sprintf(outLine, "\tBISB\t");
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

	sprintf(outLine, "\tSUB\t");
	modereg = program[adrs];
	adrs = doOperand(adrs, (modereg >> 6) & MODEREG_MASK);
	strcat(outLine, ",");
	adrs = doOperand(adrs, modereg & MODEREG_MASK);
	return adrs + 1;
}

// Output floating point instruction operands

int doFPOperand(int adrs)
{
	int	mode, reg, acc;

	acc = (program[adrs] >> 6) & 3;
	mode = (program[adrs] >> 3) & 7;
	reg = program[adrs] & 7;

	if (!mode)
	{
		sprintf(temp, "\tF%d,", acc);		// mode != 0, restricted to AC0-AC3
		adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
	}
	else		// actually, this makes no sense at all; there are only 3 bits for AC,
	{			// so it's not possible to access AC4-AC5
		sprintf(temp, "\tF%d,", acc);
		adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
	}

	strcat(outLine, temp);

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
					sprintf(outLine, "\tCFCC");
					break;

				case 0170001:
					sprintf(outLine, "\tSETF");
					break;

				case 0170002:
					sprintf(outLine, "\tSETI");
					break;

				case 0170011:
					sprintf(outLine, "\tSETD");
					break;

				case 0170012:
					sprintf(outLine, "\tSETL");
					break;

				default:
					code = (program[adrs] >> 6) & 3;

					switch (code)
					{
						case 0:
							invalid();
							break;

						case 1:
							sprintf(outLine, "\tLDFPS\t");
							adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
							break;

						case 2:
							sprintf(outLine, "\tSTFPS\t");
							adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
							break;

						case 3:
							sprintf(outLine, "\tSTST\t");
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
					sprintf(outLine, "\tCLRF\t");
					adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
					break;

				case 1:
					sprintf(outLine, "\tTSTF\t");
					adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
					break;

				case 2:
					sprintf(outLine, "\tABSF\t");
					adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
					break;

				case 3:
					sprintf(outLine, "\tNEGF\t");
					adrs = doOperand(adrs, program[adrs] & MODEREG_MASK);
					break;
			}
			break;

		case 0x02:	// mulf, muld
			sprintf(outLine, "\tMULF");
			adrs = doFPOperand(adrs);
			break;

		case 0x03:	// modf, modd
			sprintf(outLine, "\tMODF");
			adrs = doFPOperand(adrs);
			break;

		case 0x04:	// addf, addd
			sprintf(outLine, "\tADDF");
			adrs = doFPOperand(adrs);
			break;

		case 0x05:	// ldf, ldd
			sprintf(outLine, "\tLDF");
			adrs = doFPOperand(adrs);
			break;

		case 0x06:	// subf, subd
			sprintf(outLine, "\tSUBF");
			adrs = doFPOperand(adrs);
			break;

		case 0x07:	// cmpf, cmpd
			sprintf(outLine, "\tCMPF");
			adrs = doFPOperand(adrs);
			break;

		case 0x08:	// stf, std
			sprintf(outLine, "\tSTF");
			adrs = doFPOperand(adrs);
			break;

		case 0x09:	// divf, divd
			sprintf(outLine, "\tDIVF");
			adrs = doFPOperand(adrs);
			break;

		case 0x0a:	// stexp
			sprintf(outLine, "\tSTEXP");
			adrs = doFPOperand(adrs);
			break;

		case 0x0b:	// stcfi, stcfl, stcdi, stcdl
			break;

		case 0x0c:	// stcfd, stcdf
			sprintf(outLine, "\tSTCDF");
			adrs = doFPOperand(adrs);
			break;

		case 0x0d:	// ldexp
			sprintf(outLine, "\tLDEXP");
			adrs = doFPOperand(adrs);
			break;

		case 0x0e:	// ldcif, ldcid, ldclf, ldcld
			sprintf(outLine, "\tLDCIF");
			adrs = doFPOperand(adrs);
			break;

		case 0x0f:	// ldcdf, ldcfd
			sprintf(outLine, "\tLDCDF");
			adrs = doFPOperand(adrs);
			break;
	}

	return adrs + 1;
}

int doOperand(int adrs, int modereg)
{
	int	mode, reg, dest;

	mode = modereg >> 3;
	reg = modereg & 7;

	switch (mode)
	{
		case 0:			// direct
			sprintf(temp, "R%d", reg);
			break;

		case 1:			// register deferred
			sprintf(temp, "(R%d)", reg);
			break;

		case 2:			// auto-increment or immediate (r7)
			if (reg == 7)
			{
				adrs++;
				sprintf(temp, "#%o", program[adrs]);
			}
			else
				sprintf(temp, "(R%d)+", reg);
			break;

		case 3:			// auto-increment deferred or absolute (r7)
			if (reg == 7)
			{
				adrs++;
				sprintf(temp, "@#%o", program[adrs]);
			}
			else
				sprintf(temp, "@(R%d)+", reg);
			break;

		case 4:			// auto-decrement
			sprintf(temp, "-(R%d)", reg);
			break;

		case 5:			// auto-decrement deferred
			sprintf(temp, "@-(R%d)", reg);
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
				sprintf(temp, "%o(R%d)", program[adrs], reg);
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
				sprintf(temp, "@%o(R%d)", program[adrs], reg);
			break;
	}

	strcat(outLine, temp);
	return adrs;
}

// end of pdp11dasm.c

