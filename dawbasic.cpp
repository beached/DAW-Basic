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

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/any.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/utility/string_ref.hpp>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <locale>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "dawbasic.h"

namespace {
	std::string operator+( boost::string_ref lhs, boost::string_ref rhs ) {
		std::string result( lhs.size( ) + rhs.size( ), '\0' );
		auto out_it = std::copy( lhs.begin( ), lhs.end( ), result.begin( ) );
		std::copy( rhs.begin( ), rhs.end( ), out_it );
		return result;
	}
		
	template<typename Iterator, typename UnaryPredicate>
	auto find_first_of( Iterator first, Iterator last, UnaryPredicate pred ) {
		while( first != last ) {
			if( pred( *first ) ) {
				break;
			}
			++first;
		}
		return first;
	}

	boost::string_ref trim_left( boost::string_ref value ) {
		if( value.empty( ) ) {
			return value;
		}
		auto first = find_first_of( value.cbegin( ), value.cend( ), []( auto c ) {
			return !std::isspace( c );
		} );
		auto pos = std::distance( value.cbegin( ), first );
		auto result = value.substr( pos );
		return result;
	}

	boost::string_ref trim_right( boost::string_ref value ) {
		if( value.empty( ) ) {
			return value;
		}
		auto last = find_first_of( value.crbegin( ), value.crend( ), []( auto c ) {
			return !std::isspace( c );
		} );
		auto dist = std::distance( value.crbegin( ), last );
		auto len = value.size( ) - dist;
		auto result = value.substr( 0, len );
		return result;
	}

	boost::string_ref trim( boost::string_ref value ) {
		if( value.empty( ) ) {
			return value;
		}
		auto result = trim_right( trim_left( value ) );
		return result;
	}

	std::string to_upper( boost::string_ref str ) {
		std::string result( str.size( ), '\0' );
		std::transform( str.begin( ), str.end( ), result.begin( ), []( auto c ) {
			return c & 0b11011111;
		} );
		return result;
	}

	template<typename F>
	bool almost_equal( F a, F b ) {
		return std::nextafter(a, std::numeric_limits<F>::lowest()) <= b && std::nextafter(a, std::numeric_limits<F>::max()) >= b;
	}

	template <typename To, typename From>
	constexpr bool can_fit( From value ) {
		return (std::numeric_limits<To>::min( ) <= static_cast<intmax_t>(std::numeric_limits<From>::min( )) ||
				 value >= static_cast<From>(std::numeric_limits<To>::min( ))) &&
				 (std::numeric_limits<To>::max( ) >= static_cast<uintmax_t>(std::numeric_limits<From>::max( )) || 
				   value <= static_cast<From>(std::numeric_limits<To>::max( )));
	}
}	// namespace anonymous

namespace daw {
	namespace exception {

		void syntax_errror_on_false( bool test, std::string msg, daw::basic::ErrorTypes error_type = daw::basic::ErrorTypes::SYNTAX ) {
			if( !test ) {
				throw daw::basic::BasicException( msg, error_type );
			}
		}

		void syntax_errror_on_false( bool test, const char* msg, daw::basic::ErrorTypes error_type = daw::basic::ErrorTypes::SYNTAX ) {
			if( !test ) {
				throw daw::basic::BasicException( msg, error_type );
			}
		}
	}

	namespace basic {
		BasicException::~BasicException( ) { }

		BasicException::BasicException( std::string const & msg, ErrorTypes errorType ): runtime_error( msg ) { }
		BasicException::BasicException( char const * msg, ErrorTypes errorType ): runtime_error( msg ) { }

		using std::placeholders::_1;

		namespace {

			bool is_integer( BasicValue const & value ) {
				return ValueType::INTEGER == value.first;
			}

			bool is_real( BasicValue const & value ) {
				return ValueType::REAL == value.first;
			}

			bool is_numeric( BasicValue const & value ) {
				return is_integer( value ) || is_real( value );
			}

			BasicException create_basic_exception( ErrorTypes error_type, std::string msg ) {
				switch( error_type ) {
				case ErrorTypes::SYNTAX:
					msg = "SYNTAX ERROR: " + msg;
					return BasicException( std::move( msg ), std::move( error_type ) );
				case ErrorTypes::FATAL:
					msg = "FATAL ERROR: " + msg;
					return BasicException( std::move( msg ), std::move( error_type ) );
				}
				throw std::runtime_error( "Unknown error type tried to be thrown" );
			}

			integer operator_rank( boost::string_ref oper ) {
				if( "NEG" == oper ) {
					return 1;
				} else if( "^" == oper ) {
					return 2;
				} else if( "*" == oper || "/" == oper ) {
					return 3;
				} else if( "+" == oper || "-" == oper || "%" == oper ) {
					return 4;
				} else if( ">>" == oper || "<<" == oper ) {
					return 5;
				} else if( ">" == oper || ">=" == oper || "<" == oper || "<=" == oper ) {
					return 6;
				} else if( "=" == oper ) {
					return 7;
				} else if( "AND" == oper ) {
					return 8;
				} else if( "OR" == oper ) {
					return 9;
				}
				throw create_basic_exception( ErrorTypes::FATAL, "Unknown operator passed to operator_rank" );
			}

			BasicValue const & EMPTY_BASIC_VALUE( ) {
				static BasicValue const result { ValueType::EMPTY, boost::any( ) };
				return result;
			}

			ValueType get_value_type( BasicValue value ) {
				return value.first;
			}

			ValueType get_value_type( boost::string_ref value, std::string locale_str = "", bool trim_ws = true ) {
				// DAW erase
				if( trim_ws ) {
					value = trim( value );
				}
				if( value.empty( ) ) {
					return ValueType::EMPTY;
				}

				using charT = char;
				// Can only set locale once per application start.  It was slow
				static const auto loc = std::locale( locale_str );
				static const charT decimal_point = std::use_facet< std::numpunct<charT>>( loc ).decimal_point( );

				auto has_decimal = false;

				size_t startpos = 0;

				if( '-' == value[startpos] ) {
					startpos = 1;
				}

				for( size_t n = startpos; n < value.size( ); ++n ) {
					if( '-' == value[n] ) {	// We have already had a - or it is not the first
						return ValueType::STRING;
					} else if( decimal_point == value[n] ) {
						if( has_decimal || value.size( ) == n + 1 ) {	// more than one decimal point or we are the last entry and there is not a possibility of another number
							return ValueType::STRING;
						}
						has_decimal = true;
					} else if( !std::isdigit( value[n], loc ) ) {	// Of course, not a numeral
						return ValueType::STRING;
					}
					// All other items are numbers and we keep checking
				}
				if( has_decimal ) {
					return ValueType::REAL;
				}
				return ValueType::INTEGER;
			}

			std::vector<boost::string_ref> split_in_two_on_char( boost::string_ref parse_string, char separator ) {
				parse_string = trim( parse_string );
				const auto pos = parse_string.find_first_of( separator );
				std::vector<boost::string_ref> result;
				if( boost::string_ref::npos != pos ) {
					result.push_back( trim( parse_string.substr( 0, pos ) ) );
					result.push_back( trim( parse_string.substr( pos + 1 ) ) );
				} else {
					result.push_back( parse_string );
				}
				return result;
			}

			integer to_integer( BasicValue value ) {
				return boost::any_cast<integer>(value.second);
			}

			integer to_integer( boost::string_ref value ) {
				return boost::lexical_cast<integer>(value);
			}

			real to_real( boost::string_ref value ) {
				return boost::lexical_cast<real>(value);
			}

			real to_real( BasicValue value ) {
				return boost::any_cast<real>(value.second);
			}

			real to_numeric( BasicValue value ) {
				switch( value.first ) {
				case ValueType::INTEGER:
					return static_cast<real>(to_integer( value ));
				case ValueType::REAL:
					return boost::any_cast<real>(value.second);
				case ValueType::ARRAY:
				case ValueType::BOOLEAN:
				case ValueType::EMPTY:
				case ValueType::STRING:
					throw create_basic_exception( ErrorTypes::FATAL, "Cannot convert non-numeric types to a number" );
				default:
					throw std::exception { };
				}
			}

			boolean to_boolean( BasicValue value ) {
				if( ValueType::BOOLEAN == value.first ) {
					return boost::any_cast<boolean>(value.second);
				}
				throw create_basic_exception( ErrorTypes::FATAL, "Attempt to convert a non-boolean to a boolean" );
			}

			BasicValue basic_value_integer( integer value ) {
				return BasicValue { ValueType::INTEGER, boost::any( std::move( value ) ) };
			}

			BasicValue basic_value_integer( boost::string_ref value ) {
				return basic_value_integer( to_integer( value ) );
			}

			BasicValue basic_value_real( real value ) {
				return BasicValue { ValueType::REAL, boost::any( std::move( value ) ) };
			}

			BasicValue basic_value_real( boost::string_ref value ) {
				return basic_value_real( to_real( value ) );
			}

			BasicValue basic_value_numeric( boost::string_ref value ) {
				auto value_type = get_value_type( value );
				switch( value_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( value );
				case ValueType::REAL:
					return basic_value_real( value );
				case ValueType::ARRAY:
				case ValueType::BOOLEAN:
				case ValueType::EMPTY:
				case ValueType::STRING:
					throw create_basic_exception( ErrorTypes::FATAL, "Attempt to create a numeric BasicValue from a non-numeric string" );
				default:
					throw std::exception { };
				}
			}

			BasicValue basic_value_boolean( boolean value ) {
				return BasicValue { ValueType::BOOLEAN, boost::any( std::move( value ) ) };
			}

			BasicValue basic_value_string( boost::string_ref value ) {
				return BasicValue { ValueType::STRING, boost::any( value.to_string( ) ) };
			}

			/*
			std::string to_string( integer value ) {
				std::stringstream ss;
				ss << value;
				return ss.str( );
			}
			*/

			std::string to_string( BasicValue value ) {
				std::stringstream ss;
				switch( value.first ) {
				case ValueType::EMPTY:
					break;
				case ValueType::INTEGER:
					ss << to_integer( std::move( value ) );
					break;
				case ValueType::REAL:
					ss << std::setprecision( std::numeric_limits<real>::digits10 ) << to_real( std::move( value ) );
					break;
				case ValueType::STRING:
					ss << boost::any_cast<std::string>(value.second);
					break;
				case ValueType::BOOLEAN:
					if( to_boolean( value ) ) {
						ss << "TRUE";
					} else {
						ss << "FALSE";
					}
					break;
				case ValueType::ARRAY:
					break;
				default:
					throw create_basic_exception( ErrorTypes::FATAL, "Unknown ValueType" );
				}
				return ss.str( );
			}

