#pragma once

#include "c_swap.hpp"
#include "c_writer.hpp"

struct slot_t {
	size_t m_offset = {};
	c_swap* m_swap = {};
	void* m_detour = {};
};

int install_jni( JNIEnv* env );
