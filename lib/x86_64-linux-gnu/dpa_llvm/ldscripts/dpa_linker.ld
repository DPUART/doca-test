/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

OUTPUT_ARCH( "riscv" )

ENTRY(__text_start)

MEMORY
{
	code (rx) : ORIGIN =  0x0000000040000000, LENGTH = 128M
	sdata (rw) : ORIGIN = 0x0000000080000000, LENGTH = 128M
}

SECTIONS {
		.text : {
			__text_start = .;
			_text_start = .;
			text_start = .;
			*(.text.init)
			*(.text.irq)
			*(.text.idle)
			*(.text)
			*(.text.*)
			KEEP(*(.entry_point))
			*(.gnu.linkonce.t.*)
			__text_end = .;
			_text_end = .;
			text_end = .;
		} > code

		.plt     : ALIGN(8) { *( .plt) } > code
		.rodata  : { *(.rodata .rodata.* .gnu.linkonce.r.*) } > code
		.rodata1 : { *(.rodata1) } > code

		.dynsym        : ALIGN(8) { *( .dynsym        )} > code
		.gnu.version   : { *( .gnu.version   )} > code
		.gnu.version_r : { *( .gnu.version_r )} > code
		.gnu.hash      : { *( .gnu.hash      )} > code
		.hash          : { *( .hash          )} > code
		.dynstr        : { *( .dynstr        )} > code
		.rela.plt      : { *( .rela.plt      )} > code

		.code_end : ALIGN(8) {
			code_end = .;
		} > code

		.got.plt : ALIGN(8) {
			__got_plt_start = .;
			*(.got.plt)
			__got_plt_end = .;
		} > sdata

		.got : ALIGN(8) {
			__global_pointer$ = .;
			__got_start = .;
			*(.got)
			__got_end = .;
		} > sdata

		.data : {
			__DATA_BEGIN__ = .;
			*(.data.init_stage)
			*(.data .data.* .gnu.linkonce.d.*)
			SORT(CONSTRUCTORS)
		} > sdata

		.sdata : {
			__SDATA_BEGIN__ = .;
			*(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
			*(.sdata .sdata.* .gnu.linkonce.s.*)
		} > sdata

		.sbss : ALIGN(8) {
			__bss_start = .;
			*(.dynsbss)
			*(.sbss .sbss.* .gnu.linkonce.sb.*)
			*(.scommon)
		} > sdata

		.bss : ALIGN(8) {
			*(.dynbss)
			*(.bss .bss.* .gnu.linkonce.b.*)
			*(COMMON)
			__bss_end = .;
		} > sdata

		/DISCARD/ :
		{
			*(.dpa_lib_name)
		}

}

