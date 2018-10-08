#ifdef __linux__

# ELF note header to mark dgsh-compatible programs
# See http://www.netbsd.org/docs/kernel/elf-notes.html
    .comm dgsh_force_include,4,4
    .section ".note.ident", "a"
    .p2align 2
    .long 1f - 0f		# name size (not including padding)
    .long 3f - 2f		# desc size (not including padding)
    .long 1			# type
0:  .asciz "DSpinellis/dgsh"	# name
1:  .p2align 2
2:  .long 0x00000001		# desc
    .long 0x00000000
3:  .p2align 2

#elif __APPLE__

    .section ".note.ident", "a"
    .asciz "DSpinellis/dgsh"
    .section	__TEXT,__text,regular,pure_instructions
    .macosx_version_min 10, 13
    .globl	_dgsh_force_include     ## @dgsh_force_include
    .zerofill __DATA,__common,_dgsh_force_include,4,2

    .subsections_via_symbols

#endif