			template<typename M>
			auto get_keys( const M& m )  -> std::vector < typename M::key_type > {
				std::vector<typename M::key_type> result( m.size( ) );
				std::transform( m.cbegin( ), m.cend( ), std::back_inserter( result ), std::bind( &M::value_type::first, _1 ) );

				// 				for( auto it = m.cbegin( ); it != m.cend( ); ++it ) {
				// 					result.push_back( it->first );
				// 				}
				return result;
			}

			std::string value_type_to_string( ValueType value_type ) {
				switch( value_type ) {
				case ValueType::BOOLEAN:
					return "Boolean";
				case ValueType::EMPTY:
					return "Empty";
				case ValueType::INTEGER:
					return "Integer";
				case ValueType::REAL:
					return "Real";
				case ValueType::ARRAY:
					return "Array";
				case ValueType::STRING:
					return "String";
				}
				throw create_basic_exception( ErrorTypes::FATAL, "Unknown ValueType" );
			}

			std::string value_type_to_string( BasicValue value ) {
				return value_type_to_string( std::move( value.first ) );
			}

			ValueType determine_result_type( ValueType lhs_type, ValueType rhs_type ) {
				if( ValueType::INTEGER == lhs_type ) {
					switch( rhs_type ) {
					case daw::basic::ValueType::STRING:
						return ValueType::STRING;
					case daw::basic::ValueType::INTEGER:
						return ValueType::INTEGER;
					case daw::basic::ValueType::REAL:
						return ValueType::REAL;
					case daw::basic::ValueType::ARRAY:
					case daw::basic::ValueType::BOOLEAN:
					case daw::basic::ValueType::EMPTY:
						break;
					default:
						throw std::exception { };
					}
				} else if( ValueType::REAL == lhs_type ) {
					switch( rhs_type ) {
					case ValueType::INTEGER:
					case ValueType::REAL:
						return ValueType::REAL;
					case ValueType::STRING:
						return ValueType::STRING;
					case daw::basic::ValueType::ARRAY:
					case daw::basic::ValueType::BOOLEAN:
					case daw::basic::ValueType::EMPTY:
						break;
					default:
						throw std::exception { };
					}
				} else if( ValueType::STRING == lhs_type ) {
					switch( rhs_type ) {
					case ValueType::INTEGER:
					case ValueType::REAL:
					case ValueType::STRING:
						return ValueType::STRING;
					case daw::basic::ValueType::ARRAY:
					case daw::basic::ValueType::BOOLEAN:
					case daw::basic::ValueType::EMPTY:
						break;
					default:
						throw std::exception { };
					}
				} else if( ValueType::BOOLEAN == lhs_type ) {
					switch( rhs_type ) {
					case daw::basic::ValueType::BOOLEAN:
						return ValueType::BOOLEAN;
					case daw::basic::ValueType::ARRAY:
					case daw::basic::ValueType::INTEGER:
					case daw::basic::ValueType::REAL:
					case daw::basic::ValueType::STRING:
					case daw::basic::ValueType::EMPTY:
						break;
					default:
						throw std::exception { };
					}
				}
				return ValueType::EMPTY;
			}

			template<typename M>
			bool key_exists( M const & kv_map, boost::string_ref key ) {
				return kv_map.cend( ) != kv_map.find( to_upper( key ) );
			}

			template<typename M>
			auto & retrieve_value( M & kv_map, boost::string_ref key ) {
				return kv_map[to_upper( key )];
			}

			template<typename Container>
			auto pop( Container & vect ) {
				auto result = vect.back( );
				vect.pop_back( );
				return result;
			}

			//////////////////////////////////////////////////////////////////////////
			/// summary: Makes a value from a single token
			//////////////////////////////////////////////////////////////////////////
			/*
			BasicValue make_value( std::string value ) {
				boost::algorithm::trim( value );
				switch( get_value_type( value ) ) {
				case daw::basic::ValueType::EMPTY:
					return EMPTY_BASIC_VALUE( );
				case daw::basic::ValueType::STRING:
					return basic_value_string( std::move( value ) );
				case daw::basic::ValueType::INTEGER:
					return basic_value_integer( std::move( value ) );
				case daw::basic::ValueType::REAL:
					return basic_value_real( std::move( value ) );
				case daw::basic::ValueType::ARRAY:
				case daw::basic::ValueType::BOOLEAN:
					throw create_basic_exception( ErrorTypes::FATAL, "Unknown value type" );
				default:
					throw std::exception{ };
				}
			}
			*/

			size_t find_end_of_string( boost::string_ref value ) {
				size_t start = 0;
				if( '"' == value[start] ) {
					++start;
				}
				size_t quote_counter = 1;
				for( size_t pos = start; pos < value.size( ); ++pos ) {
					if( '"' == value[pos] ) {
						if( !(0 != pos && '\\' == value[pos - 1]) ) {
							--quote_counter;
						}
					}
					if( 0 >= quote_counter ) {
						return pos;
					}
				}
				throw create_basic_exception( ErrorTypes::SYNTAX, "Could not find end of quoted string, not closing quotes" );
			}

			size_t find_end_of_bracket( boost::string_ref value ) {
				intmax_t bracket_count = 1;
				size_t pos = 0;
				if( ')' == value[pos] ) {
					return pos;
				}
				++pos;
				for( ; pos < value.size( ); ++pos ) {
					if( '(' == value[pos] ) {
						++bracket_count;
					} else if( ')' == value[pos] ) {
						--bracket_count;
						if( 0 == bracket_count ) {
							return pos;
						}
					}
				}
				throw create_basic_exception( ErrorTypes::SYNTAX, "Unclosed bracket found" );
			}

			size_t find_end_of_operand( boost::string_ref value ) {
				assert( value.size( ) > 0 );

				const static std::vector<char> end_chars { ' ', '	', '^', '*', '/', '+', '-', '=', '<', '>', '%' };
				intmax_t bracket_count = 0;

				auto has_brackets = false;
				for( size_t pos = 0; pos < value.size( ); ++pos ) {
					auto const & current_char = value[pos];
					if( 0 >= bracket_count ) {
						if( '"' == current_char ) {
							throw create_basic_exception( ErrorTypes::SYNTAX, "Unexpected quote \" character at position " + std::to_string( pos ) );
						}
						if( ')' == current_char ) {
							throw create_basic_exception( ErrorTypes::SYNTAX, "Unexpected close bracket ) character at position " + std::to_string( pos ) );
						}
						if( std::end( end_chars ) != std::find( std::begin( end_chars ), std::end( end_chars ), current_char ) ) {
							return pos - 1;
						} else if( '(' == current_char ) {
							if( has_brackets ) {
								throw create_basic_exception( ErrorTypes::SYNTAX, "Unexpected opening bracket after brackets have closed at position " + std::to_string( pos ) );
							}
							++bracket_count;
							has_brackets = true;
						}
					} else {
						if( '"' == current_char ) {
							pos += find_end_of_string( value.substr( pos ) );
						} else if( ')' == current_char ) {
							--bracket_count;
						} else if( '(' == current_char ) {
							++bracket_count;
						}
					}
				}
				return value.size( ) - 1;
			}

			auto remove_outer_characters( boost::string_ref value, char lhs, char rhs ) {
				if( !value.empty( ) && 2 <= value.size( ) ) {
					if( lhs == value[0] && rhs == value[value.size( ) - 1] ) {
						value = value.substr( 1, value.size( ) - 2 );
					}
				}
				return value;
			}

			auto remove_outer_quotes( boost::string_ref value ) {
				return remove_outer_characters( value, '"', '"' );
			}

			auto remove_outer_bracket( boost::string_ref value ) {
				return remove_outer_characters( value, '(', ')' );
			}

			void replace_all( std::string & str, std::string const & from, std::string const & to ) {
				if( from.empty( ) ) {
					return;
				}
				size_t start_pos = 0;
				while( (start_pos = str.find( from, start_pos )) != std::string::npos ) {
					str.replace( start_pos, from.length( ), to );
					start_pos += to.length( ); // In case 'to' contains 'from', like replacing 'x' with 'yx'
				}
			}

			std::string char_to_string( char chr ) {
				std::string result( " " );
				result[0] = chr;
				return result;
			};

			size_t multiply_list( std::vector<size_t> const & dimensions ) {
				size_t result = 1;
				for( const auto& dimension : dimensions ) {
					result *= dimension;
				}

				return result;
			}

			std::vector<size_t> convert_dimensions( std::vector<BasicValue> dimensions ) {
				std::vector<size_t> index;
				for( const auto& value : dimensions ) {
					auto tmp = to_integer( value );
					assert( can_fit<size_t>( tmp ) );
					index.push_back( static_cast<size_t>( tmp ) );
				}
				return index;
			}

		}	// namespace anonymous

			//////////////////////////////////////////////////////////////////////////
			// Basic::BasicArray
			//////////////////////////////////////////////////////////////////////////
		Basic::BasicArray::BasicArray( ): m_dimensions( ), m_values( ) { }

		Basic::BasicArray::BasicArray( std::vector<size_t> dimensions ) : m_dimensions( dimensions ), m_values( multiply_list( dimensions ) ) { }

		Basic::BasicArray::BasicArray( BasicArray const & other ) : m_dimensions( other.m_dimensions ), m_values( other.m_values ) { }

		Basic::BasicArray::BasicArray( BasicArray && other ) : m_dimensions( std::move( other.m_dimensions ) ), m_values( std::move( other.m_values ) ) { }

		Basic::BasicArray& Basic::BasicArray::operator=( BasicArray other ) {
			m_dimensions = std::move( other.m_dimensions );
			m_values = std::move( other.m_values );
			return *this;
		}

		namespace {
			template<typename Iter1, typename Iter2, typename Predicate>
			bool are_equal( Iter1 it_start1, Iter1 it_end1, Iter2 it_start2, Iter2 it_end2, Predicate pred ) {
				while( it_start1 != it_end1 && it_start2 != it_end2 ) {
					if( !pred( *it_start1, *it_start2 ) ) {
						return false;
					}
					++it_start1;
					++it_start2;
				}
				return it_start1 == it_end1 && it_start2 == it_end2;
			}

			template<typename Iter1, typename Iter2>
			bool are_equal( Iter1 it_start1, Iter1 it_end1, Iter2 it_start2, Iter2 it_end2 ) {
				using ValueType = decltype(*it_start1);
				auto eq_compare = []( const ValueType& v1, const ValueType& v2 ) {
					return v1 == v2;
				};
				return are_equal( it_start1, it_end1, it_start2, it_end2, eq_compare );
			}

			template<typename ContainerType, typename Predicate>
			bool are_equal( const ContainerType& c1, const ContainerType& c2, Predicate pred ) {
				bool result = c1.size( ) == c2.size( );
				if( result ) {
					using std::begin;
					using std::end;
					result = are_equal( begin( c1 ), end( c1 ), begin( c2 ), end( c2 ), pred );
				}
				return result;
			}

