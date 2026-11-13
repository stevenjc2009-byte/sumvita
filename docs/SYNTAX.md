# Sumvita display syntax

One text syntax carries maths between every part of the app:

- the input editor serialises what the user built into it,
- `calc_run()` parses it,
- the engine prints answers and working-out lines in it,
- the UI parses those lines again and lays them out in natural display.

It is UTF-8. `ex_parse()` (src/engine/parse.c) is the reference implementation.

## Numbers
| Text | Meaning |
|---|---|
| `12`, `3.25`, `.5` | exact rational. A typed decimal keeps `NF_DECIMAL` so it prints back as `3.25` |
| `2_1/3` | mixed number 2⅓ (`E_MIXED`), shown with a whole part and a small fraction |
| `recur(0.1(27))`, or bare `0.1(27)` | recurring decimal 0.1272727… (`E_RECUR`). Bare form: a number with a `.` immediately followed by `(digits)` — no space, operator or anything but digits in the bracket. `2(18)`, `0.3×(18)`, `0.3 (18)`, `0.3(x)` stay multiplication. The printer always writes `recur(...)` |

No `e`-notation in input. Standard form is typed as `3.2×10^5` (`*` and `×` both mean times).

## Accepted alternative input (normalised before parsing, src/engine/parse_norm.c)
| Typed | Read as |
|---|---|
| `²` `³` | `^2` `^3` |
| `½` `¼` `¾` | `(1/2)` `(1/4)` `(3/4)`; straight after a whole number (`2½`) a mixed number `2_1/2` |
| `–` `—` (en/em dash) | `-` |
| `x**2` | `x^2` |
| `;` | same as `,` (list and equation-system separator) |
| `solve`, `simplify`, `expand`, `factorise`, `factorize`, `factor`, `rearrange` first, any case, optional `:` | the word is dropped; with the `=` (auto) key it also picks that action. Error positions still point into what was typed |
| `X` and `x` both present | `X` means `x`. A lone `X` stays a variable `X` |

Parse errors are written in plain English ("Something is missing at the end", "That symbol can't go there", "A bracket is missing: add a )").

## Variables and constants
- Any single letter a–z or A–Z except `e` is a variable. `xy` = x·y, `2ab` = 2·a·b.
- `θ` is a variable.
- `π` or `pi` is π. `e` alone is Euler's number.
- Function names are matched first, longest wins: `sin`, `cos`, `tan`, `asin` (`sin⁻¹` also accepted as `sin^-1(`), `acos`, `atan`, `sinh`, `cosh`, `tanh`, `ln`, `log`, `sqrt`, `√`, `cbrt`, `root`, `abs`, `floor`, `ceil`, `round`, `nCr`, `nPr`, `exp`, `recur`.
- `Ans` is the previous answer.

## Operators (lowest to highest precedence)
1. `,` list (top level only; also separates function arguments).
   A bare `and` or `or` between two relations is the same as a comma: `x=-2 and x=-3`,
   `2x+3y=12 and x-y=1`, `x=1 or x=2`, and with no spaces `x=-2andx=-3`, `x=1orx=2`.
   Rule: no letter just before the word (a letter just after is fine; spaces are still
   ignored) and the text on both sides, up to the next `,`/`and`/`or`, has a relation sign.
   Anywhere else the letters are letters: `and` = a·n·d, `andx`, `2a+n`, `x=and`, `x=or+1`.
   The echo shows commas.
2. relations `=`, `<`, `>`, `<=`/`≤`, `>=`/`≥`, `!=`/`≠` (chains like `-3<2x+1<=9` allowed)
3. `+`, `-`, `±`
4. `*`, `×`, `/`, `÷`, implicit multiplication (`2x`, `3(x+1)`, `(x+1)(x-2)`, `2π`, `x sin(x)`).
   `of` also means `×`, for percentages: `20% of 300` = 60 (so `of` is never the letters o and f)
5. unary `-`, unary `±`
6. `^` (right associative: `2^3^2` = 2^9). `x^-1` is allowed (unary minus binds into the exponent).
7. postfix `!`, `%`, `°`
8. function call, brackets `( )` and `[ ]`, `|x|` for absolute value

`2/3x` means (2/3)·x. `1/2x` likewise. Write `1/(2x)` for the other one.
`-x^2` means −(x²). `sin x` without brackets takes the next factor: `sin 30` = sin(30), `sin 2x` = sin(2x).
`√` without brackets takes the next factor: `√2x` = √2 · x is NOT what people mean; `√` takes the next atom only, so `√2x` = (√2)x. The input editor always emits `√(...)`.

## Text
`"or"`, `"and"`, `"so"` — quoted text is shown as words (`E_TEXT`). Used in working-out lines such as
`x=2 "or" x=-3`.

- A text may be either side of a relation: `"HCF"=6`, `"HCF"=2×3=6`, `x="none"` (GCSE tool answers).
  At the start of a relation it is an operand only when a relation sign follows it.
- A text after a value is a unit or a word: `"Area"=12 "cm²", "Perimeter"=14 "cm"`, `1 "metre"=100 "cm"`.
- In a typed list the separators are kept as typed (comma or space; `EF_COMMA`). A list the engine
  builds puts a comma between two maths items and only a space next to a text.
- Solve answers that are alternatives come back joined by `"or"`: `x=2 "or" x=3`,
  `x<-2 "or" x>2`, pairs `x=-1, y=0 "or" x=2, y=3`. One answer in several letters keeps commas: `x=3, y=2`.

## Printing rules (`ex_print`)
- Minimal brackets; keep brackets the user typed (`EF_PAREN`).
- Product of a number and a bracket prints as `3(x+1)`; product of brackets `(x+2)(x-2)`.
- Coefficient first, variables alphabetical: `2x^2y` not `y2x^2`.
- Negative terms print as subtraction: `x^2-4x+3`.
- A number coefficient that is a fraction puts the whole term over its bottom: `(1/2)x` → `x/2`,
  `(5/6)x` → `5x/6`, `(4/3)π` → `4π/3`, `(2/3)(x+1)` → `2(x+1)/3`. Never `5/6x`.
- A typed fraction in front of a letter or number is bracketed: `2/3x` prints as `(2/3)x`;
  in front of a bracket it stays `1/2(x+1)^2`. Each form re-parses to the same value.
- A `*` is only printed between two numbers (`3*10^5` prints as `3×10^5`).
