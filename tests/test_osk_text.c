/* osk_text_tidy (src/ui/osk_text.c): text from the system keyboard with spaces
 * ignored the way the engine ignores them - kept only where they separate two
 * letters or a decimal from "(" - quotes untouched, and an auto-capitalised
 * lone letter folded to the lower-case letter typed elsewhere. */
#include "testlib.h"
#include "ui/osk_text.h"

static char buf[256];

static const char *T(const char *s)
{
	snprintf(buf, sizeof buf, "%s", s);
	osk_text_tidy(buf);
	return buf;
}

int main(void)
{
	/* inputs as typed by a user */
	CHECK_STR(T("x= -2 and x= -3"), "x=-2and x=-3");
	CHECK_STR(T("X = -2 and x = -3"), "x=-2and x=-3");
	CHECK_STR(T("( x + 2 )( x + 3 )"), "(x+2)(x+3)");
	CHECK_STR(T("( x + 2 ) ( x + 3 )"), "(x+2)(x+3)");
	CHECK_STR(T("20 % of 300"), "20%of300");
	CHECK_STR(T("2 x ^ 2 + 3"), "2x^2+3");
	CHECK_STR(T("(X+2)(x+3)"), "(x+2)(x+3)");
	CHECK_STR(T("x = 2 or x = 3"), "x=2or x=3");
	CHECK_STR(T("x=-2, x=-3"), "x=-2,x=-3");

	/* spaces only kept where the lexer reads them */
	CHECK_STR(T("a sin(30)"), "a sin(30)");       /* not asin */
	CHECK_STR(T("p i"), "p i");                   /* not pi */
	CHECK_STR(T("0.3 (18)"), "0.3 (18)");         /* product, not recurring */
	CHECK_STR(T("0.3(18)"), "0.3(18)");
	CHECK_STR(T("2 (18)"), "2(18)");
	CHECK_STR(T("x . 5"), "x.5");
	CHECK_STR(T("1 000"), "1000");
	CHECK_STR(T("x < = 3"), "x<=3");
	CHECK_STR(T("2 \xC3\x97 3"), "2\xC3\x97" "3");
	CHECK_STR(T("2 \xCF\x80 r"), "2\xCF\x80r");
	CHECK_STR(T("   x   "), "x");
	CHECK_STR(T("   "), "");
	CHECK_STR(T(""), "");
	CHECK_STR(T("x\t+\n1\r"), "x+1");
	CHECK_STR(T("x\xC2\xA0+\xC2\xA0" "1"), "x+1");               /* no-break space */
	CHECK_STR(T("x\xE3\x80\x80+\xE3\x80\x80" "1"), "x+1");       /* ideographic space */
	CHECK_STR(T("a  \t b"), "a b");

	/* quoted text as typed */
	CHECK_STR(T("\"Area\" = 12 \"square cm\""), "\"Area\"=12\"square cm\"");
	CHECK_STR(T("x = \"no  answer\""), "x=\"no  answer\"");

	/* capitals: only a lone one, only when the lower-case letter is there too */
	CHECK_STR(T("X + 2"), "X+2");
	CHECK_STR(T("Ans + x"), "Ans+x");
	CHECK_STR(T("Ans + a"), "Ans+a");
	CHECK_STR(T("nCr(5, 2) + c"), "nCr(5,2)+c");
	CHECK_STR(T("E + e"), "E+e");
	CHECK_STR(T("A = 2a"), "a=2a");
	CHECK_STR(T("Xy + x"), "Xy+x");
	CHECK_STR(T("\"X\" x"), "\"X\"x");

	osk_text_tidy(NULL);
	CHECK(1, "NULL ignored");
	return test_finish();
}