			template<typename ContainerType>
			bool are_equal( ContainerType const & c1, ContainerType const & c2 ) {
				using ValueType = typename ContainerType::value_type;
				auto eq_compare = []( const ValueType& v1, const ValueType& v2 ) {
					return v1 == v2;
				};
				return are_equal( c1, c2, eq_compare );
			}
		}

		bool Basic::BasicArray::operator==( BasicArray const & rhs ) const {
			auto compare_function = []( basic::BasicValue const & v1, basic::BasicValue const & v2 ) {
				bool result = v1.first == v2.first;
				if( result ) {
					switch( v1.first ) {
					case ValueType::EMPTY:
						result &= true;
						break;
					case ValueType::BOOLEAN:
						result &= to_boolean( v1 ) == to_boolean( v2 );
						break;
					case ValueType::INTEGER:
						result &= to_integer( v1 ) == to_integer( v2 );
						break;
					case ValueType::REAL:
						result &= almost_equal( to_real( v1 ), to_real( v2 ) );
						break;
					case ValueType::STRING:
						result &= to_string( v1 ) == to_string( v2 );
						break;
					case ValueType::ARRAY:
						throw std::runtime_error( "Unimplemented feature" );
					default:
						throw std::exception { };
					}
				}
				return result;
			};

			return are_equal( m_dimensions, rhs.m_dimensions ) && are_equal( m_values, rhs.m_values, compare_function );
		}

		BasicValue & Basic::BasicArray::operator( )( std::vector<size_t> dimensions ) {
			if( m_dimensions.size( ) != dimensions.size( ) ) {
				std::stringstream ss;
				ss << "Must supply " << m_dimensions.size( ) << " indexes to address array";
				throw ::daw::basic::create_basic_exception( ErrorTypes::SYNTAX, ss.str( ) );
			}
			try {
				size_t multiplier = 1;
				size_t pos = 0;
				for( size_t n = 0; n < m_dimensions.size( ); ++n ) {
					pos += dimensions[n] * multiplier;
					multiplier *= m_dimensions[n];
				}
				return m_values.at( pos );
			} catch( std::out_of_range ex ) {
				std::stringstream ss;
				ss << "Array out of bounds.  Max is ( ";
				bool is_first = true;
				for( auto& dim : m_dimensions ) {
					if( !is_first ) {
						ss << ", ";
					} else {
						is_first = false;
					}
					ss << dim;
				}
				ss << " ) you requested ( ";
				for( auto& dim : dimensions ) {
					if( !is_first ) {
						ss << ", ";
					} else {
						is_first = false;
					}
					ss << dim;
				}
				ss << ")";
				throw ::daw::basic::create_basic_exception( ErrorTypes::SYNTAX, ss.str( ) );
			}
		}

		BasicValue const & Basic::BasicArray::operator( )( std::vector<size_t> dimensions ) const {
			if( m_dimensions.size( ) != dimensions.size( ) ) {
				std::stringstream ss;
				ss << "Must supply " << m_dimensions.size( ) << " indexes to address array";
				throw ::daw::basic::create_basic_exception( ErrorTypes::SYNTAX, ss.str( ) );
			}
			try {
				size_t multiplier = 1;
				size_t pos = 0;
				for( size_t n = 0; n < m_dimensions.size( ); ++n ) {
					pos += dimensions[n] * multiplier;
					multiplier *= m_dimensions[n];
				}
				return m_values.at( pos );
			} catch( std::out_of_range ex ) {
				std::stringstream ss;
				ss << "Array out of bounds.  Max is less than ( ";
				bool is_first = true;
				for( auto& dim : m_dimensions ) {
					if( !is_first ) {
						ss << ", ";
					} else {
						is_first = false;
					}
					ss << dim;
				}
				is_first = true;
				ss << " ) you requested ( ";
				for( auto& dim : dimensions ) {
					if( !is_first ) {
						ss << ", ";
					} else {
						is_first = false;
					}
					ss << dim;
				}
				ss << ")";
				throw ::daw::basic::create_basic_exception( ErrorTypes::SYNTAX, ss.str( ) );
			}
		}

		std::vector<size_t> Basic::BasicArray::dimensions( ) const {
			return m_dimensions;
		}

		//////////////////////////////////////////////////////////////////////////
		// Basic
		/////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		/// summary: Evaluate a string and solve all functions/variables
		BasicValue Basic::evaluate( boost::string_ref value ) {
			size_t current_position = 0;
			std::vector<BasicValue> operand_stack;
			std::vector<std::string> operator_stack;

			auto is_higher_precedence = [&operator_stack]( boost::string_ref oper ) {
				if( operator_stack.empty( ) ) {
					return true;
				}
				auto from_stack = *(operator_stack.rbegin( ));
				return operator_rank( oper ) < operator_rank( from_stack );
			};

			auto is_logical_and = []( boost::string_ref val, size_t pos, size_t end ) {
				std::string const check_text { "AND" };
				if( pos + check_text.size( ) <= end ) {
					auto const txt_window = to_upper( val.substr( pos, check_text.size( ) ) );
					if( check_text == txt_window ) {
						auto ws_check = val[pos + check_text.size( )];
						return ' ' == ws_check || '	' == ws_check;
					}
				}
				return false;
			};

			auto is_logical_or = []( boost::string_ref val, size_t pos, size_t end ) {
				std::string const check_text { "OR" };
				if( pos + check_text.size( ) <= end ) {
					auto txt_window = to_upper( val.substr( pos, check_text.size( ) ) );
					if( check_text == txt_window ) {
						auto ws_check = val[pos + check_text.size( )];
						return ' ' == ws_check || '	' == ws_check;
					}
				}
				return false;
			};

			auto const end = value.size( ) - 1;

			while( current_position <= end ) {
				auto current_char = value[current_position];
				if( 'a' == current_char ) {
					current_char = 'A';
				} else if( 'o' == current_char ) {
					current_char = 'O';
				}
				switch( current_char ) {
				case '"':
				{ // String boundary
					auto current_operand = value.substr( current_position ).to_string( );
					auto end_of_string = find_end_of_string( current_operand );
					current_operand = current_operand.substr( 0, end_of_string + 1 );
					current_operand = remove_outer_quotes( current_operand ).to_string( );
					replace_all( current_operand, "\\\"", "\"" );
					operand_stack.push_back( basic_value_string( remove_outer_quotes( current_operand ) ) );
					current_position += end_of_string;
				}
				break;
				case '(':	// Bracket boundary				
				{
					auto text_in_bracket = value.substr( current_position + 1 );
					auto end_of_bracket = find_end_of_bracket( text_in_bracket );
					text_in_bracket = text_in_bracket.substr( 0, end_of_bracket );
					auto evaluated_bracket = evaluate( std::move( text_in_bracket ) );
					operand_stack.push_back( evaluated_bracket );
					current_position += end_of_bracket + 1;
				}
				break;
				case ' ':
				case '	':
					while( ' ' == value[current_position + 1] || '	' == value[current_position + 1] ) {
						++current_position;
					}
					break;
				case 'A':
					if( is_logical_and( value, current_position, end ) ) {
						goto explicit_operator;
					} else {
						goto explicit_default;
					}
				case 'O':
					if( is_logical_or( value, current_position, end ) ) {
						goto explicit_operator;
					} else {
						goto explicit_default;
					}
				case '%':
				case '^':
				case '*':
				case '/':
				case '+':
				case '-':
				case '<':
				case '>':
				case '=':
				{
explicit_operator:
					std::string current_operator = char_to_string( current_char );
					// Negation of numbers					
					if( '-' == current_char ) {
						// Check for unary negation.
						if( operator_stack.empty( ) && operand_stack.empty( ) ) {
							current_operator = "NEG";
						} else if( !operator_stack.empty( ) && (1 == operand_stack.size( ) % 2) ) {
							current_operator = "NEG";
						}
					}
					if( '>' == current_char || '<' == current_char ) {
						if( current_position >= end ) {
							throw create_basic_exception( ErrorTypes::SYNTAX, "Binary operator with only left hand side, not right" );
						}
						if( '=' == value[current_position + 1] ) {
							current_operator += value[++current_position];
						}
					} else if( 'A' == current_char ) {
						current_operator = "AND";
						current_position += 2;
					} else if( 'O' == current_char ) {
						current_operator = "OR";
						current_position += 1;
					}
					// Used to be an if, changed to a while
					while( !is_higher_precedence( current_operator ) ) {
						// Pop from operand stack and do operators and push value back to stack
						BasicValue rhs( pop( operand_stack ) );
						auto prev_operator( pop( operator_stack ) );
						BasicValue result( EMPTY_BASIC_VALUE( ) );
						if( is_unary_operator( prev_operator ) ) {
							result = m_unary_operators[prev_operator]( std::move( rhs ) );
						} else if( is_binary_operator( prev_operator ) ) {
							result = m_binary_operators[prev_operator]( pop( operand_stack ), std::move( rhs ) );
						} else {
							throw create_basic_exception( ErrorTypes::SYNTAX, "Unknown operator " + prev_operator );	// Probably should never happen...but you know
						}
						operand_stack.push_back( result );
					}
					operator_stack.push_back( current_operator );
				}
				break;
				default:
				{
explicit_default:
					auto current_operand = value.substr( current_position );
					auto end_of_operand = find_end_of_operand( current_operand );
					current_operand = current_operand.substr( 0, end_of_operand + 1 );

					auto split_operand = split_arrayfunction_from_string( current_operand, false );


					if( 0 < split_operand.second.size( ) ) {	// We are a function, find parameters and push value to stack
						if( is_function( split_operand.first ) ) {
							operand_stack.push_back( exec_function( split_operand.first, split_operand.second ) );
						} else if( is_array( split_operand.first ) ) {
							operand_stack.emplace_back( get_array_variable( split_operand.first, split_operand.second ) );
						} else {
							throw create_basic_exception( ErrorTypes::SYNTAX, "Unknown symbol name '" + split_operand.first + "'" );
						}
					} else {
						if( is_variable( split_operand.first ) ) {
							// We are a variable, push value onto stack
							operand_stack.emplace_back( get_variable_constant( split_operand.first ) );
						} else {
							// We must be a number
							auto value_type = get_value_type( current_operand.to_string( ) );
							switch( value_type ) {
							case ValueType::INTEGER:
								operand_stack.push_back( basic_value_integer( std::move( current_operand ) ) );
								break;
							case ValueType::REAL:
								operand_stack.push_back( basic_value_real( std::move( current_operand ) ) );
								break;
							case ValueType::EMPTY:
							case ValueType::STRING:
							case ValueType::BOOLEAN:
							case ValueType::ARRAY:
								throw create_basic_exception( ErrorTypes::SYNTAX, "Unknown symbol '" + current_operand.to_string( ) + "'" );
							default:
								throw std::exception { };
							}
						}
					}
					current_position += end_of_operand;
				}
				}	// switch				
				++current_position;
			}	// while
				// finish stacks
			while( !operator_stack.empty( ) ) {
				auto current_operator = pop( operator_stack );
				auto rhs = pop( operand_stack );
				BasicValue result = EMPTY_BASIC_VALUE( );
				if( is_unary_operator( current_operator ) ) {
					result = m_unary_operators[current_operator]( std::move( rhs ) );
				} else if( is_binary_operator( current_operator ) ) {
					result = m_binary_operators[current_operator]( pop( operand_stack ), std::move( rhs ) );
				} else {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Unknown operator " + current_operator );	// Probably should never happen...but you know
				}
				operand_stack.push_back( result );
			}
			if( operand_stack.empty( ) ) {
				return EMPTY_BASIC_VALUE( );
			}
			auto current_operand = pop( operand_stack );
			if( !operand_stack.empty( ) ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Unknown error while parsing line.  Not value left at end of evaluation" );
			}
			return current_operand;
		}


