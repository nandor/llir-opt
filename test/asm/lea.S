	.code
caml_c_call_i64_i64$unix_getnameinfo:
	.visibility	extern
	.call	caml_ext
	.stack	5328
	.args	0, i64, i64
.LBBentry:
  mov.i64	$0, $ret_addr
	mov.i64	$1, caml_last_return_address
	st.8	$1, $0
	mov.i64	$3, $frame_addr
	mov.i64	$4, caml_bottom_of_stack
	st.8	$4, $3
	arg.i64	$6, 0
	arg.i64	$7, 1
	mov.i64	$8, caml_local_roots
	ld.8.i64	$9, $8
	mov.i64	$10, 1
	frame.i64	$11, 5320
	st.8	$11, $10
	frame.i64	$13, 5312
	st.8	$13, $10
	frame.i64	$15, 5304
	st.8	$15, $10
	frame.i64	$17, 5256
	st.8	$17, $10
	mov.i64	$19, 8
	frame.i64	$20, 5248
	mov.i64	$21, 3
	st.8	$20, $21
	frame.i64	$23, 5240
	st.8	$8, $23
	frame.i64	$25, 5240
	st.8	$25, $9
	frame.i64	$27, 5264
	frame.i64	$28, 5320
	st.8	$27, $28
	frame.i64	$30, 5272
	frame.i64	$31, 5312
	st.8	$30, $31
	frame.i64	$33, 5280
	frame.i64	$34, 5304
	st.8	$33, $34
	mov.i64	$36, get_sockaddr
	frame.i64	$37, 5128
	frame.i64	$38, 5124
	call.3.c	$36, $6, $37, $38
	mov.i64	$40, getnameinfo_flag_table$164
	mov.i64	$41, caml_convert_flag_list
	call.2.c.i32	$42, $41, $7, $40
	mov.i64	$43, caml_enter_blocking_section
	call.0.c	$43
	mov.i32	$45, 1024
	mov.i32	$46, 4096
	mov.i64	$47, getnameinfo
	frame.i64	$48, 5124
	ld.4.i32	$49, $48
	frame.i64	$50, 5128
	frame.i64	$51, 1024
	frame.i64	$52, 0
	call.7.c.i32	$53, $47, $50, $49, $51, $46, $52, $45, $42
	mov.i64	$54, caml_leave_blocking_section
	call.0.c	$54
	jcc	$53, .LBB0_1$164$caml_c_call_i64_i64$unix_getnameinfo$unix_getnameinfo, .LBBterm3$caml_c_call_i64_i64$unix_getnameinfo$unix_getnameinfo
.LBBterm3$caml_c_call_i64_i64$unix_getnameinfo$unix_getnameinfo:
	mov.i64	$57, caml_copy_string
	frame.i64	$58, 1024
	call.1.c.i64	$59, $57, $58
	frame.i64	$60, 5320
	st.8	$60, $59
	frame.i64	$62, 0
	call.1.c.i64	$63, $57, $62
	frame.i64	$64, 5312
	st.8	$64, $63
	mov.i32	$66, 0
	mov.i64	$67, 2
	mov.i64	$68, caml_alloc_small
	call.2.c.i64	$69, $68, $67, $66
	frame.i64	$70, 5304
	st.8	$70, $69
	frame.i64	$72, 5320
	ld.8.i64	$73, $72
	st.8	$69, $73
	frame.i64	$75, 5304
	ld.8.i64	$76, $75
	add.i64	$77, $76, $19
	frame.i64	$78, 5312
	ld.8.i64	$79, $78
	st.8	$77, $79
	st.8	$8, $9
	frame.i64	$82, 5304
	ld.8.i64	$83, $82
	ret	$83
.LBB0_1$164$caml_c_call_i64_i64$unix_getnameinfo$unix_getnameinfo:
	mov.i64	$85, caml_raise_not_found
	call.0.c	$85
	trap	


	.extern	caml_bottom_of_stack
	.extern	caml_last_return_address
	.extern	caml_local_roots
	.extern	caml_sockaddr
	.extern	caml_enter_blocking_section
	.extern	caml_leave_blocking_section
	.extern	caml_alloc_small
	.extern	caml_raise_not_found
	.extern	caml_convert_flag_list
	.extern	getnameinfo_flag_table$164
	.extern	get_sockaddr
	.extern	getnameinfo
	.extern	caml_copy_string
