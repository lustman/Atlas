#pragma once

#include "common.hpp"

class c_swap {
public:
	c_swap( ) = default;
	c_swap( const c_swap& ) = delete;
	c_swap& operator=( const c_swap& ) = delete;
	~c_swap( ) { destroy( ); }

	bool create( void** slot, void* detour ) {
		if ( !slot || !detour || !*slot )
			return false;

		if ( m_created ) {
			if ( *slot == m_detour )
				return true;

			if ( *slot != m_original )
				return false;

			return patch( slot, m_detour );
		}

		m_slot = slot;
		m_detour = detour;
		m_original = *slot;
		m_created = true;

		return true;
	}

	bool enable( ) {
		if ( !m_created )
			return false;

		if ( m_enabled )
			return true;

		if ( !patch( m_slot, m_detour ) )
			return false;

		m_enabled = true;

		return true;
	}

	bool disable( ) {
		if ( !m_created || !m_enabled )
			return true;

		if ( !patch( m_slot, m_original ) )
			return false;

		m_enabled = false;

		return true;
	}

	template< typename T >
	T original( ) const { return reinterpret_cast< T >( m_original ); }

	template< typename T, typename... args_t >
	auto invoke( args_t&&... args ) const {
		auto* fn = original< T >( );
		using ret_t = std::invoke_result_t< T, args_t... >;
		if ( !fn ) {
			if constexpr ( std::is_void_v< ret_t > )
				return;
			else
				return ret_t{ };
		}

		return fn( std::forward< args_t >( args )... );
	}

	template< auto Member, typename... args_t >
	auto invoke( args_t&&... args ) const {
		using fn_t = std::remove_reference_t< decltype( ( std::declval< JNINativeInterface_& >( ).*Member ) ) >;
		return invoke< fn_t >( std::forward< args_t >( args )... );
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
		if ( !m_created )
			return;

		disable( );
		m_slot = {};
		m_detour = {};
		m_original = {};
		m_created = {};
		m_enabled = {};
	}

	void** m_slot = {};
	void* m_detour = {};
	void* m_original = {};
	bool m_created = {};
	bool m_enabled = {};
};