		size_t Basic::BasicArray::total_items( ) const {
			return m_values.size( );
		}

		bool Basic::is_unary_operator( boost::string_ref oper ) {
			return key_exists( m_unary_operators, oper );
		}

		bool Basic::is_binary_operator( boost::string_ref oper ) {
			return key_exists( m_binary_operators, oper );
		}

		std::vector<BasicValue> Basic::evaluate_parameters( boost::string_ref value ) {
			value = remove_outer_bracket( value );
			//value = boost::algorithm::trim_copy( value );
			// Parameters are separated by comma's.  Comma's should only exist within quotes and between parameters
			std::vector<BasicValue> result;
			if( value.empty( ) ) {
				return result;
			}
			std::vector<size_t> comma_pos;


			auto const end = value.size( ) - 1;
			for( size_t pos = 0; pos <= end; ++pos ) {
				auto const & current_char = value[pos];
				switch( current_char ) {
				case '"':
					pos += find_end_of_string( value.substr( pos ) );
					break;
				case ',':
					comma_pos.push_back( pos );
					break;
				}
			}
			comma_pos.push_back( value.size( ) );
			size_t start = 0;
			for( auto current_end : comma_pos ) {
				auto current_param = value.substr( start, current_end - start );
				auto eval = evaluate( std::move( current_param ) );
				result.push_back( eval );
				start = current_end + 1;
			}
			return result;
		}

		BasicValue & Basic::get_variable_constant( boost::string_ref name ) {
			if( is_constant( name ) ) {
				return  m_constants[name.to_string( )].value;
			} else if( is_variable( name ) ) {
				return get_variable( name );
			}
			throw create_basic_exception( ErrorTypes::FATAL, "Undefined variable or constant" );
		}

		void Basic::add_variable( boost::string_ref name, BasicValue value ) {
			if( is_constant( name ) ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Cannot create a variable that is a system constant" );
			} else if( is_function( name ) | is_keyword( name ) ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Cannot create a variable with the same name as a system function/keyword" );
			}
			m_variables[name.to_string( )] = std::move( value );
		}

		void Basic::add_array_variable( boost::string_ref name, std::vector<BasicValue> dimensions ) {
			if( is_constant( name ) ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Cannot create a variable that is a system constant" );
			} else if( is_function( name ) | is_keyword( name ) ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Cannot create a variable with the same name as a system function/keyword" );
			}
			m_arrays[name.to_string( )] = BasicArray { convert_dimensions( std::move( dimensions ) ) };
		}

		void Basic::add_constant( boost::string_ref name, std::string description, BasicValue value ) {
			if( is_function( name ) | is_keyword( name ) ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Cannot create a constant with the same name as a system function/keyword" );
			}
			if( key_exists( m_variables, name ) ) {
				remove_variable( name );
			}
			m_constants[name.to_string( )] = ConstantType { std::move( description ), std::move( value ) };
		}

		bool Basic::is_variable( boost::string_ref name ) {
			return key_exists( m_variables, name ) || is_constant( name );
		}

		bool Basic::is_constant( boost::string_ref name ) {
			return key_exists( m_constants, name );
		}

		bool Basic::is_array( boost::string_ref name ) {
			return key_exists( m_arrays, name );
		}

		void Basic::remove_variable( boost::string_ref name, bool throw_on_nonexist ) {
			auto pos = m_variables.find( to_upper( name ) );
			if( m_variables.end( ) == pos && throw_on_nonexist ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to delete unknown variable" );
			}
			m_variables.erase( pos );
		}

		void Basic::remove_constant( boost::string_ref name, bool throw_on_nonexist ) {
			auto pos = m_constants.find( to_upper( name ) );
			if( m_constants.end( ) == pos && throw_on_nonexist ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to delete unknown constant" );
			}
			m_constants.erase( pos );
		}

		void Basic::add_function( boost::string_ref name, std::string description, BasicFunction func ) {
			if( "SIN" == name ) {
				std::cout << "";
			}
			if( is_keyword( name ) ) {
				throw create_basic_exception( ErrorTypes::FATAL, "Cannot create a function with the same name as a system keyword" );
			}
			m_functions[name.to_string( )] = FunctionType( std::move( description ), std::move( func ) );
		}

		ProgramType::iterator Basic::find_line( integer line_number ) {
			auto result = std::find_if( std::begin( m_program ), std::end( m_program ), [&line_number]( ProgramLine current_line ) {
				return current_line.first == line_number;
			} );
			return result;
		}

		void Basic::add_line( integer line_number, boost::string_ref line ) {
			auto pos = find_line( line_number );
			if( std::end( m_program ) == pos ) {
				m_program.push_back( make_pair( line_number, line ) );
			} else {
				pos->second = line;
			}
		}

		void Basic::remove_line( integer line_number ) {
			auto pos = find_line( line_number );
			if( m_program.end( ) != pos ) {
				m_program.erase( pos );
			}
		}

		bool Basic::is_keyword( boost::string_ref name ) {
			return key_exists( m_keywords, name );
		}

		bool Basic::is_function( boost::string_ref name ) {
			return key_exists( m_functions, name );
		}

		BasicValue & Basic::get_array_variable( boost::string_ref name, std::vector<BasicValue> params ) {
			auto & current_array( retrieve_value( m_arrays, name ) );
			return current_array( convert_dimensions( std::move( params ) ) );
		}

		std::pair<boost::string_ref, std::vector<BasicValue>> Basic::split_arrayfunction_from_string( boost::string_ref name, bool throw_on_missing_bracket ) {
			auto bracket_pos = name.find( '(' );
			if( std::string::npos == bracket_pos ) {
				if( !throw_on_missing_bracket ) {
					return std::make_pair( name, std::vector<BasicValue>( ) );
				} else {
					throw create_basic_exception( ErrorTypes::FATAL, "Expected to find start bracket but none found." );
				}
			}
			auto lb_count = std::count( std::begin( name ), std::end( name ), '(' );
			auto rb_count = std::count( std::begin( name ), std::end( name ), ')' );
			if( lb_count != rb_count ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Unclosed bracket on function '" + name.to_string( ) + "'" );
			}
			auto array_name = name.substr( 0, bracket_pos );
			auto bracket_end = name.find_last_of( ')' );

			auto param_str = name.substr( bracket_pos + 1, bracket_end - bracket_pos - 1 );
			auto param_values = evaluate_parameters( param_str );
			return { array_name, std::move( param_values ) };
		}

		BasicValue & Basic::get_array_variable( boost::string_ref name ) {
			auto nameparam = split_arrayfunction_from_string( name );
			return get_array_variable( nameparam.first, std::move( nameparam.second ) );
		}

		BasicValue & Basic::get_variable( boost::string_ref name ) {
			// Parse brackets and return individual variable
			auto is_array_value = false;
			size_t brackets_start = 0;
			size_t brackets_end = 0;
			for( size_t pos = 0; pos < name.size( ); ++pos ) {
				if( '(' == name[pos] ) {
					brackets_start = pos;
					brackets_end = find_end_of_bracket( name.substr( pos ) );
					is_array_value = std::string::npos != brackets_end;
					break;
				}
			}
			if( is_array_value ) {
				auto array_name = name.substr( 0, brackets_start );
				auto params_str = name.substr( brackets_start + 1, brackets_end - 1 );
				auto params = evaluate_parameters( std::move( params_str ) );
				return get_array_variable( array_name, std::move( params ) );
			} else {
				return retrieve_value( m_variables, name );
			}
		}

		void Basic::clear_program( ) {
			m_program.clear( );
			m_program.emplace_back( -1, "" );
		}

		void Basic::clear_variables( ) {
			m_variables.clear( );
		}

		void Basic::reset( ) {
			m_basic.reset( new Basic( ) );
			clear_program( );
			clear_variables( );
		}

		std::string Basic::list_functions( ) {
			std::stringstream ss;
			auto keys = get_keys( m_functions );
			std::sort( std::begin( keys ), std::end( keys ) );
			for( auto& current_function_name : keys ) {
				const auto& current_function = m_functions[current_function_name];
				ss << current_function_name << ": " << current_function.description << "\n";
			}
			return ss.str( );
		}

		std::string Basic::list_constants( ) {
			std::stringstream ss;
			auto keys = get_keys( m_constants );
			std::sort( std::begin( keys ), std::end( keys ) );
			for( auto& current_constant_name : keys ) {
				const auto& current_constant = m_constants[current_constant_name];
				ss << current_constant_name << ": " << value_type_to_string( current_constant.value ) << " = " << to_string( current_constant.value ) << ": " << current_constant.description << "\n";
			}
			return ss.str( );
		}

		std::string Basic::list_keywords( ) {
			std::stringstream ss;
			auto keys = get_keys( m_keywords );
			std::sort( std::begin( keys ), std::end( keys ) );
			for( auto& current_keyword_name : keys ) {
				ss << current_keyword_name << "\n"; // ": " << m_keywords[current_keyword_name].description << "\n";
			}
			return ss.str( );
		}

