/*
 * ELF note header to mark dgsh-compatible programs
 * See http://www.netbsd.org/docs/kernel/elf-notes.html
 * Don't use line comments as these are not portable between
 * different CPU architectures.
 * https://en.wikipedia.org/wiki/GNU_Assembler#Single-Line_comments
 */

    .comm dgsh_force_include,4,4
    .section ".note.ident", "a"
    .p2align 2
    .long 1f - 0f		/* name size (not including padding) */
    .long 3f - 2f		/* desc size (not including padding) */
    .long 1			/* type */
0:  .asciz "DSpinellis/dgsh"	/* name */
1:  .p2align 2
2:  .long 0x00000001		/* desc */
    .long 0x00000000
3:  .p2align 2

