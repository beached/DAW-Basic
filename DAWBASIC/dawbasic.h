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
#include <array>


namespace daw {
	namespace basic {
		enum class ErrorTypes { SYNTAX, FATAL };
		enum class ValueType { EMPTY, STRING, INTEGER, REAL, BOOLEAN, ARRAY };

		typedef ::std::pair<ValueType, boost::any> BasicValue;
		typedef bool boolean;
		typedef double real;
		typedef int32_t integer;


		typedef ::std::function<BasicValue( ::std::vector<BasicValue> )> BasicFunction;
		typedef ::std::function<BasicValue( BasicValue )> BasicUnaryOperand;
		typedef ::std::function<BasicValue( BasicValue, BasicValue )> BasicBinaryOperand;
		typedef ::std::function<bool( ::std::string )> BasicKeyword;
		typedef ::std::pair<integer, ::std::string> ProgramLine;
		typedef ::std::vector<ProgramLine> ProgramType;

		class BasicException: public ::std::runtime_error {
		public:
			explicit BasicException( const ::std::string& msg, ErrorTypes errorType ): runtime_error( msg ) { }
			explicit BasicException( const char* msg, ErrorTypes errorType ): runtime_error( msg ) { }
			ErrorTypes error_type;
		};
		
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

			class BasicArray {
			private:
				const ::std::vector<size_t> m_dimensions;
				::std::vector<BasicValue> m_values;
			public:
				BasicArray( );
				BasicArray( ::std::vector<size_t> dimensions );

				BasicValue& operator() ( ::std::vector<size_t> dimensions );
				const BasicValue& operator() ( ::std::vector<size_t> dimensions ) const;
				::std::vector<size_t> dimensions( ) const;
				size_t total_items( ) const;
			};
		
			::std::unique_ptr<Basic> m_basic;
			::std::unordered_map<::std::string, ::std::function<bool( ::std::string )>> m_keywords;
			::std::unordered_map<::std::string, BasicArray> m_arrays;
			::std::unordered_map<::std::string, BasicBinaryOperand> m_binary_operators;
			::std::unordered_map<::std::string, BasicUnaryOperand> m_unary_operators;
			::std::unordered_map<::std::string, BasicValue> m_variables;
			::std::unordered_map<::std::string, ConstantType> m_constants;
			::std::unordered_map<::std::string, FunctionType> m_functions;
			::std::vector<ProgramType::iterator> m_program_stack;	// GOSUB/RETURN

			::std::vector<BasicValue> evaluate_parameters( ::std::string value );

			BasicException create_basic_exception( ErrorTypes error_type, ::std::string msg );
			BasicValue exec_function( ::std::string name, ::std::vector<BasicValue> arguments );
			BasicValue& get_variable( ::std::string name );
			BasicValue& get_array_variable( ::std::string name, ::std::vector<BasicValue> params );
			ProgramType m_program;
			ProgramType::iterator find_line( integer line_number );
			ProgramType::iterator first_line( );
			ProgramType::iterator m_program_it;

			enum class RunMode { IMMEDIATE, DEFERRED };
			RunMode m_run_mode;

			bool continue_run( );

			bool is_array( ::std::string name );
			bool is_binary_operator( ::std::string oper );
			bool is_unary_operator( ::std::string oper );
			bool let_helper( ::std::string parse_string, bool show_error = true );
			bool m_exiting;
			bool m_has_syntax_error;
			bool run( integer line_number = -1 );
			static ::std::vector<::std::string> split( ::std::string text, ::std::string delimiter );
			static ::std::vector<::std::string> split( ::std::string text, char delimiter );
			void add_array_variable( ::std::string name, ::std::vector<BasicValue> dimensions );
			void clear_program( );
			void clear_variables( );
			void init( );
			void reset( );
			void set_program_it( integer line_number, integer offset = 0 );
			void sort_program_code( );

		public:
			Basic( );
			Basic( ::std::string program_code );

			::std::string list_constants( );
			::std::string list_functions( );
			::std::string list_keywords( );
			::std::string list_variables( );
			BasicValue evaluate( ::std::string value );
			BasicValue& get_variable_constant( ::std::string name );
			bool is_constant( ::std::string name );
			bool is_function( ::std::string name );
			bool is_keyword( ::std::string name );
			bool is_variable( ::std::string name );
			bool parse_line( const ::std::string& parse_string, bool show_ready = true );
			void add_constant( ::std::string name, ::std::string description, BasicValue value );
			void add_function( ::std::string name, ::std::string description, BasicFunction func );
			void add_line( integer line_number, ::std::string line );
			void add_variable( ::std::string name, BasicValue value );
			void remove_array( ::std::string name, bool throw_on_nonexist = true );
			void remove_constant( ::std::string name, bool throw_on_nonexist );
			void remove_line( integer line );
			void remove_variable( ::std::string name, bool throw_on_nonexist = true );
		};
	}
}



#endif	//HAS_DAWBASIC_HEADER