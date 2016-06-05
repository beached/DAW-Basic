// The MIT License (MIT)
// 
// Copyright (c) 2016 Darrell Wright
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#pragma once

#include <algorithm>

namespace daw {
	//////////////////////////////////////////////////////////////////////////
	/// Summary: Defines a template that allows easy reading of a value but one
	/// must use the accessor method write_value to modify the value.  This should
	/// simulate const but allow writes and catch some errors.
	template<typename To>
	class MostlyImmutable {
		To m_value;
	public:
		MostlyImmutable( ) = default;
		~MostlyImmutable( ) = default;
		MostlyImmutable( MostlyImmutable const & ) = default;
		MostlyImmutable( MostlyImmutable && ) = default;
		MostlyImmutable( To value ): m_value( std::move( value ) ) { }
		MostlyImmutable & operator=( MostlyImmutable const & ) = default;
		MostlyImmutable & operator=( MostlyImmutable && ) = default;

		bool operator==( MostlyImmutable const & rhs) {
			return rhs.m_value == m_value;
		}

		operator To const &( ) const {
			return m_value;
		}

		To & write( ) {
			return m_value;
		}

		void write( To value ) {
			m_value = std::move( value );
		}

		To const & read( ) const {
			return m_value;
		}

		To copy( ) {
			return m_value;
		}
	};	// class MostlyImmutable

	template<typename To>
	std::ostream& operator<<( std::ostream & os, MostlyImmutable<To> const & value ) {
		os << value.read( );
		return os;
	}
}

