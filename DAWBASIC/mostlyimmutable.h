#ifndef HAS_DAW_MOSTLYIMMUTABLE_HEADER
#define HAS_DAW_MOSTLYIMMUTABLE_HEADER

#include <algorithm>

namespace daw {
	//////////////////////////////////////////////////////////////////////////
	/// Summary: Defines a template that allows easy reading of a value but one
	/// must use the accessor method write_value to modify the value.  This should
	/// simulate const but allow writes and catch some errors.
	template<typename ValueType>
	class MostlyImmutable {
	public:
		MostlyImmutable( ): m_value{ } { }
		MostlyImmutable( ValueType value ): m_value{ std::move( value ) } { }
		MostlyImmutable( const MostlyImmutable& ) = default;
		MostlyImmutable( const MostlyImmutable&& value ): m_value( std::move( value.m_value ) ) { }
		~MostlyImmutable( ) = default;

		MostlyImmutable& operator=(MostlyImmutable rhs) {
			m_value = std::move( rhs.m_value );
			return *this;
		}

		bool operator==(const MostlyImmutable& rhs) {
			return rhs.m_value == m_value;
		}

		operator const ValueType&() const {
			return m_value;
		}

		ValueType& write( ) {
			return m_value;
		}

		void write( ValueType value ) {
			m_value = std::move( value );
		}

		const ValueType& read( ) const {
			return m_value;
		}

		ValueType copy( ) {
			return m_value;
		}

	private:
		ValueType m_value;
	};

	template<typename ValueType>
	std::ostream& operator<<(std::ostream& os, const MostlyImmutable<ValueType> &value) {
		os << value.read( );
		return os;
	}
}

#endif	//HAS_DAW_MOSTLYIMMUTABLE_HEADER

