#ifndef HAS_DAWBASIC_HEADER
#define HAS_DAWBASIC_HEADER

#include <boost/any.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <list>


namespace daw {
	namespace basic {
		enum class ValueType { EMPTY, STRING, INTEGER, REAL, BOOLEAN };
		typedef int32_t integer;
		typedef double real;
		typedef bool boolean;

		typedef ::std::pair<ValueType, boost::any> BasicValue;
		typedef ::std::function<bool( ::std::string )> BasicKeyword;
		typedef ::std::function<BasicValue( ::std::vector<BasicValue> )> BasicFunction;
		typedef ::std::function < BasicValue( BasicValue, BasicValue )> BasicBinaryOperand;
		typedef ::std::pair<integer, ::std::string> ProgramLine;
		typedef ::std::vector<ProgramLine> ProgramType;


		class Basic {
		private:			
			BasicValue evaluate( ::std::string value );

			::std::map<::std::string, BasicFunction> functions;
			::std::map <::std::string, BasicValue> variables;
			::std::map <::std::string, BasicValue> constants;
			::std::map<::std::string, ::std::function<bool( ::std::string )>> keywords;
			ProgramType program;
			::std::map<::std::string, BasicBinaryOperand> binary_operators;
			::std::vector<BasicValue> evaluate_parameters( ::std::string value );
			enum class RunMode { IMMEDIATE, PROGRAM };
			RunMode m_run_mode;
			ProgramType::iterator m_program_it;
			ProgramType::iterator find_line( integer line_number );

			BasicValue& get_variable( ::std::string name );

			void init( );
			bool has_syntax_error;
			bool run( );
		public:
			Basic( );

			void add_variable( ::std::string name, BasicValue value );
			void add_constant( ::std::string name, BasicValue value );

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

			bool parse_line( const ::std::string& parse_string );
		};
	}
}



#endif	//HAS_DAWBASIC_HEADER