#include "ldr.hpp"

peb_t* peb( ) {
	return reinterpret_cast< peb_t* >( __readgsqword( 0x60 ) );
}

ldr_entry_t* main_module( ) {
	auto* root = peb( );
	if ( !root || !root->m_ldr || !root->m_ldr->m_in_load_order.m_flink )
		return nullptr;

	return reinterpret_cast< ldr_entry_t* >( root->m_ldr->m_in_load_order.m_flink );
}
