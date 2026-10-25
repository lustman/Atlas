#pragma once

#include "c_swap.hpp"
#include "c_writer.hpp"

struct id_parts_t {
	std::string m_cls = {};
	std::string m_java = {};
	std::string_view m_nm = {};
	std::string_view m_sg = {};
};

int install_jni( JNIEnv* env );
