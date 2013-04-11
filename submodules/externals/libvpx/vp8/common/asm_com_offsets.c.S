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
	.file	"asm_com_offsets.c"
	.text
	.align	2
	.global	main
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
#APP
@ 21 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_y_width EQU #0
@ 0 "" 2
@ 22 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_y_height EQU #4
@ 0 "" 2
@ 23 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_y_stride EQU #8
@ 0 "" 2
@ 24 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_uv_width EQU #12
@ 0 "" 2
@ 25 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_uv_height EQU #16
@ 0 "" 2
@ 26 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_uv_stride EQU #20
@ 0 "" 2
@ 27 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_y_buffer EQU #24
@ 0 "" 2
@ 28 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_u_buffer EQU #28
@ 0 "" 2
@ 29 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_v_buffer EQU #32
@ 0 "" 2
@ 30 "vp8/common/asm_com_offsets.c" 1
	
yv12_buffer_config_border EQU #40
@ 0 "" 2
@ 31 "vp8/common/asm_com_offsets.c" 1
	
VP8BORDERINPIXELS_VAL EQU #32
@ 0 "" 2
	mov	r0, #0
	bx	lr
	.size	main, .-main
	.ident	"GCC: (GNU) 4.4.3"
	.section	.note.GNU-stack,"",%progbits
