#pragma once

#include "common.hpp"

#include <intrin.h>

struct list_entry_t {
	list_entry_t* m_flink = {};
	list_entry_t* m_blink = {};
};

struct unicode_string_t {
	uint16_t m_length = {};
	uint16_t m_maximum_length = {};
	uint32_t m_pad = {};
	uint16_t* m_buffer = {};
};

struct ldr_entry_t {
	list_entry_t m_in_load_order = {};
	list_entry_t m_in_memory_order = {};
	list_entry_t m_in_init_order = {};
	void* m_dll_base = {};
	void* m_entry_point = {};
	uint32_t m_size_of_image = {};
	uint32_t m_pad = {};
	unicode_string_t m_full_name = {};
	unicode_string_t m_base_name = {};
};

struct peb_ldr_t {
	uint32_t m_length = {};
	uint8_t m_initialized = {};
	uint8_t m_pad[ 3 ] = {};
	void* m_ss_handle = {};
	list_entry_t m_in_load_order = {};
};

struct peb_t {
	uint8_t m_pad[ 0x18 ] = {};
	peb_ldr_t* m_ldr = {};
};

peb_t* peb( );
ldr_entry_t* main_module( );
void* set_caller( void* from );

class c_caller {
public:
	void* m_prev = {};

	explicit c_caller( void* from ) { this->m_prev = set_caller( from ); }
	~c_caller( ) { set_caller( this->m_prev ); }

	c_caller( const c_caller& ) = delete;
	c_caller& operator=( const c_caller& ) = delete;
};
