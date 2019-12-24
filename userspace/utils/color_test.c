
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "termcols.h"


int main(int argc, char **argv, char **envp)
{
    printf(TERM_COLOR_RESET TERM_COLOR_RED                           " Test RED!     ");
    printf(TERM_COLOR_RESET TERM_COLOR_BG_RED                        "Test BG RED!     ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_RED        "Test REV RED!     ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_BG_RED     "Test REV BG RED!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_GREEN                         " Test GREEN!   ");
    printf(TERM_COLOR_RESET TERM_COLOR_BG_GREEN                      "Test BG GREEN!   ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_GREEN      "Test REV GREEN!   ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_BG_GREEN   "Test REV BG GREEN!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_YELLOW                        " Test YELLOW!  ");
    printf(TERM_COLOR_RESET TERM_COLOR_BG_YELLOW                     "Test BG YELLOW!  ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_YELLOW     "Test REV YELLOW!  ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_BG_YELLOW  "Test REV BG YELLOW!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_BLUE                          " Test BLUE!    ");
    printf(TERM_COLOR_RESET TERM_COLOR_BG_BLUE                       "Test BG BLUE!    ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_BLUE       "Test REV BLUE!    ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_BG_BLUE    "Test REV BG BLUE!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_MAGENTA                       " Test MAGENTA! ");
    printf(TERM_COLOR_RESET TERM_COLOR_BG_MAGENTA                    "Test BG MAGENTA! ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_MAGENTA    "Test REV MAGENTA! ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_BG_MAGENTA "Test REV BG MAGENTA!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_CYAN                          " Test CYAN!    ");
    printf(TERM_COLOR_RESET TERM_COLOR_BG_CYAN                       "Test BG CYAN!    ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_CYAN       "Test REV CYAN!    ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_BG_CYAN    "Test REV BG CYAN!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_WHITE                         " Test WHITE!   ");
    printf(TERM_COLOR_RESET TERM_COLOR_BG_WHITE                      "Test BG WHITE!   ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_WHITE      "Test REV WHITE!   ");
    printf(TERM_COLOR_RESET TERM_COLOR_REVERSE TERM_COLOR_BG_WHITE   "Test REV BG WHITE!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_RED        " Test RED!     ");
    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_BG_RED     "Test BG RED!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_GREEN      " Test GREEN!   ");
    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_BG_GREEN   "Test BG GREEN!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_YELLOW     " Test YELLOW!  ");
    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_BG_YELLOW  "Test BG YELLOW!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_BLUE       " Test BLUE!    ");
    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_BG_BLUE    "Test BG BLUE!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_MAGENTA    " Test MAGENTA! ");
    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_BG_MAGENTA "Test BG MAGENTA!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_CYAN       " Test CYAN!    ");
    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_BG_CYAN    "Test BG CYAN!\n");

    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_WHITE      " Test WHITE!   ");
    printf(TERM_COLOR_RESET TERM_COLOR_BOLD TERM_COLOR_BG_WHITE   "Test BG WHITE!\n");

    printf(TERM_COLOR_RESET "\n");

    return 0;
}