		std::string Basic::list_variables( ) {
			std::stringstream ss;
			{
				auto keys = get_keys( m_variables );
				std::sort( std::begin( keys ), std::end( keys ) );
				for( auto& current_variable_name : keys ) {
					const auto& current_variable = get_variable( current_variable_name );
					auto value_type = get_value_type( current_variable );
					ss << current_variable_name << ": " << value_type_to_string( value_type ) << " = " << to_string( current_variable ) << "\n";
				}
			}
			{
				auto keys = get_keys( m_arrays );
				std::sort( std::begin( keys ), std::end( keys ) );
				for( auto& current_array_name : keys ) {
					const auto& array_value = m_arrays[current_array_name];
					auto dimensions = array_value.dimensions( );
					ss << current_array_name << "( ";
					for( size_t n = 0; n < dimensions.size( ); ++n ) {
						if( 0 < n ) {
							ss << ", ";
						}
						ss << dimensions[n];
					}
					ss << " )\n";
				}
			}
			return ss.str( );
		}

		BasicValue Basic::exec_function( boost::string_ref name, std::vector<BasicValue> arguments ) {
			const auto& func = m_functions[to_upper( name )].func;
			if( !func ) {
				throw create_basic_exception( ErrorTypes::FATAL, "Expected function '" + name.to_string( ) + "' to exist.  Could not find it" );
			}
			return func( std::move( arguments ) );
		}

		bool Basic::let_helper( boost::string_ref parse_string, bool show_error ) {
			auto parsed_string = split_in_two_on_char( parse_string, '=' );
			if( 2 != parsed_string.size( ) ) {
				if( show_error ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "LET requires a variable and an assignment" );
				} else {
					return false;
				}
			}
			if( is_function( parsed_string[0] ) || is_keyword( parsed_string[0] ) || is_constant( parsed_string[0] ) ) {
				if( show_error ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to set variable with name of built-in symbol" );
				} else {
					return false;
				}
			}
			auto & var = get_variable( parsed_string[0] );

			var = evaluate( parsed_string[1] );

			return true;
		}

