#ifndef HAS_DAWBASIC_HEADER
#define HAS_DAWBASIC_HEADER

#include <boost/any.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <unordered_map>


namespace daw {
	namespace basic {
		class BasicException: public ::std::runtime_error {
		public:
			explicit BasicException( const ::std::string& msg ): runtime_error( msg ) { }
			explicit BasicException( const char* msg ): runtime_error( msg ) { }
		};

		enum class ValueType { EMPTY, STRING, INTEGER, REAL, BOOLEAN, STRINGARRAY, INTEGERARRAY, REALARRAY, BOOLEANARRAY };
		typedef int32_t integer;
		typedef double real;
		typedef bool boolean;

		typedef ::std::pair<ValueType, boost::any> BasicValue;
		typedef ::std::function<bool( ::std::string )> BasicKeyword;
		typedef ::std::function<BasicValue( ::std::vector<BasicValue> )> BasicFunction;
		typedef ::std::function < BasicValue( BasicValue, BasicValue )> BasicBinaryOperand;
		typedef ::std::function < BasicValue( BasicValue )> BasicUnaryOperand;
		typedef ::std::pair<integer, ::std::string> ProgramLine;
		typedef ::std::vector<ProgramLine> ProgramType;

		enum class ErrorTypes { SYNTAX };		

		class Basic {
		private:
			struct ConstantType {
				::std::string description;
				BasicValue value;
				ConstantType( ) { }
				ConstantType( ::std::string Description, BasicValue Value ): description( Description ), value( Value ) { }
			};
			struct FunctionType {
				::std::string description;
				BasicFunction func;
				FunctionType( ) { }
				FunctionType( ::std::string Description, BasicFunction Function ): description( Description ), func( Function ) { }
			};

			BasicValue evaluate( ::std::string value );

			::std::unordered_map<::std::string, FunctionType> m_functions;
			::std::unordered_map <::std::string, BasicValue> m_variables;
			::std::unordered_map <::std::string, ConstantType> m_constants;
			::std::unordered_map<::std::string, ::std::function<bool( ::std::string )>> m_keywords;
			ProgramType m_program;
			::std::unordered_map<::std::string, BasicBinaryOperand> m_binary_operators;
			::std::unordered_map<::std::string, BasicUnaryOperand> m_unary_operators;
			::std::vector<BasicValue> evaluate_parameters( ::std::string value );
			enum class RunMode { IMMEDIATE, DEFERRED };
			RunMode m_run_mode;
			ProgramType::iterator m_program_it;
			::std::vector<ProgramType::iterator> m_program_stack;	// GOSUB/RETURN
			ProgramType::iterator find_line( integer line_number );

			static ::std::vector<::std::string> split( ::std::string text, ::std::string delimiter );
			BasicValue& get_variable( ::std::string name );
			bool is_unary_operator( ::std::string oper );
			bool is_binary_operator( ::std::string oper );
			void clear_program( );
			void clear_variables( );
			void reset( );
			void init( );
			bool m_has_syntax_error;
			bool m_exiting;
			void set_program_it( integer line_number, integer offset = 0 );
			bool run( integer line_number = -1 );
			bool continue_run( );
			void sort_program_code( );
			ProgramType::iterator first_line( );
			BasicValue exec_function( ::std::string name, ::std::vector<BasicValue> arguments );
			::std::unique_ptr<Basic> m_basic;
			bool let_helper( ::std::string parse_string, bool show_error = true );

			BasicException CreateError( ErrorTypes error_type, ::std::string msg );

		public:
			Basic( );
			Basic( ::std::string program_code );

			void add_variable( ::std::string name, BasicValue value );
			void add_constant( ::std::string name, ::std::string description, BasicValue value );
			void add_function( ::std::string name, ::std::string description, BasicFunction func );
			::std::string list_functions( );
			::std::string list_constants( );
			::std::string list_keywords( );
			::std::string list_variables( );
			bool is_constant( ::std::string name );
			void remove_constant( ::std::string name, bool throw_on_nonexist );

			bool is_variable( ::std::string name );
			void remove_variable( ::std::string name, bool throw_on_nonexist = true );

			BasicValue get_variable_constant( ::std::string name );

			void add_line( integer line_number, ::std::string line );
			void remove_line( integer line );
			bool is_keyword( ::std::string name );
			bool is_function( ::std::string name );
			bool is_symbol( ::std::string name );

			bool parse_line( const ::std::string& parse_string, bool show_ready = true );
		};
	}
}



#endif	//HAS_DAWBASIC_HEADER