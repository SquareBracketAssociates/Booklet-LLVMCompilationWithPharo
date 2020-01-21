	.text
	.file	"my_module"
	.globl	sum
	.p2align	4, 0x90
	.type	sum,@function
sum:
	.cfi_startproc
	addl	%esi, %edi
	movl	%edi, %eax
	retq
.Lfunc_end0:
	.size	sum, .Lfunc_end0-sum
	.cfi_endproc


	.section	".note.GNU-stack","",@progbits