		void Basic::init( ) {
			//////////////////////////////////////////////////////////////////////////
			// Binary Operators
			//////////////////////////////////////////////////////////////////////////
			m_binary_operators["*"] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( std::move( lhs ) ) * to_integer( std::move( rhs ) ) );
				case ValueType::REAL:
					return basic_value_real( to_numeric( std::move( lhs ) ) * to_numeric( std::move( rhs ) ) );
				case ValueType::ARRAY:
				case ValueType::BOOLEAN:
				case ValueType::EMPTY:
				case ValueType::STRING:
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to multiply non-numeric types" );
				default:
					throw std::exception { };

				}
			};

			m_binary_operators["/"] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( std::move( lhs ) ) / to_integer( std::move( rhs ) ) );
				case ValueType::REAL:
					return basic_value_real( to_numeric( std::move( lhs ) ) / to_numeric( std::move( rhs ) ) );
				case ValueType::ARRAY:
				case ValueType::BOOLEAN:
				case ValueType::EMPTY:
				case ValueType::STRING:
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to multiply non-numeric types" );
				default:
					throw std::exception { };
				}
			};

			m_binary_operators["+"] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( std::move( lhs ) ) + to_integer( std::move( rhs ) ) );
				case ValueType::REAL:
					return basic_value_real( to_numeric( std::move( lhs ) ) + to_numeric( std::move( rhs ) ) );
				case ValueType::STRING:
				{// Append
					auto lhs_str = remove_outer_quotes( to_string( std::move( lhs ) ) );
					auto rhs_str = remove_outer_quotes( to_string( std::move( rhs ) ) );
					return basic_value_string( lhs_str + rhs_str );
				}
				case ValueType::ARRAY:
				case ValueType::BOOLEAN:
				case ValueType::EMPTY:
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to add non-numeric types" );
				default:
					throw std::exception { };
				}
			};

			m_binary_operators["-"] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( std::move( lhs ) ) - to_integer( std::move( rhs ) ) );
				case ValueType::REAL:
					return basic_value_real( to_numeric( std::move( lhs ) ) - to_numeric( std::move( rhs ) ) );
				case ValueType::ARRAY:
				case ValueType::BOOLEAN:
				case ValueType::EMPTY:
				case ValueType::STRING:
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to subtract non-numeric types" );
				default:
					throw std::exception { };
				}
			};

			m_binary_operators["^"] = [&]( BasicValue lhs, BasicValue rhs ) {
				return exec_function( "POW", { std::move( lhs ), std::move( rhs ) } );
			};

			m_binary_operators["%"] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( std::move( lhs ) ) % to_integer( std::move( rhs ) ) );
				case ValueType::ARRAY:
				case ValueType::BOOLEAN:
				case ValueType::EMPTY:
				case ValueType::STRING:
				case ValueType::REAL:
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to do modular arithmetic with non-integers" );
				default:
					throw std::exception { };
				}
			};

			m_binary_operators["="] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				boolean result = false;
				switch( result_type ) {
				case ValueType::BOOLEAN:
					result = to_boolean( lhs ) == to_boolean( rhs );
					break;
				case ValueType::EMPTY:
					if( lhs.first == rhs.first ) {
						result = true;
					} else {
						throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to compare different types " + value_type_to_string( lhs ) + " and " + value_type_to_string( rhs ) );
					}
					break;
				case ValueType::INTEGER:
					result = to_integer( lhs ) == to_integer( rhs );
					break;
				case ValueType::REAL:
					result = almost_equal( to_numeric( lhs ), to_numeric( rhs ) );
					break;
				case ValueType::STRING:
					result = 0 == to_string( lhs ).compare( to_string( rhs ) );
					break;
				case ValueType::ARRAY:
					throw create_basic_exception( ErrorTypes::FATAL, "Unknown ValueType" );
				default:
					throw std::exception { };
				}
				return basic_value_boolean( result );
			};

			m_binary_operators["<"] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				boolean result = false;
				switch( result_type ) {
				case ValueType::BOOLEAN:
					result = to_boolean( lhs ) < to_boolean( rhs );
					break;
				case ValueType::EMPTY:
					if( lhs.first == rhs.first ) {
						result = false;
					} else {
						throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to compare different types " + value_type_to_string( lhs ) + " and " + value_type_to_string( rhs ) );
					}
					break;
				case ValueType::INTEGER:
					result = to_integer( lhs ) < to_integer( rhs );
					break;
				case ValueType::REAL:
					result = to_numeric( lhs ) < to_numeric( rhs );
					break;
				case ValueType::STRING:
					result = 0 < to_string( lhs ).compare( to_string( rhs ) );
					break;
				case ValueType::ARRAY:
					throw create_basic_exception( ErrorTypes::FATAL, "Unknown ValueType" );
				default:
					throw std::exception { };
				}
				return basic_value_boolean( result );
			};

			m_binary_operators["<="] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				boolean result = false;
				switch( result_type ) {
				case ValueType::BOOLEAN:
					result = to_boolean( lhs ) <= to_boolean( rhs );
					break;
				case ValueType::EMPTY:
					if( lhs.first == rhs.first ) {
						result = true;
					} else {
						throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to compare different types " + value_type_to_string( lhs ) + " and " + value_type_to_string( rhs ) );
					}
					break;
				case ValueType::INTEGER:
					result = to_integer( lhs ) <= to_integer( rhs );
					break;
				case ValueType::REAL:
					result = to_numeric( lhs ) <= to_numeric( rhs );
					break;
				case ValueType::STRING:
					result = 0 <= to_string( lhs ).compare( to_string( rhs ) );
					break;
				case ValueType::ARRAY:
					throw create_basic_exception( ErrorTypes::FATAL, "Unknown ValueType" );
				default:
					throw std::exception { };
				}
				return basic_value_boolean( result );
			};

			m_binary_operators[">"] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				boolean result = false;
				switch( result_type ) {
				case ValueType::BOOLEAN:
					result = to_boolean( lhs ) >= to_boolean( rhs );
					break;
				case ValueType::EMPTY:
					if( lhs.first == rhs.first ) {
						result = false;
					} else {
						throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to compare different types " + value_type_to_string( lhs ) + " and " + value_type_to_string( rhs ) );
					}
					break;
				case ValueType::INTEGER:
					result = to_integer( lhs ) > to_integer( rhs );
					break;
				case ValueType::REAL:
					result = to_numeric( lhs ) > to_numeric( rhs );
					break;
				case ValueType::STRING:
					result = 0 > to_string( lhs ).compare( to_string( rhs ) );
					break;
				case ValueType::ARRAY:
					throw create_basic_exception( ErrorTypes::FATAL, "Unknown ValueType" );
				default:
					throw std::exception { };
				}
				return basic_value_boolean( result );
			};

			m_binary_operators[">="] = [&]( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				boolean result = false;
				switch( result_type ) {
				case ValueType::BOOLEAN:
					result = to_boolean( lhs ) >= to_boolean( rhs );
					break;
				case ValueType::EMPTY:
					if( lhs.first == rhs.first ) {
						result = true;
					} else {
						throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to compare different types " + value_type_to_string( lhs ) + " and " + value_type_to_string( rhs ) );
					}
					break;
				case ValueType::INTEGER:
					result = to_integer( lhs ) >= to_integer( rhs );
					break;
				case ValueType::REAL:
					result = to_numeric( lhs ) >= to_numeric( rhs );
					break;
				case ValueType::STRING:
					result = 0 >= to_string( lhs ).compare( to_string( rhs ) );
					break;
				case ValueType::ARRAY:
					throw create_basic_exception( ErrorTypes::FATAL, "Unknown ValueType" );
				default:
					throw std::exception { };
				}
				return basic_value_boolean( result );
			};

			m_binary_operators["AND"] = [&]( BasicValue lhs, BasicValue rhs ) {
				return basic_value_boolean( to_boolean( lhs ) && to_boolean( rhs ) );
			};

			m_binary_operators["OR"] = [&]( BasicValue lhs, BasicValue rhs ) {
				return basic_value_boolean( to_boolean( lhs ) || to_boolean( rhs ) );
			};

			//////////////////////////////////////////////////////////////////////////
			// Unary Operators
			//////////////////////////////////////////////////////////////////////////

			m_unary_operators["NEG"] = [&]( BasicValue lhs ) {
				if( ValueType::INTEGER == lhs.first ) {
					return basic_value_integer( -to_integer( lhs ) );
				} else if( ValueType::REAL == lhs.first ) {
					return basic_value_real( -to_real( lhs ) );
				}
				throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to apply a negative sign to a non-number" );
			};

			//////////////////////////////////////////////////////////////////////////
			// Functions
			//////////////////////////////////////////////////////////////////////////
			// Mathematical
			//////////////////////////////////////////////////////////////////////////

			add_function( "COS", "COS( Angle ) -> Returns the cosine of angle in radians", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "COS requires 1 parameter" );
				}
				return basic_value_real( cos( to_numeric( value[0] ) ) );
			} );

			add_function( "SIN", "SIN( Angle ) -> Returns the sine of angle in radians", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "SIN requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = sin( dbl_param );
				return basic_value_real( std::move( result ) );
			} );

			add_function( "TAN", "TAN( Angle ) -> Returns the tangent of angle in radians", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "TAN requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = tan( dbl_param );
				return basic_value_real( std::move( result ) );
			} );

			add_function( "ATN", "ATN( Angle ) -> Returns the arctangent of angle in radians", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "ATN requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = atan( dbl_param );
				return basic_value_real( std::move( result ) );
			} );

			add_function( "EXP", "EXP( Exponent ) -> Resturn e raised to the power of exponent. Where e = 2.71828183...", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "EXP requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = exp( dbl_param );
				return basic_value_real( std::move( result ) );
			} );

			add_function( "LOG", "LOG( x ) -> Returns the natural logarithm of x", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "LOG requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = log( dbl_param );
				return basic_value_real( std::move( result ) );
			} );

			add_function( "SQR", "SQR( x ) -> Returns the square root of x", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "SQRT requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = sqrt( dbl_param );
				return basic_value_real( std::move( result ) );
			} );

			add_function( "SQUARE", "SQUARE( x ) -> Returns x squared", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "SQR requires 1 parameter" );
				}
				if( ValueType::INTEGER == value[0].first ) {
					auto int_param = to_integer( value[0] );
					int_param *= int_param;
					return basic_value_integer( std::move( int_param ) );
				} else {
					auto dbl_param = to_numeric( value[0] );
					dbl_param *= dbl_param;
					return basic_value_real( std::move( dbl_param ) );
				}
			} );

			add_function( "ABS", "ABS( x ) -> Returns the absolute value of x", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "SIN requires 1 parameter" );
				}
				if( ValueType::INTEGER == value[0].first ) {
					auto int_param = to_integer( value[0] );
					if( 0 > int_param ) {
						int_param = -int_param;
					}
					return basic_value_integer( std::move( int_param ) );
				} else {
					auto dbl_param = to_numeric( value[0] );
					auto result = fabs( dbl_param );
					return basic_value_real( std::move( result ) );
				}
			} );

			add_function( "SGN", "SGN( x ) -> Returns the sign of x ( -1 for negative, 0 for 0, and 1 for positive)", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "SGN requires 1 parameter" );
				}
				auto result = to_numeric( value[0] );
				if( 0 < result ) {
					result = 1;
				} else if( 0 > result ) {
					result = -1;
				} else {
					result = 0;
				}
				if( ValueType::INTEGER == value[0].first ) {
					return basic_value_integer( static_cast<integer>(result) );
				}
				return basic_value_real( result );
			} );

			add_function( "INT", "INT( x ) -> Returns x truncated to the greatest integer less or equal", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "INT requires 1 parameter" );
				}
				if( ValueType::INTEGER == value[0].first ) {
					return value[0];
				}

				auto result = to_real( value[0] );
				result = round( result - 0.5 );
				return basic_value_integer( static_cast<integer>(result) );
			} );

			add_function( "RND", "RND( [s] ) -> Returns a random number between 0.0 and 1.0.  An optional seed can be specified", [&]( std::vector<BasicValue> value ) -> BasicValue {
				if( 1 >= value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "INT requires 1 or 0 parameters" );
				}
				throw create_basic_exception( ErrorTypes::SYNTAX, "Not implemented" );
				// 				if( ValueType::INTEGER == value[0].first ) {
				// 					return value[0];
				// 				}
				// 
				// 				auto result = to_real( value[0] );
				// 				result = round( result - 0.5 );
				// 				return basic_value_integer( static_cast<integer>(result) );				
			} );

			add_function( "NEG", "NEG( x ) -> Returns the negated number", [&]( std::vector<BasicValue> values ) {
				if( 1 != values.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "NEG requires 1 parameter" );
				}
				const BasicValue zero = basic_value_integer( 0 );
				return m_binary_operators["-"]( zero, values[0] );
			} );

			add_function( "POW", "POW( base, exponent ) -> Returns base raised to the power exponent", [&]( std::vector<BasicValue> value ) {
				if( 2 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "POW requires 2 parameters" );
				}
				if( ValueType::INTEGER == determine_result_type( value[0].first, value[1].first ) ) {
					auto int_param1 = to_integer( value[0] );
					auto int_param2 = to_integer( value[1] );
					auto result = static_cast<integer>(pow( std::move( int_param1 ), std::move( int_param2 ) ));
					return basic_value_integer( std::move( result ) );
				} else {
					auto result = pow( to_numeric( value[0] ), to_numeric( value[1] ) );
					return basic_value_real( std::move( result ) );
				}
			} );
			//////////////////////////////////////////////////////////////////////////
			// Logical
			//////////////////////////////////////////////////////////////////////////

			add_function( "NOT", "Boolean negation", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "NOT requires 1 parameter" );
				}
				return basic_value_boolean( !to_boolean( value[0] ) );
			} );

			//////////////////////////////////////////////////////////////////////////
			// Character and String Processing
			//////////////////////////////////////////////////////////////////////////

			add_function( "LEN", "LEN( s ) -> Returns the length of string s", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "LEN requires 1 parameter" );
				} else if( ValueType::STRING != get_value_type( value[0] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "LEN only works on string data" );
				}
				auto str_value = to_string( value[0] );
				assert( can_fit<daw::basic::integer>( str_value.size( ) ) );
				return basic_value_integer( static_cast<daw::basic::integer>(str_value.size( )) );
			} );

			add_function( "LEFT$", "LEFT$( string, len ) -> Returns the left side of the string up to len characters long", [&]( std::vector<BasicValue> value ) {
				if( 2 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "LEFT$ requires 2 parameters" );
				} else if( ValueType::STRING != get_value_type( value[0] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "The first parameter of LEFT$ must be a string" );
				} else if( ValueType::INTEGER != get_value_type( value[1] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "The second parameter of LEFT$ must be an integer" );
				}
				auto len = [&]( ) {
					auto result = to_integer( value[1] );
					if( 0 > result ) {
						throw create_basic_exception( ErrorTypes::SYNTAX, "The len parameter of LEFT$ must be positive" );
					}
					assert( can_fit<size_t>( result ) );
					return static_cast<size_t>(result);
				}( );
				return basic_value_string( to_string( value[0] ).substr( 0, std::move( len ) ) );
			} );

			add_function( "RIGHT$", "RIGHT$( string, len ) -> Returns the right side of the string up to len characters long", [&]( std::vector<BasicValue> value ) {
				if( 2 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "RIGHT$ requires 2 parameters" );
				} else if( ValueType::STRING != get_value_type( value[0] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "The first parameter of RIGHT$ must be a string" );
				} else if( ValueType::INTEGER != get_value_type( value[1] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "The second parameter of RIGHT$ must be an integer" );
				}
				auto str_value = to_string( value[0] );
				auto start = [&]( ) {
					auto result = to_integer( value[1] );
					if( 0 > result ) {
						throw create_basic_exception( ErrorTypes::SYNTAX, "The len parameter of RIGHT$ must be positive" );
					}
					assert( can_fit<size_t>( result ) );
					return static_cast<size_t>(result);
				}();
				start = str_value.size( ) - start;
				return basic_value_string( str_value.substr( start ) );
			} );

			add_function( "MID$", "MID$( string, start, len ) -> Returns the middle of the string from start up to len characters long", [&]( std::vector<BasicValue> value ) {
				if( 3 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "MID$ requires 3 parameters" );
				} else if( ValueType::STRING != get_value_type( value[0] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "The first parameter of MID$ must be a string" );
				} else if( ValueType::INTEGER != get_value_type( value[1] ) || ValueType::INTEGER != get_value_type( value[2] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "The parameters start and len of MID$ must be an integer" );
				}

				auto start = [&]( ) {
					auto result = to_integer( std::move( value[1] ) );
					if( 0 > result ) {
						throw create_basic_exception( ErrorTypes::SYNTAX, "The start parameter of MID$ must be greater than zero" );
					}
					--result;	// BASIC arrays start at 1
					assert( can_fit<size_t>( result ) );
					return static_cast<size_t>( result );
				}( );
				
				auto len = [&]( ) {
					auto result = to_integer( std::move( value[2] ) );
					if( 0 > result ) {
						throw create_basic_exception( ErrorTypes::SYNTAX, "The len parameter of MID$ must be positive" );
					}
					assert( can_fit<size_t>( result ) );
					return static_cast<size_t>(result);
				}( );
				return basic_value_string( to_string( std::move( value[0] ) ).substr( start, len ) );
			} );

			add_function( "STR$", "STR$( x ) -> Converts a number to a string", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "STR$ requires 1 parameter" );
				} else if( ValueType::INTEGER != get_value_type( value[0] ) && ValueType::REAL != get_value_type( value[0] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "STR$ only works on numeric data" );
				}
				return basic_value_string( to_string( std::move( value[0] ) ) );
			} );

			add_function( "VAL", "VAL( s ) -> Converts a string to a number", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "VAL requires 1 parameter" );
				} else if( ValueType::STRING != get_value_type( value[0] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "VAL only works on string data" );
				}
				auto str_value = to_string( value[0] );
				switch( get_value_type( str_value ) ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( std::move( str_value ) ) );
				case ValueType::REAL:
					return basic_value_real( to_real( std::move( str_value ) ) );
				case ValueType::ARRAY:
				case ValueType::BOOLEAN:
				case ValueType::EMPTY:
				case ValueType::STRING:
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to convert a string of non-numbers to a number" );
				default:
					throw std::exception { };
				}
			} );

			add_function( "ASC", "ASC( s ) -> Returns the ASCII code of the first character of a string", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "ASC requires 1 parameter" );
				} else if( ValueType::STRING != get_value_type( value[0] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "ASC only works on string data" );
				}
				auto chr_value = to_string( value[0] )[0];
				assert( can_fit<integer>( chr_value ) );
				return basic_value_integer( static_cast<integer>(chr_value) );
			} );

			add_function( "CHR$", "CHR$( x ) -> Returns a string with the character of the specified ASCII code", [&]( std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "CHR$ requires 1 parameter" );
				} else if( ValueType::INTEGER != get_value_type( value[0] ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "CHR$ only works on integer data" );
				}
				auto ascii_code = to_integer( value[0] );
				if( 0 > ascii_code || 255 < ascii_code ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Specified ASCII code must be between 0 and 255 inclusive" );
				}
				assert( can_fit<char>( ascii_code ) );
				return basic_value_string( char_to_string( static_cast<char>(ascii_code) ) );
			} );

			// 			add_function( "SPLIT$", "SPLIT$( string, delimiter ) -> Returns an array of strings from the original string delimited by delimiter", [&]( std::vector<BasicValue> value ) {
			// 				if( 2 != value.size( ) ) {
			// 					throw CreateError( ErrorTypes::SYNTAX, "SPLIT$ requires 2 parameters" );
			// 				} else if( ValueType::STRING != get_value_type( value[0] ) || ValueType::STRING != get_value_type( value[1] ) ) {
			// 					throw CreateError( ErrorTypes::SYNTAX, "SPLIT$ requires string parameters" );
			// 				}
			// 				auto str_string = to_string( value[0] );
			// 				auto str_delim = to_string( value[1] );
			// 				auto result = split( std::move( str_string ), std::move( str_delim ) );
			// 				return basic_value_array( std::move( result ) );
			// 			} );

			//////////////////////////////////////////////////////////////////////////
			// Keywords
			//////////////////////////////////////////////////////////////////////////

			m_keywords["NEW"] = [&]( boost::string_ref ) {
				reset( );
				return true;
			};

			m_keywords["CLR"] = [&]( boost::string_ref parse_string ) {
				parse_string = trim( parse_string );
				if( parse_string.empty( ) ) {
					clear_variables( );
				} else {
					remove_variable( parse_string );
				}
				return true;
			};

			m_keywords["DELETE"] = [&]( boost::string_ref parse_string ) {
				if( ValueType::INTEGER != get_value_type( parse_string ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "DELETE requires an INTEGER parameter for the line number to delete" );
				}
				remove_line( to_integer( parse_string ) );
				return true;
			};

			m_keywords["DIM"] = [&]( boost::string_ref parse_string ) {
				auto var_name_and_param = split_in_two_on_char( parse_string, '(' );
				if( 2 != var_name_and_param.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Could not find parameters surrounded by ( )" );
				}
				var_name_and_param[1] = var_name_and_param[1].substr( 0, find_end_of_bracket( var_name_and_param[1] ) );

				auto params = evaluate_parameters( var_name_and_param[1] );
				if( 2 < params.size( ) || 1 > params.size( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Must specify at least 1 size parameter to DIM and optionally 2" );
				}

				// 				size_t dim_1 = to_integer( params[0] );
				// 				size_t dim_2 = 0;
				// 				if( 2 == params.size( ) ) {
				// 					dim_2 = to_integer( params[1] );
				// 				}

				auto var_name = var_name_and_param[0];
				if( is_keyword( var_name ) || is_function( var_name ) || is_constant( var_name ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Cannot create an array with the same name as a keyword or function" );
				}
				if( is_variable( var_name ) ) {
					// Do error or erase.  check in spec but for now erase
					remove_variable( var_name );
				} else if( is_array( var_name ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to Re-DIM an existing array" );
				}
				add_array_variable( var_name, params );

				return true;
			};

			m_keywords["LET"] = [&]( boost::string_ref parse_string ) {
				return let_helper( parse_string );
			};

			m_keywords["STOP"] = [&]( boost::string_ref ) {
				if( RunMode::IMMEDIATE == m_run_mode ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to STOP from outside a program" );
				}
				std::cout << "BREAK IN " << m_program_it->first << std::endl;
				m_exiting = true;
				return true;
			};

			m_keywords["CONT"] = [&]( boost::string_ref ) {
				if( RunMode::DEFERRED == m_run_mode ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to CONT from inside a program" );
				}
				m_basic->m_run_mode = RunMode::DEFERRED;
				return m_basic->continue_run( );
			};

			m_keywords["GOTO"] = [&]( boost::string_ref parse_string ) {
				if( RunMode::IMMEDIATE == m_run_mode ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to GOTO from outside a program" );
				}
				{
					auto value_type = get_value_type( parse_string );
					if( ValueType::INTEGER != value_type ) {
						throw create_basic_exception( ErrorTypes::SYNTAX, "Can only GOTO line numbers" );
					}
				}
				set_program_it( to_integer( parse_string ), -1 );
				return true;
			};

			m_keywords["GOSUB"] = [&]( boost::string_ref parse_string ) {
				// Store program line on stack and then call goto
				if( RunMode::IMMEDIATE == m_run_mode ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to GOSUB from outside a program" );
				}
				m_program_stack.push_back( m_program_it );
				return m_keywords["GOTO"]( parse_string );
			};

			m_keywords["RETURN"] = [&]( boost::string_ref parse_string ) {
				// Pop program line from stack and then run GOTO
				if( RunMode::IMMEDIATE == m_run_mode ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to RETURN from outside a program" );
				} else if( m_program_stack.empty( ) ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to RETURN without a preceding GOSUB" );
				}

				set_program_it( pop( m_program_stack )->first );
				return true;

			};

			m_keywords["PRINT"] = [&]( boost::string_ref parse_string ) {
				parse_string = trim( parse_string );
				if( parse_string.empty( ) ) {
					std::cout << std::endl;
					return true;
				}

				std::string evaluated_value = to_string( evaluate( std::move( parse_string ) ) );
				std::cout << evaluated_value << "\n";
				return true;
			};

			m_keywords["QUIT"] = [&]( boost::string_ref ) {
				std::cout << "Good bye\n" << std::endl;
				m_exiting = true;
				return true;
			};

			m_keywords["EXIT"] = [&]( boost::string_ref ) {
				m_exiting = true;
				return true;
			};

			m_keywords["END"] = [&]( boost::string_ref ) {
				if( RunMode::IMMEDIATE == m_run_mode ) {
					throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to END from outside a program" );
				}
				m_exiting = true;
				return true;
			};

			m_keywords["REM"] = []( boost::string_ref ) {
				// truly do nothing
				return true;
			};

			m_keywords["LIST"] = [&]( boost::string_ref ) {
				sort_program_code( );
				for( auto const & current_line : m_program ) {
					if( 0 <= current_line.first ) {
						std::cout << current_line.first << "	" << current_line.second << "\n";
					}
				}
				std::cout << std::endl;
				return true;
			};

			m_keywords["RUN"] = [&]( boost::string_ref parse_line ) {
				sort_program_code( );
				integer line_number = -1;
				if( !parse_line.empty( ) && ValueType::INTEGER == get_value_type( parse_line ) ) {
					line_number = to_integer( parse_line );
				}
				if( !m_basic || 0 <= line_number ) {
					m_basic.reset( new Basic( ) );
				}
				m_basic->m_run_mode = RunMode::DEFERRED;
				m_basic->m_program = m_program;
				return m_basic->run( line_number );
			};

			m_keywords["VARS"] = [&]( boost::string_ref ) {
				std::cout << "Constants:\n" << list_constants( ) << "\n";
				std::cout << "\nVariables:\n" << list_variables( ) << "\n";
				return true;
			};

			m_keywords["FUNCTIONS"] = [&]( boost::string_ref ) {
				std::cout << list_functions( ) << std::endl;
				return true;
			};

			m_keywords["KEYWORDS"] = [&]( boost::string_ref ) {
				std::cout << list_keywords( ) << std::endl;
				return true;
			};

			m_keywords["THEN"] = [&]( boost::string_ref ) -> bool {
				throw create_basic_exception( ErrorTypes::SYNTAX, "THEN is invalid without a preceeding IF and condition" );
			};

			// 			m_keywords["IF"] = [&]( std::string parse_string ) -> bool {
			// 				if( RunMode::IMMEDIATE == m_run_mode ) {
			// 					throw CreateError( ErrorTypes::SYNTAX,  "Attempt to IF from outside a program" );
			// 				}
			// 				// IF <condition> <THEN [GOTO] <LINE NUMBER>>
			// 
			// 				// Verify that last token is an integer
			// 				auto then_pos = boost::algorithm::to_upper_copy( parse_string ).find( "THEN" );
			// 				if( std::string::npos == then_pos ) {
			// 					throw CreateError( ErrorTypes::SYNTAX,  "IF keyword must have a THEN clause" );
			// 				}
			// 				// Evaluate condition
			// 				auto condition = evaluate( parse_string.substr( 0, then_pos ) );
			// 				if( to_boolean( condition ) ) {
			// 					std::string goto_clause = parse_string.substr( then_pos + 4 );
			// 					auto goto_pos = boost::algorithm::to_upper_copy( goto_clause ).find( "GOTO" );
			// 					if( std::string::npos != goto_pos ) {
			// 						goto_clause = goto_clause.substr( goto_pos + 4 );
			// 					}
			// 					goto_clause = boost::algorithm::trim_copy( goto_clause );
			// 					if( ValueType::INTEGER != get_value_type( goto_clause ) ) {
			// 						throw CreateError( ErrorTypes::SYNTAX,  "Error parsing IF after the THEN" );
			// 					}
			// 					return m_keywords["GOTO"]( goto_clause );
			// 				}
			// 				return true;
			// 			};

			m_keywords["IF"] = [&]( boost::string_ref parse_string ) {
				// IF <CONDITION> THEN <statement>
				// IF <CONDITION> THEN <line_number>
				// IF <CONDITION> GOTO <line_number>

				// Find end of condition
				size_t start_of_thengoto_clause = std::string::npos;
				{
					auto is_then_goto = [&]( size_t pos ) {
						std::vector<std::string> const strings_to_find { "THEN", "GOTO" };
						for( auto & string_to_find : strings_to_find ) {
							if( pos + string_to_find.size( ) < static_cast<size_t>( parse_string[pos] ) ) {
								return string_to_find == to_upper( parse_string.substr( pos, string_to_find.size( ) ) );
							}
						}
						return false;
					};

					for( size_t pos = 0; pos < parse_string.size( ); ++pos ) {
						const auto current_char = parse_string[pos];
						if( '"' == current_char ) {
							pos += find_end_of_string( parse_string.substr( pos ) );
						} else if( '(' == current_char ) {
							pos += find_end_of_bracket( parse_string.substr( pos ) );
						} else if( is_then_goto( pos ) ) {
							start_of_thengoto_clause = pos;
							break;
						}
					}
					if( std::string::npos == start_of_thengoto_clause ) {
						throw create_basic_exception( ErrorTypes::SYNTAX, "Unable to find end of condition in IF keyword" );
					}
				}
				auto condition = parse_string.substr( 0, start_of_thengoto_clause );
				if( to_boolean( evaluate( condition ) ) ) {
					auto str_action = parse_string.substr( start_of_thengoto_clause + 4 );
					if( ValueType::INTEGER == get_value_type( str_action ) ) {
						str_action = "GOTO " + str_action;
					}
					return parse_line( str_action );
				}
				// Do nothing
				return true;
			};

			//////////////////////////////////////////////////////////////////////////
			// Constants
			//////////////////////////////////////////////////////////////////////////

			add_constant( "TRUE", "", basic_value_boolean( true ) );
			add_constant( "FALSE", "", basic_value_boolean( false ) );
			add_constant( "PI", "Trigometric Pi value", basic_value_real( boost::math::constants::pi<real>( ) ) );


			clear_program( );
		}

		void Basic::sort_program_code( ) {
			std::sort( std::begin( m_program ), std::end( m_program ), []( ProgramLine a, ProgramLine b ) {
				return a.first < b.first;
			} );
		}

		void Basic::set_program_it( integer line_number, integer offset ) {
			sort_program_code( );
			auto line_it = std::begin( m_program ) + std::distance( std::begin( m_program ), find_line( line_number ) + offset );
			if( std::end( m_program ) == line_it ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Attempt to jump to an invalid line" );
			}
			m_program_it = line_it;
		}

		bool Basic::continue_run( ) {
			auto next_line = m_program_it + 1;
			if( std::end( m_program ) == next_line ) {
				throw create_basic_exception( ErrorTypes::SYNTAX, "Cannot continue.  End of program reached" );
			}
			return run( next_line->first );
		}

		ProgramType::iterator Basic::first_line( ) {
			return std::begin( m_program ) + 1;
		}

		bool Basic::run( integer line_number ) {
			m_has_syntax_error = false;
			if( 0 <= line_number ) {
				set_program_it( line_number );
			} else {
				m_program_it = first_line( );
			}
			while( m_program_it != std::end( m_program ) ) {
				if( 0 <= m_program_it->first ) {
					add_constant( "CURRENT_LINE", "Current Line of program execution", basic_value_integer( m_program_it->first ) );
					if( !parse_line( m_program_it->second ) ) {
						return false;
					}
					if( m_has_syntax_error ) {
						std::cerr << "Error was on line " << m_program_it->first << std::endl;
						m_has_syntax_error = false;
						break;
					}
					if( m_exiting ) {
						m_exiting = false;
						break;
					}
					++m_program_it;
				}
			}
			return true;
		}

		Basic::Basic( ):
			m_basic { nullptr },
			m_program_it( std::end( m_program ) ),
			m_run_mode( RunMode::IMMEDIATE ),
			m_exiting( false ),
			m_has_syntax_error( false ) {
			init( );
		}

		std::vector<std::string> Basic::split( std::string text, char delimiter ) {
			std::string str_delimeter( " " );
			str_delimeter[0] = delimiter;
			return split( text, str_delimeter );
		}

		std::vector<std::string> Basic::split( std::string text, std::string delimiter ) {
			std::vector<std::string> tokens;
			size_t pos = 0;
			std::string token;

			while( std::string::npos != (pos = text.find( delimiter )) ) {
				token = text.substr( 0, pos );
				tokens.push_back( token );
				text.erase( 0, pos + delimiter.length( ) );
			}
			tokens.push_back( text );

			return tokens;
		}

		Basic::Basic( std::string program_code ):
			m_basic( nullptr ),
			m_program_it( std::end( m_program ) ),
			m_run_mode( RunMode::IMMEDIATE ),
			m_exiting( false ),
			m_has_syntax_error( false ) {
			init( );
			for( auto current_line : split( program_code, '\n' ) ) {
				parse_line( current_line );
			}

		}

		BasicException Basic::create_basic_exception( ErrorTypes error_type, std::string msg ) {
			switch( error_type ) {
			case ErrorTypes::SYNTAX:
				msg = "SYNTAX ERROR: " + msg;
				if( RunMode::DEFERRED == m_run_mode && std::end( m_program ) != m_program_it ) {
					msg += "\nError on line " + std::to_string( m_program_it->first );
				}
				return BasicException( std::move( msg ), std::move( error_type ) );
			case ErrorTypes::FATAL:
				msg = "FATAL ERROR: " + msg;
				if( RunMode::DEFERRED == m_run_mode && std::end( m_program ) != m_program_it ) {
					msg += "\nError on line " + std::to_string( m_program_it->first );
				}
				return BasicException( std::move( msg ), std::move( error_type ) );
			}
			throw std::runtime_error( "Unknown error type tried to be thrown" );
		}

		bool Basic::parse_line( boost::string_ref parse_string, bool show_ready ) {
			m_exiting = false;
			auto parsed_string( split_in_two_on_char( parse_string, ' ' ) );
			try {
				const auto value_type = get_value_type( parsed_string[0] );
				if( ValueType::INTEGER == value_type ) {
					auto line_number = to_integer( parsed_string[0] );
					if( 0 > line_number ) {
						throw create_basic_exception( ErrorTypes::SYNTAX, "Line numbers cannot be negative" );
					}
					if( parsed_string.size( ) > 1 && !parsed_string[1].empty( ) ) {
						add_line( line_number, parsed_string[1] );
					} else {
						remove_line( line_number );
					}
					return true;
				} else if( ValueType::STRING == value_type ) {
					if( trim( parse_string ).empty( ) ) {
						return true;
					}

					// Except within quoted areas, split string on colon : boundaries 

					std::vector<boost::string_ref> statements;
					{
						size_t last_pos = 0;
						size_t pos = 0;
						for( ; pos < parse_string.size( ); ++pos ) {
							const auto current_char = parse_string[pos];
							switch( current_char ) {
							case '"':
								pos += find_end_of_string( parse_string.substr( pos ) );
								break;
							case ':':
								statements.push_back( parse_string.substr( last_pos, pos - last_pos ) );
								if( pos + 1 < parse_string.size( ) ) {
									++pos;
								}
								last_pos = pos;
								break;
							}
						}
						statements.push_back( parse_string.substr( last_pos, pos - last_pos ) );
					}

					for( auto current_statement : statements ) {
						boost::string_ref params;
						parsed_string = split_in_two_on_char( current_statement, ' ' );
						if( 2 == parsed_string.size( ) ) {
							params = parsed_string[1];
						}
						auto keyword = to_upper( parsed_string[0] );
						bool result = false;
						if( !is_keyword( keyword ) ) {
							// Try assignment if the above fails
							if( !(result = let_helper( current_statement, false )) ) {
								throw create_basic_exception( ErrorTypes::SYNTAX, "Invalid keyword '" + keyword + "'" );
							}
						} else {
							result = m_keywords[keyword]( params );
						}
						if( m_exiting ) {
							return m_run_mode != RunMode::IMMEDIATE;
						}
						if( !result ) {
							return result;
						}
					}
					if( show_ready && RunMode::IMMEDIATE == m_run_mode ) {
						std::cout << "\nREADY" << std::endl;
					}
				}
			} catch( BasicException se ) {
				std::cerr << std::endl << se.what( ) << std::endl;
				switch( se.error_type ) {
				case ErrorTypes::SYNTAX:
				{
					if( show_ready ) {
						std::cout << "\nREADY" << std::endl;
					}
					m_has_syntax_error = true;
				}
				return true;
				case ErrorTypes::FATAL:
					return false;
				}
			} catch( std::exception ex ) {
				std::cerr << std::endl << "UNKNOWN ERROR: while parsing: " << ex.what( ) << std::endl;
				if( RunMode::DEFERRED == m_run_mode ) {
					std::cerr << "ERROR on line " << m_program_it->first << std::endl;
				}
				return false;
			}
			return true;
		}



		//Basic::LoopStackType
		Basic::LoopStackType::LoopStackValueType& Basic::LoopStackType::peek_full( ) {
			return *(std::end( loop_stack ));
		}

		ProgramType::iterator Basic::LoopStackType::peek( ) {
			return peek_full( ).start_of_loop;
		}

		ProgramType::iterator Basic::LoopStackType::pop( ) {
			auto result = peek( );
			loop_stack.pop_back( );
			return result;
		}

		bool Basic::LoopStackType::empty( ) const {
			return 0 == size( );
		}

		bool Basic::LoopStackType::LoopStackValueType::can_enter_loop_body( ) {

			return false;
		}

		size_t Basic::LoopStackType::size( ) const {
			return loop_stack.size( );
		}

		namespace {
			struct for_loop_parts_string {
				boost::string_ref counter_variable;
				boost::string_ref start_value;
				boost::string_ref end_value;
				boost::string_ref step_value;
			};

			struct for_loop_parts {
				boost::string_ref counter_variable;
				BasicValue start_value;
				BasicValue end_value;
				BasicValue step_value;
				for_loop_parts( ) = delete;
				for_loop_parts( for_loop_parts_string&& value ):
					counter_variable( value.counter_variable ),
					start_value( basic::basic_value_numeric( std::move( value.start_value ) ) ),
					end_value( basic::basic_value_numeric( std::move( value.end_value ) ) ),
					step_value( basic::basic_value_numeric( std::move( value.step_value ) ) ) { }
			};

			for_loop_parts_string parse_for_loop( boost::string_ref  for_loop ) {
				// {FOR<WS>}Variable[WS]=<evaluate start><WS>TO<WS><evaluate end>[<WS>STEP<evaluate step>]
				// { } - denotes already parsed away
				auto statement_parts = split_in_two_on_char( for_loop, '=' );
				//if(  )
				for_loop_parts_string result;
				return result;
			}
		}

		Basic::LoopStackType::LoopStackValueType::LoopStackValueType( std::shared_ptr<LoopType> loopControl, ProgramType::iterator program_line ): loop_control( loopControl ), start_of_loop( program_line ) { }

		void Basic::LoopStackType::push( std::shared_ptr<LoopType> type_of_loop, ProgramType::iterator start_of_loop ) {
			loop_stack.emplace_back( type_of_loop, start_of_loop );
		}

		Basic::LoopStackType::LoopType::~LoopType( ) { };

		Basic::LoopStackType::ForLoop::ForLoop( boost::string_ref variable_name, BasicValue start_value, BasicValue end_value, BasicValue step_value ):
			Basic::LoopStackType::LoopType( ),
			m_variable_name( variable_name.to_string( ) ),
			m_start_value( start_value ),
			m_end_value( end_value ),
			m_step_value( step_value ) {
			daw::exception::syntax_errror_on_false( is_numeric( start_value ), "Start Value must be numeric" );
			daw::exception::syntax_errror_on_false( is_numeric( end_value ), "End Value must be numeric" );
			daw::exception::syntax_errror_on_false( is_numeric( step_value ), "Step Value must be numeric" );
		}

		std::shared_ptr<Basic::LoopStackType::LoopType> Basic::LoopStackType::ForLoop::create_for_loop( ProgramType::iterator program_line ) {
			auto parts_of_for_loop = for_loop_parts( parse_for_loop( program_line->second ) );
			return std::shared_ptr<LoopType>( new ForLoop( parts_of_for_loop.counter_variable, parts_of_for_loop.start_value, parts_of_for_loop.end_value, parts_of_for_loop.step_value ) );
		}

		bool Basic::LoopStackType::ForLoop::can_enter_loop_body( ) {
			return false;
		}

	}  // namespace basic
} // namespace daw
