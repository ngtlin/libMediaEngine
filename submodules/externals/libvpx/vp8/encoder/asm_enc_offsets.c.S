	.arch armv5te
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 18, 4
	.file	"asm_enc_offsets.c"
	.text
	.align	2
	.global	main
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
#APP
@ 23 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_coeff EQU #4
@ 0 "" 2
@ 24 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_zbin EQU #20
@ 0 "" 2
@ 25 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_round EQU #28
@ 0 "" 2
@ 26 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_quant EQU #8
@ 0 "" 2
@ 27 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_quant_fast EQU #12
@ 0 "" 2
@ 28 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_zbin_extra EQU #32
@ 0 "" 2
@ 29 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_zrun_zbin_boost EQU #24
@ 0 "" 2
@ 30 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_quant_shift EQU #16
@ 0 "" 2
@ 32 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_blockd_qcoeff EQU #0
@ 0 "" 2
@ 33 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_blockd_dequant EQU #12
@ 0 "" 2
@ 34 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_blockd_dqcoeff EQU #4
@ 0 "" 2
@ 35 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_blockd_eob EQU #40
@ 0 "" 2
@ 38 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_base_src EQU #36
@ 0 "" 2
@ 39 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_src EQU #40
@ 0 "" 2
@ 40 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_src_diff EQU #0
@ 0 "" 2
@ 41 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_block_src_stride EQU #44
@ 0 "" 2
@ 43 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_blockd_predictor EQU #8
@ 0 "" 2
@ 46 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_lowvalue EQU #0
@ 0 "" 2
@ 47 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_range EQU #4
@ 0 "" 2
@ 48 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_value EQU #8
@ 0 "" 2
@ 49 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_count EQU #12
@ 0 "" 2
@ 50 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_pos EQU #16
@ 0 "" 2
@ 51 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_buffer EQU #20
@ 0 "" 2
@ 52 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_buffer_end EQU #24
@ 0 "" 2
@ 53 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_error EQU #28
@ 0 "" 2
@ 55 "vp8/encoder/asm_enc_offsets.c" 1
	
tokenextra_token EQU #6
@ 0 "" 2
@ 56 "vp8/encoder/asm_enc_offsets.c" 1
	
tokenextra_extra EQU #4
@ 0 "" 2
@ 57 "vp8/encoder/asm_enc_offsets.c" 1
	
tokenextra_context_tree EQU #0
@ 0 "" 2
@ 58 "vp8/encoder/asm_enc_offsets.c" 1
	
tokenextra_skip_eob_node EQU #7
@ 0 "" 2
@ 59 "vp8/encoder/asm_enc_offsets.c" 1
	
TOKENEXTRA_SZ EQU #8
@ 0 "" 2
@ 61 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_extra_bit_struct_sz EQU #16
@ 0 "" 2
@ 63 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_token_value EQU #0
@ 0 "" 2
@ 64 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_token_len EQU #4
@ 0 "" 2
@ 66 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_extra_bit_struct_tree EQU #0
@ 0 "" 2
@ 67 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_extra_bit_struct_prob EQU #4
@ 0 "" 2
@ 68 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_extra_bit_struct_len EQU #8
@ 0 "" 2
@ 69 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_extra_bit_struct_base_val EQU #12
@ 0 "" 2
@ 71 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_comp_tplist EQU #163520
@ 0 "" 2
@ 72 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_comp_common EQU #103760
@ 0 "" 2
@ 73 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_comp_bc EQU #110096
@ 0 "" 2
@ 74 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_writer_sz EQU #40
@ 0 "" 2
@ 76 "vp8/encoder/asm_enc_offsets.c" 1
	
tokenlist_start EQU #0
@ 0 "" 2
@ 77 "vp8/encoder/asm_enc_offsets.c" 1
	
tokenlist_stop EQU #4
@ 0 "" 2
@ 78 "vp8/encoder/asm_enc_offsets.c" 1
	
TOKENLIST_SZ EQU #8
@ 0 "" 2
@ 80 "vp8/encoder/asm_enc_offsets.c" 1
	
vp8_common_mb_rows EQU #2392
@ 0 "" 2
	mov	r0, #0
	bx	lr
	.size	main, .-main
	.ident	"GCC: (GNU) 4.4.3"
	.section	.note.GNU-stack,"",%progbits
