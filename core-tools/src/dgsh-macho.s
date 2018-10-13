# MACHO note header to mark dgsh-compatible programs
    .section ".note.ident", "a"
    .asciz "DSpinellis/dgsh"
    .section	__TEXT,__text,regular,pure_instructions
    .macosx_version_min 10, 13
    .globl	_dgsh_force_include     ## @dgsh_force_include
    .zerofill __DATA,__common,_dgsh_force_include,4,2

    .subsections_via_symbols
