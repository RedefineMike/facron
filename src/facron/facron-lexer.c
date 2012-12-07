/*
 *      This file is part of facron.
 *
 *      Copyright 2012 Marc-Antoine Perennou <Marc-Antoine@Perennou.com>
 *
 *      facron is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      facron is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with facron.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "facron-lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/fanotify.h>

struct FacronLexer
{
    FILE *file;
    char *line;
    char *line_beg;
    ssize_t len;
    ssize_t index;
};

static FacronChar
char_to_FacronChar (char c)
{
    switch (c)
    {
    case 'a': case 'A':
        return C_A;
    case 'b': case 'B':
        return C_B;
    case 'c': case 'C':
        return C_C;
    case 'd': case 'D':
        return C_D;
    case 'e': case 'E':
        return C_E;
    case 'f': case 'F':
        return C_F;
    case 'g': case 'G':
        return C_G;
    case 'h': case 'H':
        return C_H;
    case 'i': case 'I':
        return C_I;
    case 'j': case 'J':
        return C_J;
    case 'k': case 'K':
        return C_K;
    case 'l': case 'L':
        return C_L;
    case 'm': case 'M':
        return C_M;
    case 'n': case 'N':
        return C_N;
    case 'o': case 'O':
        return C_O;
    case 'p': case 'P':
        return C_P;
    case 'q': case 'Q':
        return C_Q;
    case 'r': case 'R':
        return C_R;
    case 's': case 'S':
        return C_S;
    case 't': case 'T':
        return C_T;
    case 'u': case 'U':
        return C_U;
    case 'v': case 'V':
        return C_V;
    case 'w': case 'W':
        return C_W;
    case 'x': case 'X':
        return C_X;
    case 'y': case 'Y':
        return C_Y;
    case 'z': case 'Z':
        return C_Z;
    case '_':
        return C_UNDERSCORE;
    case '|':
        return C_PIPE;
    case ' ': case '\t': case '\n': case '\r':
        return C_SPACE;
    case ',':
        return C_COMMA;
    default:
        return C_OTHER;
    }
}

static unsigned long long
FacronToken_to_mask (FacronToken t)
{
    switch (t)
    {
    case T_FAN_ACCESS:
        return FAN_ACCESS;
    case T_FAN_MODIFY:
        return FAN_MODIFY;
    case T_FAN_CLOSE_WRITE:
        return FAN_CLOSE_WRITE;
    case T_FAN_CLOSE_NOWRITE:
        return FAN_CLOSE_NOWRITE;
    case T_FAN_OPEN:
        return FAN_OPEN;
    case T_FAN_Q_OVERFLOW:
        return FAN_Q_OVERFLOW;
    case T_FAN_OPEN_PERM:
        return FAN_OPEN_PERM;
    case T_FAN_ACCESS_PERM:
        return FAN_ACCESS_PERM;
    case T_FAN_ONDIR:
        return FAN_ONDIR;
    case T_FAN_EVENT_ON_CHILD:
        return FAN_EVENT_ON_CHILD;
    case T_FAN_CLOSE:
        return FAN_CLOSE;
    case T_FAN_ALL_EVENTS:
        return FAN_ALL_EVENTS;
    case T_FAN_ALL_PERM_EVENTS:
        return FAN_ALL_PERM_EVENTS;
    case T_FAN_ALL_OUTGOING_EVENTS:
        return FAN_ALL_OUTGOING_EVENTS;
    case T_EMPTY:
        return 0;
    default:
        fprintf (stderr, "Warning: unknown token: %d\n", t);
        return 0;
    }
}

static const char state_transitions_table[ERROR][NB_CHARS] =
{
          /*  A,  B,  C,  D,  E,  F,  G,  H,  I,  J,  K,  L,  M,  N,  O,  P,  Q,  R,  S,  T,  U,  V,  W,  X,  Y,  Z,  _,  |,   ,  ,, ... */
    [_0] = { __, __, __, __, __, F0, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, __, __ },
    /* FAN_ */
    [F0] = { F1, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [F1] = { __, __, __, __, __, __, __, __, __, __, __, __, __, F2, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [F2] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, F3, __, __, __, __ },
    [F3] = { A0, __, C0, __, E0, __, __, __, __, __, __, __, M0, __, O0, __, Q0, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    /* FAN_A */
    [A0] = { __, __, A1, __, __, __, __, __, __, __, __, AD, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    /* FAN_ACCESS */
    [A1] = { __, __, A2, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [A2] = { __, __, __, __, A3, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [A3] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, A4, __, __, __, __, __, __, __, __, __, __, __, __ },
    [A4] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, A5, __, __, __, __, __, __, __, __, __, __, __, __ },
    [A5] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, A6, _0, _0, _0, __ },
    /* FAN_ACCESS_PERM */
    [A6] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, A7, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [A7] = { __, __, __, __, A8, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [A8] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, A9, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [A9] = { __, __, __, __, __, __, __, __, __, __, __, __, AA, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [AA] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_MODIFY */
    [M0] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, M1, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [M1] = { __, __, __, M2, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [M2] = { __, __, __, __, __, __, __, __, M3, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [M3] = { __, __, __, __, __, M4, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [M4] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, M5, __, __, __, __, __, __ },
    [M5] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_CLOSE */
    [C0] = { __, __, __, __, __, __, __, __, __, __, __, C1, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [C1] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, C2, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [C2] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, C3, __, __, __, __, __, __, __, __, __, __, __, __ },
    [C3] = { __, __, __, __, C4, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [C4] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, C5, _0, _0, _0, __ },
    /* FAN_CLOSE_ */
    [C5] = { __, __, __, __, __, __, __, __, __, __, __, __, __, N0, __, __, __, __, __, __, __, __, W0, __, __, __, __, __, __, __, __ },
    /* FAN_CLOSE_WRITE */
    [W0] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, W1, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [W1] = { __, __, __, __, __, __, __, __, W2, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [W2] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, W3, __, __, __, __, __, __, __, __, __, __, __ },
    [W3] = { __, __, __, __, W4, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [W4] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_CLOSE_NOWRITE */
    [N0] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, N1, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [N1] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, N2, __, __, __, __, __, __, __, __ },
    [N2] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, N3, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [N3] = { __, __, __, __, __, __, __, __, N4, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [N4] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, N5, __, __, __, __, __, __, __, __, __, __, __ },
    [N5] = { __, __, __, __, N6, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [N6] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_O */
    [O0] = { __, __, __, __, __, __, __, __, __, __, __, __, __, OA, __, O1, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    /* FAN_OPEN */
    [O1] = { __, __, __, __, O2, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [O2] = { __, __, __, __, __, __, __, __, __, __, __, __, __, O3, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [O3] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, O4, _0, _0, _0, __ },
    /* FAN_OPEN_PERM */
    [O4] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, O5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [O5] = { __, __, __, __, O6, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [O6] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, O7, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [O7] = { __, __, __, __, __, __, __, __, __, __, __, __, O8, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [O8] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_Q_OVERFLOW */
    [Q0] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, Q1, __, __, __, __ },
    [Q1] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, Q2, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [Q2] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, Q3, __, __, __, __, __, __, __, __, __ },
    [Q3] = { __, __, __, __, Q4, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [Q4] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, Q5, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [Q5] = { __, __, __, __, __, Q6, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [Q6] = { __, __, __, __, __, __, __, __, __, __, __, Q7, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [Q7] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, Q8, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [Q8] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, Q9, __, __, __, __, __, __, __, __ },
    [Q9] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_ONDIR */
    [OA] = { __, __, __, OB, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OB] = { __, __, __, __, __, __, __, __, OC, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OC] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, OD, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OD] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_EVENT_ON_CHILD */
    [E0] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, E1, __, __, __, __, __, __, __, __, __ },
    [E1] = { __, __, __, __, E2, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [E2] = { __, __, __, __, __, __, __, __, __, __, __, __, __, E3, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [E3] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, E4, __, __, __, __, __, __, __, __, __, __, __ },
    [E4] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, E5, __, __, __, __ },
    [E5] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, E6, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [E6] = { __, __, __, __, __, __, __, __, __, __, __, __, __, E7, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [E7] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, E8, __, __, __, __ },
    [E8] = { __, __, E9, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [E9] = { __, __, __, __, __, __, __, EA, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [EA] = { __, __, __, __, __, __, __, __, EB, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [EB] = { __, __, __, __, __, __, __, __, __, __, __, EC, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [EC] = { __, __, __, ED, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [ED] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_ALL_ */
    [AD] = { __, __, __, __, __, __, __, __, __, __, __, AE, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [AE] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, AF, __, __, __, __ },
    [AF] = { __, __, __, __, EG, __, __, __, __, __, __, __, __, __, OF, P0, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    /* FAN_ALL_EVENTS */
    [EG] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, EH, __, __, __, __, __, __, __, __, __ },
    [EH] = { __, __, __, __, EI, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [EI] = { __, __, __, __, __, __, __, __, __, __, __, __, __, EJ, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [EJ] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, EK, __, __, __, __, __, __, __, __, __, __, __ },
    [EK] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, EL, __, __, __, __, __, __, __, __, __, __, __, __ },
    [EL] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_ALL_PERM_EVENTS */
    [P0] = { __, __, __, __, P1, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [P1] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, P2, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [P2] = { __, __, __, __, __, __, __, __, __, __, __, __, P3, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [P3] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, P4, __, __, __, __ },
    [P4] = { __, __, __, __, P5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [P5] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, P6, __, __, __, __, __, __, __, __, __ },
    [P6] = { __, __, __, __, P7, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [P7] = { __, __, __, __, __, __, __, __, __, __, __, __, __, P8, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [P8] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, P9, __, __, __, __, __, __, __, __, __, __, __ },
    [P9] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, PA, __, __, __, __, __, __, __, __, __, __, __, __ },
    [PA] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ },
    /* FAN_ALL_OUTGOING_EVENTS */
    [OF] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, OG, __, __, __, __, __, __, __, __, __, __ },
    [OG] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, OH, __, __, __, __, __, __, __, __, __, __, __ },
    [OH] = { __, __, __, __, __, __, OI, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OI] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, OJ, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OJ] = { __, __, __, __, __, __, __, __, OK, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OK] = { __, __, __, __, __, __, __, __, __, __, __, __, __, OL, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OL] = { __, __, __, __, __, __, OM, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OM] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, ON, __, __, __, __ },
    [ON] = { __, __, __, __, OO, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OO] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, OP, __, __, __, __, __, __, __, __, __ },
    [OP] = { __, __, __, __, OQ, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OQ] = { __, __, __, __, __, __, __, __, __, __, __, __, __, OR, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OR] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, OS, __, __, __, __, __, __, __, __, __, __, __ },
    [OS] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, OT, __, __, __, __, __, __, __, __, __, __, __, __ },
    [OT] = { __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, _0, _0, _0, __ }
          /*  A,  B,  C,  D,  E,  F,  G,  H,  I,  J,  K,  L,  M,  N,  O,  P,  Q,  R,  S,  T,  U,  V,  W,  X,  Y,  Z,  _,  |, \ ,  ,, ... */
};

static inline bool
is_space (char c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

bool
facron_lexer_read_line (FacronLexer *lexer)
{
    if (lexer->line_beg)
        free (lexer->line_beg);

    size_t dummy_len = 0;

    lexer->len = getline (&lexer->line, &dummy_len, lexer->file);
    lexer->line_beg = lexer->line;
    lexer->index = 0;

    return (lexer->len >= 1);
}

bool
facron_lexer_invalid_line (FacronLexer *lexer)
{
    return (lexer->line == NULL || is_space (lexer->line[0]));
}

bool
facron_lexer_end_of_line (FacronLexer *lexer)
{
    return (lexer->len == 0 || lexer->index == lexer->len);
}

char *
facron_lexer_read_string (FacronLexer *lexer)
{
    if (!lexer->line || !lexer->len)
        return NULL;

    char delim = (lexer->line[0] == '"' || lexer->line[0] == '\'') ? lexer->line[0] : '\0';
    if (delim != '\0')
    {
        ++lexer->line;
        --lexer->len;
    }

    const char *line_beg = lexer->line;

    while (0 < lexer->len &&
           ((delim == '\0' && !is_space (lexer->line[0])) ||
           (delim != '\0' && lexer->line[0] != delim)))
    {
        ++lexer->line;
        --lexer->len;
    }
    lexer->line[0] = '\0';
    ++lexer->line;
    --lexer->len;

    return strdup (line_beg);
}

void
facron_lexer_skip_spaces (FacronLexer *lexer)
{
    while (is_space (lexer->line[0]))
    {
        ++lexer->line;
        --lexer->len;
    }
}

FacronResult
facron_lexer_next_token (FacronLexer *lexer, unsigned long long *mask)
{
    if (lexer->line == NULL)
        return R_ERROR;

    FacronState state = EMPTY;
    while (lexer->index < lexer->len)
    {
        FacronState prev_state = state;
        FacronChar c = char_to_FacronChar (lexer->line[lexer->index]);
        state = state_transitions_table[state][c];
        switch (state)
        {
        case ERROR:
            lexer->line[lexer->len - 1] = '\0';
            fprintf (stderr, "Error at char %c: \"%s\" not understood\n", lexer->line[lexer->index], lexer->line + lexer->index);
            return R_ERROR;
        case EMPTY:
            *mask = FacronToken_to_mask (prev_state);
            ++lexer->line;
            --lexer->len;
            switch (c)
            {
            case C_SPACE:
                lexer->line += (lexer->index);
                lexer->len -= (lexer->index);
                lexer->index = 0;
                return R_END;
            case C_COMMA:
                return R_COMMA;
            case C_PIPE:
                return R_PIPE;
            default:
                return R_ERROR;
            }
        default:
            break;
        }
        ++lexer->index;
    }

    return R_ERROR;
}

bool
facron_lexer_reload_file (FacronLexer *lexer)
{
    if (lexer->file)
        fclose (lexer->file);

    lexer->file = fopen (SYSCONFDIR "/facron.conf", "ro");
    lexer->line = lexer->line_beg = NULL;
    lexer->index = 0;

    if (!lexer->file)
    {
        fprintf (stderr, "Error: could not load configuration file, does \"" SYSCONFDIR "/facron.conf\" exist?\n");
        return false;
    }

    return true;
}

void
facron_lexer_free (FacronLexer *lexer)
{
    if (lexer->file)
        fclose (lexer->file);
    if (lexer->line_beg)
        free (lexer->line_beg);
    free (lexer);
}

FacronLexer *
facron_lexer_new (void)
{
    FacronLexer *lexer = (FacronLexer *) malloc (sizeof (FacronLexer));

    lexer->file = NULL;
    facron_lexer_reload_file (lexer);

    return lexer;
}