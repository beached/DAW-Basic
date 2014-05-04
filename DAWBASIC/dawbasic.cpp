#include "dawbasic.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/any.hpp>
#include <boost/math/constants/constants.hpp>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <vector>


namespace daw {
	namespace basic {
		using ::std::placeholders::_1;

		namespace {
			integer operator_rank( ::std::string oper ) {
				if( "^" == oper ) {
					return 1;
				} else if( "*" == oper || "\\" == oper ) {
					return 2;
				} else if( "+" == oper || "-" == oper || "%" == oper ) {
					return 3;
				} else if( ">" == oper || ">=" == oper || "<" == oper || "<=" == oper ) {
					return 4;
				} else if( "==" == oper ) {
					return 5;
				}
				throw ::std::runtime_error( "Uknown operator" );
			}

			const BasicValue EMPTY_BASIC_VALUE{ ValueType::EMPTY, boost::any( ) };

			ValueType get_value_type( const ::std::string value, ::std::string locale_str = "" ) {
				// DAW erase

				if( value.empty( ) ) {
					return ValueType::EMPTY;
				}

				typedef char charT;
				// Can only set locale once per application start.  It was slow
				static const ::std::locale loc = ::std::locale( locale_str );
				static const charT decimal_point = ::std::use_facet< ::std::numpunct<charT>>( loc ).decimal_point( );

				bool is_negative = false;
				bool has_decimal = false;

				size_t startpos = 0;

				if( '-' == value[startpos] ) {
					is_negative = true;
					startpos = 1;
				}

				const auto len( value.size( ) );
				for( size_t n = startpos; n < len; ++n ) {
					if( '-' == value[n] ) {	// We have already had a - or it is not the first
						return ValueType::STRING;
					} else if( decimal_point == value[n] ) {
						if( has_decimal || len == n + 1 ) {	// more than one decimal point or we are the last entry and there is not a possibility of another number
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

			::std::vector<::std::string> parse_left( ::std::string parse_string, char separator = ' ' ) {
				using boost::algorithm::trim;
				parse_string = trim( parse_string );
				const auto pos = parse_string.find_first_of( separator );
				::std::vector<::std::string> result;
				if( ::std::string::npos != pos ) {
					result.push_back( trim( parse_string.substr( 0, pos ) ) );
					result.push_back( trim( parse_string.substr( pos + 1 ) ) );
				} else {
					result.push_back( parse_string );
				}
				return result;
			}

			class SyntaxException: public ::std::runtime_error {
			public:
				explicit SyntaxException( const ::std::string& msg ): runtime_error( msg ) { }
				explicit SyntaxException( const char* msg ): runtime_error( msg ) { }
			};

			integer to_integer( BasicValue value ) {
				return boost::any_cast<integer>(value.second);
			}

			integer to_integer( ::std::string value ) {
				::std::stringstream ss;
				ss << value;
				integer result;
				ss >> result;
				return result;
			}

			real to_real( ::std::string value ) {
				::std::stringstream ss;
				ss << value;
				real result;
				ss >> result;
				return result;
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
				default:
					throw ::std::runtime_error( "Cannot convert non-numeric types to a number" );
				}
			}

			boolean to_boolean( BasicValue value ) {
				if( ValueType::BOOLEAN == value.first ) {
					return boost::any_cast<boolean>(value.second);
				}
				throw ::std::runtime_error( "Attempt to convert a non-boolean to a boolean" );
			}

			BasicValue basic_value_integer( integer value ) {
				return BasicValue{ ValueType::INTEGER, boost::any( ::std::move( value ) ) };
			}

			BasicValue basic_value_integer( ::std::string value ) {
				return basic_value_integer( to_integer( ::std::move( value ) ) );
			}

			BasicValue basic_value_real( real value ) {
				return BasicValue{ ValueType::REAL, boost::any( ::std::move( value ) ) };
			}

			BasicValue basic_value_real( ::std::string value ) {
				return basic_value_real( to_real( ::std::move( value ) ) );
			}

			BasicValue basic_value_boolean( boolean value ) {
				return BasicValue{ ValueType::BOOLEAN, boost::any( ::std::move( value ) ) };
			}

			BasicValue basic_value_string( ::std::string value ) {
				return BasicValue{ ValueType::STRING, boost::any( ::std::move( value ) ) };
			}

			::std::string to_string( BasicValue value ) {
				::std::stringstream ss;
				switch( value.first ) {
				case ValueType::EMPTY:
					break;
				case ValueType::INTEGER:
					ss << to_integer( ::std::move( value ) );
					break;
				case ValueType::REAL:
					ss << ::std::setprecision( ::std::numeric_limits<real>::digits10 ) << to_real( ::std::move( value ) );
					break;
				case ValueType::STRING:
					ss << "\"" << boost::any_cast<::std::string>(value.second) << "\"";
					break;
				case ValueType::BOOLEAN:
					if( to_boolean( value ) ) {
						ss << "TRUE";
					} else {
						ss << "FALSE";
					}
					break;
				default:
					throw ::std::runtime_error( "Unknown ValueType" );
				}
				return ss.str( );
			}

			template<typename M>
			auto get_keys( const M& m )  -> ::std::vector<typename M::key_type> {
				::std::vector<M::key_type> result;
				for( auto it = m.cbegin( ); it != m.cend( ); ++it ) {
					result.push_back( it->first );
				}
				return ::std::move( result );
			}

			::std::string value_type_string( ValueType value_type ) {
				switch( value_type ) {
				case ValueType::BOOLEAN:
					return "Boolean";
				case ValueType::EMPTY:
					return "Empty";
				case ValueType::INTEGER:
					return "Integer";
				case ValueType::REAL:
					return "Real";
				case ValueType::STRING:
					return "String";
				}
				throw ::std::runtime_error( "Unknown ValueType" );
			}

			::std::string value_type_string( BasicValue value ) {
				return value_type_string( ::std::move( value.first ) );
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
					}
				} else if( ValueType::REAL == lhs_type ) {
					switch( rhs_type ) {
					case ValueType::INTEGER:
					case ValueType::REAL:
						return ValueType::REAL;
					case ValueType::STRING:
						return ValueType::STRING;
					}
				} else if( ValueType::STRING == lhs_type ) {
					switch( rhs_type ) {
					case ValueType::INTEGER:
					case ValueType::REAL:
					case ValueType::STRING:
						return ValueType::STRING;
					}
				} else if( ValueType::BOOLEAN == lhs_type ) {
					switch( rhs_type ) {
					case daw::basic::ValueType::BOOLEAN:
						return ValueType::BOOLEAN;
					}
				}
				return ValueType::EMPTY;
			}

			template<typename M, typename K>
			bool key_exists( const M& kv_map, K key ) {
				boost::algorithm::to_upper( key );
				return kv_map.cend( ) != kv_map.find( key );
			}

			template<typename K, typename V>
			V& retrieve_value( ::std::map<K, V>& kv_map, K key ) {
				boost::algorithm::to_upper( key );
				return kv_map[key];
			}

			template<typename V>
			V pop( ::std::vector<V>& vect ) {
				auto result = *vect.rbegin( );
				vect.pop_back( );
				return ::std::move( result );
			}

			//////////////////////////////////////////////////////////////////////////
			/// summary: Makes a value from a single token
			//////////////////////////////////////////////////////////////////////////
			BasicValue make_value( std::string value ) {
				boost::algorithm::trim( value );
				switch( get_value_type( value ) ) {
				case daw::basic::ValueType::EMPTY:
					return EMPTY_BASIC_VALUE;
				case daw::basic::ValueType::STRING:
					return basic_value_string( ::std::move( value ) );
				case daw::basic::ValueType::INTEGER:
					return basic_value_integer( ::std::move( value ) );
				case daw::basic::ValueType::REAL:
					return basic_value_real( ::std::move( value ) );
				default:
					throw ::std::runtime_error( "Unknown value type" );
				}
			}

			int32_t find_end_of_string( ::std::string value ) {
				int32_t start = 0;
				if( '"' == value[start] ) {
					++start;
				}
				int32_t quote_counter = 1;
				for( int32_t pos = start; pos < static_cast<int32_t>(value.size( )); ++pos ) {
					if( '"' == value[pos] ) {
						if( !(0 != pos && '\\' == value[pos - 1]) ) {
							--quote_counter;
						}
					}
					if( 0 >= quote_counter ) {
						return pos;
					}
				}
				throw SyntaxException( "Could not find end of quoted string, not closing quotes" );
			}

			int32_t find_end_of_bracket( ::std::string value ) {
				int32_t bracket_count = 1;
				for( int32_t pos = 0; pos < static_cast<int32_t>(value.size( )); ++pos ) {
					if( '(' == value[pos] ) {
						++bracket_count;
					} else if( ')' == value[pos] ) {
						--bracket_count;
						if( 0 == bracket_count ) {
							return pos;
						}
					}
				}
				throw SyntaxException( "Unclosed bracket found" );
			}

			int32_t find_end_of_operand( ::std::string value ) {
				const static ::std::vector<char> end_chars{ ' ', '	', '^', '*', '/', '+', '-', '=', '<', '>', '%' };
				int32_t bracket_count = 0;

				bool has_brackets = false;
				for( int32_t pos = 0; pos < static_cast<int32_t>(value.size( )); ++pos ) {
					auto current_char = value[pos];
					if( '"' == current_char ) {
						throw SyntaxException( "Unexpected quote \" character at position " + pos );
					}
					if( 0 >= bracket_count ) {
						if( ')' == current_char ) {
							throw SyntaxException( "Unexpected close bracket ) character at position " + pos );
						} else if( ::std::end( end_chars ) != ::std::find( ::std::begin( end_chars ), ::std::end( end_chars ), current_char ) ) {
							return pos - 1;
						} else if( '(' == current_char ) {
							if( has_brackets ) {
								throw SyntaxException( "Unexpected opening bracket after brackets have closed at position " + pos );
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

			::std::string remove_outer_quotes( ::std::string value ) {
				if( value.empty( ) ) {
					return ::std::move( value );
				}
				if( 2 <= value.size( ) ) {
					if( '"' == value[0] && '"' == value[value.size( ) - 1] ) {
						return value.substr( 1, value.size( ) - 2 );
					}
				}
				return ::std::move( value );
			}

			void replace_all( ::std::string& str, const ::std::string& from, const ::std::string& to ) {
				if( from.empty( ) ) {
					return;
				}
				size_t start_pos = 0;
				while( (start_pos = str.find( from, start_pos )) != std::string::npos ) {
					str.replace( start_pos, from.length( ), to );
					start_pos += to.length( ); // In case 'to' contains 'from', like replacing 'x' with 'yx'
				}
			}

		}	// namespace anonymous

		::std::vector<BasicValue> Basic::evaluate_parameters( ::std::string value ) {
			//value = boost::algorithm::trim_copy( value );
			// Parameters are separated by comma's.  Comma's should only exist within quotes and between parameters
			::std::vector<int32_t> comma_pos;
			::std::vector<BasicValue> result;
			if( value.empty( ) ) {
				return result;
			}
			const int32_t end = value.size( ) - 1;
			for( int32_t pos = 0; pos <= end; ++pos ) {
				const auto current_char = value[pos];
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
			int32_t start = 0;
			for( auto current_end : comma_pos ) {
				auto current_param = value.substr( start, current_end - start );
				auto eval = evaluate( ::std::move( current_param ) );
				result.push_back( eval );
				start = current_end + 1;
			}
			return result;
		}

		BasicValue Basic::get_variable_constant( ::std::string name ) { 
			if( is_constant( name ) ) {
				return  constants[name];
			} else if( is_variable( name ) ) {
				return variables2[name];
			}
			throw ::std::runtime_error( "Undefined variable or constant" );
		}

		void Basic::add_variable( ::std::string name, BasicValue value ) {
			if( is_constant( name ) ) {
				throw SyntaxException( "Cannot create a variable that is a system constant" );
			} else if( is_function( name ) | is_keyword( name ) ) {
				throw SyntaxException( "Cannot create a variable with the same name as a system function/keyword" );
			}
			variables2[::std::move( name )] = ::std::move( value );
		}

		void Basic::add_constant( ::std::string name, BasicValue value ) {
			if( is_function( name ) | is_keyword( name ) ) {
				throw SyntaxException( "Cannot create a constant with the same name as a system function/keyword" );
			}
			if( is_variable( name ) ) {
				remove_variable( name );
			}
			constants[::std::move( name )] = ::std::move( value );
		}

		bool Basic::is_variable( ::std::string name ) {
			name = boost::algorithm::to_upper_copy( name );
			return key_exists( variables2, name ) || is_constant( name );
		}

		bool Basic::is_constant( ::std::string name ) {
			return key_exists( constants, ::std::move( name ) );
		}

		void Basic::remove_variable( ::std::string name, bool throw_on_nonexist ) {
			auto pos = variables2.find( boost::algorithm::to_upper_copy( name ) );
			if( variables2.end( ) == pos && throw_on_nonexist ) {
				throw SyntaxException( "Attempt to delete unknown variable" );
			}
			variables2.erase( pos );
		}

		void Basic::remove_constant( ::std::string name, bool throw_on_nonexist ) {
			auto pos = constants.find( boost::algorithm::to_upper_copy( name ) );
			if( constants.end( ) == pos && throw_on_nonexist ) {
				throw SyntaxException( "Attempt to delete unknown constant" );
			}
			constants.erase( pos );
		}

		ProgramType::iterator Basic::find_line( integer line_number ) {
			auto result = ::std::find_if( ::std::begin( program ), ::std::end( program ), [&line_number]( ProgramLine current_line ) {
				return current_line.first == line_number;
			} );
			return result;
		}

		void Basic::add_line( integer line_number, ::std::string line ) {
			auto pos = find_line( line_number );
			if( program.end( ) == pos ) {
				program.push_back( make_pair( line_number, line ) );
			} else {
				pos->second = line;
			}
		}

		void Basic::remove_line( integer line_number ) {
			auto pos = find_line( line_number );
			if( program.end( ) != pos ) {
				program.erase( pos );
			}
		}

		bool Basic::is_keyword( ::std::string name ) {
			return key_exists( keywords, name );
		}

		bool Basic::is_function( ::std::string name ) {
			return key_exists( functions, name );
		}

		bool Basic::is_symbol( ::std::string name ) {
			return is_keyword( name ) || is_function( name ) || is_variable( name ) || is_constant( name );
		}

		BasicValue& Basic::get_variable( ::std::string name ) {
			return retrieve_value( variables2, ::std::move( name ) );
		}

		void Basic::init( ) {
			binary_operands["*"] = []( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( ::std::move( lhs ) ) * to_integer( ::std::move( rhs ) ) );
				case ValueType::REAL:
					return basic_value_real( to_numeric( ::std::move( lhs ) ) * to_numeric( ::std::move( rhs ) ) );
				default:
					throw SyntaxException( "Attempt to multiply non-numeric types" );
				}
			};

			binary_operands["/"] = []( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( ::std::move( lhs ) ) / to_integer( ::std::move( rhs ) ) );
				case ValueType::REAL:
					return basic_value_real( to_numeric( ::std::move( lhs ) ) / to_numeric( ::std::move( rhs ) ) );
				default:
					throw SyntaxException( "Attempt to multiply non-numeric types" );
				}
			};

			binary_operands["+"] = []( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( ::std::move( lhs ) ) + to_integer( ::std::move( rhs ) ) );
				case ValueType::REAL:
					return basic_value_real( to_numeric( ::std::move( lhs ) ) + to_numeric( ::std::move( rhs ) ) );
				case ValueType::STRING:	{// Append
					auto lhs_str = remove_outer_quotes( to_string( ::std::move( lhs ) ) );
					auto rhs_str = remove_outer_quotes( to_string( ::std::move( rhs ) ) );
					return basic_value_string( lhs_str + rhs_str );
				}
				default:
					throw SyntaxException( "Attempt to add non-numeric types" );
				}
			};

			binary_operands["-"] = []( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( ::std::move( lhs ) ) - to_integer( ::std::move( rhs ) ) );
				case ValueType::REAL:
					return basic_value_real( to_numeric( ::std::move( lhs ) ) - to_numeric( ::std::move( rhs ) ) );
				default:
					throw SyntaxException( "Attempt to multiply non-numeric types" );
				}
			};

