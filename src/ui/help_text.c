/* help_text.c - the words of the Help pages. Plain data; each line is short enough
 * to fit the panel at 17 px without wrapping. Button names: ✕ confirm, ○ back. */
#include "ui/help.h"

static const char *const p_start[] = {
	"# Pick a mode",
	"Press L and R to switch: Calc, Scientific, Algebra, Graph, GCSE.",
	"You can also tap a mode at the top of the screen.",
	"",
	"# Type and work it out",
	"Move round the keypad with the d-pad. Press a key with \xE2\x9C\x95, or tap it.",
	"START (or the = key) works it out.",
	"\xE2\x97\x8B deletes the last thing you typed.",
	"",
	"# Settings and Help",
	"SELECT opens Settings, where the top row opens this Help.",
	"The ? at the top right opens it too.",
	NULL,
};

static const char *const p_answers[] = {
	"# Exact answers",
	"Answers stay exact where they can: fractions, surds like 2\xE2\x88\x9A" "3, and \xCF\x80.",
	"\xE2\x96\xA1 swaps exact \xE2\x87\x94 decimal (the S\xE2\x87\x94" "D key does the same).",
	"In Calc and Scientific, the buttons under the answer convert it:",
	"\xE2\x86\x92 Fraction, \xE2\x86\x92 Decimal, \xE2\x86\x92 Percent, Standard form, Surd.",
	"",
	"# Step-by-step working",
	"\xE2\x96\xB3 opens the working for the answer. \xE2\x96\xB2\xE2\x96\xBC scrolls it, \xE2\x97\x8B closes it.",
	"",
	"# Use an earlier sum again",
	"Press \xE2\x96\xB2 on the input line to go up into your history.",
	"\xE2\x9C\x95 puts that sum back to edit, \xE2\x96\xA1 puts its answer in,",
	"\xE2\x96\xB3 shows its working.",
	NULL,
};

static const char *const p_algebra[] = {
	"# Solve an equation",
	"Type an equation such as 2x+3=11, then press = to solve it.",
	"It also solves sin(x)=0.5, 2^(x+3)=4^x, \xE2\x88\x9A(x+3)=x\xE2\x88\x92" "3 and cubics.",
	"You can type \"solve\" or \"factorise\" first: solve x^2=9",
	"",
	"# Choose what to do",
	"The Actions buttons are Solve, Factorise, Expand, Simplify,",
	"Complete the square and Rearrange. Press \xE2\x97\x80 at the left edge of",
	"the keypad to reach them, or just tap one.",
	"",
	"# Typing letters and powers",
	"The keypad has x y a b. The \xE2\x8C\xA8 key opens the keyboard for any letter.",
	"Use ^ for powers: x^2 means x squared (x\xC2\xB2 typed on the keyboard works too).",
	"",
	"# Simultaneous equations",
	"Separate the two with a comma: 2x+3y=12, x-y=1",
	"",
	"# Solutions back to an equation",
	"Type x=-2 and x=-3 then press =  to get x^2+5x+6=0.",
	NULL,
};

static const char *const p_graph[] = {
	"# Functions",
	"Up to six functions of x. \xE2\x9C\x95 edits one, e.g. x^2-3x+2 or sin(x).",
	"\xE2\x96\xB3 shows or hides a function, \xE2\x96\xA1 clears it.",
	"START (or \xE2\x96\xB6) plots them.",
	"",
	"# On the graph",
	"The d-pad pans. L and R zoom. \xE2\x96\xA1 resets the view.",
	"\xE2\x9C\x95 lists the roots and turning points.",
	"\xE2\x96\xB3 traces along a curve: \xE2\x97\x80\xE2\x96\xB6 moves, \xE2\x96\xB2\xE2\x96\xBC changes curve.",
	"START opens a table of values.",
	"\xE2\x97\x8B goes back to the list of functions.",
	NULL,
};

static const char *const p_gcse[] = {
	"# Find a tool",
	"Choose a topic with \xE2\x96\xB2\xE2\x96\xBC, then press \xE2\x9C\x95 (or \xE2\x96\xB6) to see its tools.",
	"Press \xE2\x9C\x95 on a tool to open it.",
	"",
	"# Use a tool",
	"Fill the boxes: \xE2\x96\xB2\xE2\x96\xBC picks a box, \xE2\x9C\x95 lets you type in it.",
	"Boxes with choices change with \xE2\x97\x80\xE2\x96\xB6.",
	"START works it out.",
	"\xE2\x96\xB3 shows the working.",
	"\xE2\x97\x8B goes back to the tools.",
	NULL,
};

static const char *const p_errors[] = {
	"# A bracket is missing: add a )",
	"A bracket was opened but not closed. Add ) where it ends.",
	"# Something is missing at the end",
	"Nothing comes after a + \xE2\x88\x92 \xC3\x97 \xC3\xB7 or ^. Finish the sum.",
	"# That symbol can't go there",
	"A sign or bracket is in the wrong place, like a ) with no ( before it.",
	"# There is nothing inside the brackets",
	"Put something between the brackets, or delete them.",
	"# Maths error: can't divide by 0",
	"The bottom of a fraction is 0. Change it to another number.",
	"# Maths error: square root of a negative number",
	"You can't take the square root of a negative. Check the signs.",
	"# Maths error: answer too big",
	"The number is too large for the calculator. Try smaller numbers.",
	"# Can't read that  /  Unknown symbol",
	"There is a character the calculator doesn't know. Delete it.",
	"# It has letters in it: try Simplify or Solve",
	"A letter is in a sum that needs numbers. Use Algebra mode.",
	"# Couldn't work that out",
	"That action doesn't fit this input. Check it, or try another action.",
	"After any error, \xE2\x97\x8B takes you back to edit the input.",
	NULL,
};

const HelpPage g_help_pages[] = {
	{ "Getting started", p_start },
	{ "Answers and working", p_answers },
	{ "Algebra", p_algebra },
	{ "Graph", p_graph },
	{ "GCSE tools", p_gcse },
	{ "Error messages", p_errors },
};

const int g_help_npages = (int)(sizeof g_help_pages / sizeof g_help_pages[0]);
