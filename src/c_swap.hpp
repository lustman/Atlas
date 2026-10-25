#pragma once

#include "common.hpp"

class c_swap {
public:
	c_swap( ) = default;
	c_swap( const c_swap& ) = delete;
	c_swap& operator=( const c_swap& ) = delete;
	~c_swap( ) { this->destroy( ); }

	bool create( void** slot, void* detour ) {
		if ( !slot || !detour || !*slot )
			return false;

		if ( this->m_created ) {
			if ( *slot == this->m_detour )
				return true;

			if ( *slot != this->m_original )
				return false;

			return this->patch( slot, this->m_detour );
		}

		this->m_slot = slot;
		this->m_detour = detour;
		this->m_original = *slot;
		this->m_created = true;

		return true;
	}

	bool enable( ) {
		if ( !this->m_created )
			return false;

		if ( this->m_enabled )
			return true;

		if ( !this->patch( this->m_slot, this->m_detour ) )
			return false;

		this->m_enabled = true;

		return true;
	}

	bool disable( ) {
		if ( !this->m_created || !this->m_enabled )
			return true;

		if ( !this->patch( this->m_slot, this->m_original ) )
			return false;

		this->m_enabled = false;

		return true;
	}

	template< typename T >
	T original( ) const { return reinterpret_cast< T >( this->m_original ); }

	template< typename T, typename... args_t >
	auto invoke( args_t&&... args ) const {
		auto* fn = this->original< T >( );
		if ( !fn ) {
			if constexpr ( std::is_void_v< std::invoke_result_t< T, args_t... > > )
				return;
			else
				return std::invoke_result_t< T, args_t... >{ };
		}

		return fn( std::forward< args_t >( args )... );
	}

	template< auto Member, typename... args_t >
	auto invoke( args_t&&... args ) const {
		using fn_t = std::remove_reference_t< decltype( ( std::declval< JNINativeInterface_& >( ).*Member ) ) >;
		return this->invoke< fn_t >( std::forward< args_t >( args )... );
	}

private:
	bool patch( void** slot, void* value ) {
		uint32_t old = {};
		if ( !::VirtualProtect( slot, sizeof( void* ), PAGE_EXECUTE_READWRITE, reinterpret_cast< unsigned long* >( &old ) ) )
			return false;

		*slot = value;
		::VirtualProtect( slot, sizeof( void* ), old, reinterpret_cast< unsigned long* >( &old ) );

		return true;
	}

	void destroy( ) {
		if ( !this->m_created )
			return;

		this->disable( );
		this->m_slot = {};
		this->m_detour = {};
		this->m_original = {};
		this->m_created = {};
		this->m_enabled = {};
	}

	void** m_slot = {};
	void* m_detour = {};
	void* m_original = {};
	bool m_created = {};
	bool m_enabled = {};
};