			binary_operands["^"] = [&]( BasicValue lhs, BasicValue rhs ) {				
				return functions["POW"]( { ::std::move( lhs ), ::std::move( rhs ) } );
			};

			binary_operands["%"] = []( BasicValue lhs, BasicValue rhs ) {
				auto result_type = determine_result_type( lhs.first, rhs.first );
				switch( result_type ) {
				case ValueType::INTEGER:
					return basic_value_integer( to_integer( ::std::move( lhs ) ) % to_integer( ::std::move( rhs ) ) );
				default:
					throw SyntaxException( "Attempt to do modular arithmetic with non-integers" );
				}
			};

			binary_operands["=="] = []( BasicValue lhs, BasicValue rhs ) {
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
						throw SyntaxException( "Attempt to compare different types " + value_type_string( lhs ) + " and " + value_type_string( rhs ) );
					}
					break;
				case ValueType::INTEGER:
					result = to_integer( lhs ) == to_integer( rhs );
					break;
				case ValueType::REAL:
					result = to_numeric( lhs ) == to_numeric( rhs );
					break;
				case ValueType::STRING:
					result = 0 == to_string( lhs ).compare( to_string( rhs ) );
					break;
				default:
					throw ::std::runtime_error( "Unknown ValueType" );
				}
				return basic_value_boolean( result );
			};

			binary_operands["<"] = []( BasicValue lhs, BasicValue rhs ) {
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
						throw SyntaxException( "Attempt to compare different types " + value_type_string( lhs ) + " and " + value_type_string( rhs ) );
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
				default:
					throw ::std::runtime_error( "Unknown ValueType" );
				}
				return basic_value_boolean( result );			
			};

			binary_operands["<="] = []( BasicValue lhs, BasicValue rhs ) {
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
						throw SyntaxException( "Attempt to compare different types " + value_type_string( lhs ) + " and " + value_type_string( rhs ) );
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
				default:
					throw ::std::runtime_error( "Unknown ValueType" );
				}
				return basic_value_boolean( result );			
			};

			binary_operands[">"] = []( BasicValue lhs, BasicValue rhs ) {
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
						throw SyntaxException( "Attempt to compare different types " + value_type_string( lhs ) + " and " + value_type_string( rhs ) );
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
				default:
					throw ::std::runtime_error( "Unknown ValueType" );
				}
				return basic_value_boolean( result );
			};

			binary_operands[">="] = []( BasicValue lhs, BasicValue rhs ) {
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
						throw SyntaxException( "Attempt to compare different types " + value_type_string( lhs ) + " and " + value_type_string( rhs ) );
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
				default:
					throw ::std::runtime_error( "Unknown ValueType" );
				}
				return basic_value_boolean( result );
			};


			functions["COS"] = []( ::std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw SyntaxException( "COS requires 1 parameter" );
				}
				return basic_value_real( cos( to_numeric( value[0] ) ) );
			};

			functions["SIN"] = []( ::std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw SyntaxException( "SIN requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = sin( dbl_param );
				return basic_value_real( ::std::move( result ) );
			};

			functions["TAN"] = []( ::std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw SyntaxException( "TAN requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = tan( dbl_param );
				return basic_value_real( ::std::move( result ) );
			};

			functions["SQRT"] = []( ::std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw SyntaxException( "SQRT requires 1 parameter" );
				}
				auto dbl_param = to_numeric( value[0] );
				auto result = sqrt( dbl_param );
				return basic_value_real( ::std::move( result ) );
			};

			functions["SQR"] = []( ::std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw SyntaxException( "SQR requires 1 parameter" );
				}
				if( ValueType::INTEGER == value[0].first ) {
					auto int_param = to_integer( value[0] );
					int_param *= int_param;
					return basic_value_integer( ::std::move( int_param ) );
				} else {
					auto dbl_param = to_numeric( value[0] );
					dbl_param *= dbl_param;
					return basic_value_real( ::std::move( dbl_param ) );
				}
			};

			functions["ABS"] = []( ::std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw SyntaxException( "SIN requires 1 parameter" );
				}
				if( ValueType::INTEGER == value[0].first ) {
					auto int_param = to_integer( value[0] );
					if( 0 > int_param ) {
						int_param = -int_param;
					}
					return basic_value_integer( ::std::move( int_param ) );
				} else {
					auto dbl_param = to_numeric( value[0] );
					auto result = abs( dbl_param );
					return basic_value_real( ::std::move( result ) );
				}
			};


			functions["POW"] = []( ::std::vector<BasicValue> value ) {
				if( 2 != value.size( ) ) {
					throw SyntaxException( "POW requires 2 parameter" );
				}
				if( ValueType::INTEGER == determine_result_type( value[0].first, value[1].first ) ) {
					auto int_param1 = to_integer( value[0] );
					auto int_param2 = to_integer( value[1] );
					auto result = static_cast<integer>( pow( ::std::move( int_param1 ), ::std::move( int_param2 ) ) );
					return basic_value_integer( ::std::move( result ) );
				} else {
					auto result = pow( to_numeric( value[0] ), to_numeric( value[1] ) );
					return basic_value_real( ::std::move( result ) );
				}
			};

			functions["NOT"] = []( ::std::vector<BasicValue> value ) {
				if( 1 != value.size( ) ) {
					throw SyntaxException( "NOT requires 1 parameter" );
				} else if( ValueType::BOOLEAN != value[0].first ) {
					throw SyntaxException( "NOT can only take a boolean parameter" );
				}
				return basic_value_boolean( !to_boolean( value[0] ) );
			};

			keywords["DELETE"] = [&]( ::std::string parse_string )-> bool {
				if( parse_string.empty( ) ) {
					variables2.clear( );
				} else {
					remove_variable( parse_string );
				}
				return true;
			};

			keywords["DIM"] = [&]( ::std::string parse_string ) mutable -> bool {
				throw ::std::runtime_error( "Not implemented" );
				return true;
			};

			keywords["LET"] = [&]( ::std::string parse_string ) mutable -> bool {
				auto parsed_string = parse_left( parse_string, '=' );
				::std::string str_value;
				if( 2 == parsed_string.size( ) ) {
					str_value = parsed_string[1];
				} else { 
					throw SyntaxException( "LET requires a variable and an assignment" );
				}
				if( is_function( parsed_string[0] ) || is_keyword( parsed_string[0] ) ) {
					throw SyntaxException( "Attempt to set variable with name of built-in symbol" );
				}
				const auto value_type = get_value_type( str_value );
				auto& var = get_variable( parsed_string[0] );
				
				var = evaluate( str_value );

				return true;
			};

			keywords["GOTO"] = [&]( ::std::string parse_string ) -> bool {
				if( RunMode::IMMEDIATE == m_run_mode ) {
					throw SyntaxException( "Attempt to GOTO from outside a program" );
				}
				{
					auto value_type = get_value_type( parse_string );
					if( ValueType::INTEGER != value_type ) {
						throw SyntaxException( "Can only GOTO line numbers" );
					}
				}
				auto line_number = to_integer( parse_string );
				auto line_it = ::std::begin( program ) + ::std::distance( ::std::begin( program ), find_line( line_number ) ) - 1;
				if( ::std::end( program ) == line_it ) {
					throw SyntaxException( "Attempt to GOTO a non-existent line" );
				}
				m_program_it = line_it;
				return true;
			};
			
			keywords["PRINT"] = [&]( ::std::string parse_string ) mutable -> bool {
				boost::algorithm::trim( parse_string );
				if( parse_string.empty( ) ) {
					::std::cout << ::std::endl;
					return true;
				}				
				::std::string evaluated_value = to_string( evaluate( parse_string ) );
				::std::cout << ::std::move( evaluated_value ) << ::std::endl;
				return true;
			};

			keywords["QUIT"] = []( ::std::string ) -> bool {
				::std::cout << "Good bye\n" << std::endl;
				return false;
			};

			keywords["REM"] = []( ::std::string ) -> bool {
				// truly do nothing
				return true;
			};

			keywords["LIST"] = [&]( ::std::string ) -> bool {
				::std::sort( ::std::begin( program ), ::std::end( program ), []( ProgramLine a, ProgramLine b ) {
					return a.first < b.first;
				} );
				for( auto& current_line : program ) {
					if( 0 <= current_line.first ) {
						::std::cout << current_line.first << "	" << current_line.second << "\n";
					}
				}
				::std::cout << ::std::endl;
				return true;
			};

			keywords["RUN"] = [&]( ::std::string ) -> bool {
				::std::sort( ::std::begin( program ), ::std::end( program ), []( ProgramLine a, ProgramLine b ) {
					return a.first < b.first;
				} );
				Basic b;
				b.m_run_mode = RunMode::PROGRAM;
				b.program = program;
				return b.run( );
			};

			keywords["VARS"] = [&]( ::std::string ) -> bool {
				::std::cout << variables2.size( ) + constants.size( ) << " variable(s) in use\n";
				::std::cout << "Constants:\n";
				for( auto it = constants.begin( ); it != constants.end( ); ++it ) {
					const auto value = it->second;
					::std::cout << it->first << ": " << value_type_string( value.first ) << ": " << to_string( value ) << ::std::endl;
				}
				::std::cout << "\nVariables:\n";
				for( auto it = variables2.begin( ); it != variables2.end( ); ++it ) {
					const auto value = it->second;
					::std::cout << it->first << ": " << value_type_string( value.first ) << ": " << to_string( value ) << ::std::endl;
				}
				return true;
			};

			keywords["FUNCTIONS"] = [&]( ::std::string ) -> bool {
				auto funcs = get_keys( functions );
				for( auto& func : get_keys( functions ) ) {
					::std::cout << func << ::std::endl;
				}
				return true;

			};

			keywords["THEN"] = []( std::string ) -> bool {
				throw SyntaxException( "THEN is invalid without a preceeding IF and condition" );
			};

			keywords["IF"] = [&]( ::std::string parse_string ) -> bool {
				if( RunMode::IMMEDIATE == m_run_mode ) {
					throw SyntaxException( "Attempt to IF from outside a program" );
				}
				// IF <condition> <THEN [GOTO] <LINE NUMBER>>

				// Verify that last token is an integer
				auto then_pos = boost::algorithm::to_upper_copy( parse_string ).find( "THEN" );
				if( ::std::string::npos == then_pos ) {
					throw SyntaxException( "IF keyword must have a THEN clause" );
				}
				// Evaluate condition
				auto condition = evaluate( parse_string.substr( 0, then_pos ) );
				if( to_boolean( condition ) ) {
					::std::string goto_clause = parse_string.substr( then_pos + 4 );
					auto goto_pos = boost::algorithm::to_upper_copy( goto_clause ).find( "GOTO" );
					if( ::std::string::npos != goto_pos ) {
						goto_clause = goto_clause.substr( goto_pos + 4 );
					}
					goto_clause = boost::algorithm::trim_copy( goto_clause );
					if( ValueType::INTEGER != get_value_type( goto_clause ) ) {
						throw SyntaxException( "Error parsing IF after the THEN" );
					}
					return keywords["GOTO"]( goto_clause );
				}
				return true;
			};

			add_constant( "TRUE", basic_value_boolean( true ) );
			add_constant( "FALSE", basic_value_boolean( false ) );
			add_constant( "PI", basic_value_real( boost::math::constants::pi<real>( ) ) );

			program.emplace_back( -1, "" );
		}
		
		//////////////////////////////////////////////////////////////////////////
		/// summary: Evaluate a string and solve all functions/variables
		BasicValue Basic::evaluate( ::std::string value ) {
			auto current_position = 0;
			::std::vector<BasicValue> operand_stack;
			::std::vector<::std::string> operator_stack;

			const auto char_to_string = []( char chr ) {
				::std::string result( " " );
				result[0] = chr;
				return ::std::move( result );
			};

			const auto is_higher_precedence = [&operator_stack]( std::string oper ) {
				if( operator_stack.empty( ) ) {
					return true;
				}
				auto from_stack = *(operator_stack.rbegin( ));
				return operator_rank( oper ) < operator_rank( from_stack );
			};

			const int32_t end = value.size( ) - 1;

			while( current_position <= end ) {
				const auto& current_char = value[current_position];
				switch( current_char ) {
				case '"':{ // String boundary
					auto current_operand = value.substr( current_position );
					auto end_of_string = find_end_of_string( current_operand );
					current_operand = current_operand.substr( 0, end_of_string + 1 );
					current_operand = remove_outer_quotes( current_operand );
					replace_all( current_operand, "\\\"", "\"" );
					operand_stack.push_back( basic_value_string( ::std::move( current_operand ) ) );
					current_position += end_of_string;
				}
				break;
				case '(':	// Bracket boundary				
				{

					auto text_in_bracket = value.substr( current_position + 1 );
					auto end_of_bracket = find_end_of_bracket( text_in_bracket );
					text_in_bracket = text_in_bracket.substr( 0, end_of_bracket );
					auto evaluated_bracket = evaluate( ::std::move( text_in_bracket ) );
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
				case '%':
				case '^':
				case '*':
				case '/':
				case '+':
				case '-':
				case '<': 
				case '>':
				case '=': {
					::std::string current_operator = char_to_string( current_char );
					if( '>' == current_char || '<' == current_char || '=' == current_char ) {
						if( current_position >= end ) {
							throw SyntaxException( "Binary operator with only left hand side, not right" );
						}
						if( '=' == value[current_position + 1] ) {
							current_operator += value[++current_position];							
						}
					}
					if( !is_higher_precedence( current_operator ) ) {
						// Pop from operand stack and do operators and push value back to stack
						auto rhs = pop( operand_stack );
						auto prev_operator = pop( operator_stack );
						auto result = binary_operands[prev_operator]( pop( operand_stack ), rhs );
						operand_stack.push_back( result );
					}
					operator_stack.push_back( current_operator );
				}
				break;
				default: {
					auto current_operand = value.substr( current_position );
					auto end_of_operand = find_end_of_operand( current_operand );
					current_operand = current_operand.substr( 0, end_of_operand + 1 );
					auto current_operand_upper = boost::algorithm::to_upper_copy( current_operand );
					int32_t lb_count = ::std::count( ::std::begin( current_operand ), ::std::end( current_operand ), '(' );
					int32_t rb_count = ::std::count( ::std::begin( current_operand ), ::std::end( current_operand ), ')' );
					if( lb_count != rb_count ) {
						throw SyntaxException( "Unclosed bracket on function '" + current_operand + "'" );
					}
					if( 0 < lb_count ) {	// We are a function, find parameters and push value to stack
						// Find function name
						auto first_bracket = current_operand.find( '(' );
						auto function_name = boost::algorithm::to_upper_copy( current_operand.substr( 0, first_bracket ) );
						if( !is_function( function_name ) ) {
							throw SyntaxException( "Unknown function '" + function_name + "'" );
						}
						auto param_string = current_operand.substr( first_bracket + 1, current_operand.size( ) - first_bracket - 2 );
						auto function_parameters = evaluate_parameters( param_string );
						operand_stack.push_back( functions[function_name]( function_parameters ) );
					} else if( is_variable( current_operand_upper ) ) {
						// We are a variable, push value onto stack
						operand_stack.emplace_back( get_variable_constant( current_operand_upper ) );
					} else {
						// We must be a number
						auto value_type = get_value_type( current_operand );
						switch( value_type ) {
						case ValueType::INTEGER:
							operand_stack.push_back( basic_value_integer( ::std::move( current_operand ) ) );
							break;
						case ValueType::REAL:
							operand_stack.push_back( basic_value_real( ::std::move( current_operand ) ) );
							break;
						default:
							throw SyntaxException( "Unknown symbol '" + current_operand + "'" );
						}
					}
					current_position += end_of_operand;
				}
				break;
				}	// switch				
				++current_position;
			}	// while
			// finish stacks
			while( !operator_stack.empty( ) ) {
				auto current_operator = pop( operator_stack );
				auto rhs = pop( operand_stack );
				auto result = binary_operands[current_operator]( pop( operand_stack ), ::std::move( rhs ) );
				operand_stack.push_back( result );
			}
			if( operand_stack.empty( ) ) {
				return EMPTY_BASIC_VALUE;
			}
			auto current_operand = pop( operand_stack );
			if( !operand_stack.empty( ) ) {
				throw SyntaxException( "Error parsing line" );
			}
			return current_operand;
		}

		bool Basic::run( ) {
			has_syntax_error = false;
			for( m_program_it = ::std::begin( program ); m_program_it != ::std::end( program ); ++m_program_it ) {
				if( 0 <= m_program_it->first ) {
					add_constant( "CURRENT_LINE", basic_value_integer( m_program_it->first ) );
					if( !parse_line( m_program_it->second ) ) {
						return false;
					}
					if( has_syntax_error ) {
						::std::cerr << "Error was on line " << m_program_it->first << ::std::endl;
						has_syntax_error = false;
						break;
					}
				}
			}
			return true;
		}

		Basic::Basic( ): m_run_mode( RunMode::IMMEDIATE ), m_program_it( ::std::end( program ) ), has_syntax_error( false ) {
			init( );
		}

		bool Basic::parse_line( const ::std::string& parse_string ) {
			const auto parsed_string( parse_left( parse_string ) );
			try {
				const auto value_type = get_value_type( parsed_string[0] );
				if( ValueType::INTEGER == value_type ) {
					auto line_number = to_integer( parsed_string[0] );
					if( 0 > line_number ) {
						throw SyntaxException( "Line numbers cannot be negative" );
					}
					if( parsed_string.size( ) > 1 && !parsed_string[1].empty( ) ) {
						add_line( line_number, parsed_string[1] );
					} else {
						remove_line( line_number );
					}
					return true;
				} else if( ValueType::STRING == value_type ) {
					::std::string params;
					if( 2 == parsed_string.size( ) ) {
						params = parsed_string[1];
					}
					auto keyword = boost::algorithm::to_upper_copy( parsed_string[0] );
					if( !is_keyword( keyword ) ) {
						throw SyntaxException( "Invalid keyword '" + keyword + "'" );
					}
					auto result = keywords[keyword]( params );
					if( result && RunMode::IMMEDIATE == m_run_mode) {
						::std::cout << "\nREADY" << ::std::endl;
					}
					return result;
				} else if( ValueType::EMPTY == value_type ) {
					return true;
				}
				::std::string msg( "Syntax Error: " );
				msg += parsed_string[0];
				msg += " is an invalid token";
				throw SyntaxException( msg );
			} catch( SyntaxException se ) {
				::std::cerr << ::std::endl << "SYNTAX ERROR\n" << se.what( ) << ::std::endl;
				has_syntax_error = true;
				return true;
			} catch( ::std::exception ex ) {
				::std::cerr << ::std::endl << "UNKNOWN ERROR: while parsing line: " << ex.what( ) << ::std::endl;
				return false;
			}
		}

	}  // namespace basic
} // namespace daw