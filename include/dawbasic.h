#ifndef HAS_DAWBASIC_HEADER
#define HAS_DAWBASIC_HEADER

#include <array>
#include <boost/any.hpp>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "mostlyimmutable.h"

namespace daw {
	namespace basic {
		enum class ErrorTypes { SYNTAX, FATAL };
		enum class ValueType { EMPTY, STRING, INTEGER, REAL, BOOLEAN, ARRAY };
		using BasicValue = std::pair<ValueType, boost::any>;
		using boolean = bool;
		using real = double;
		using integer = int32_t;

		using BasicFunction = std::function<BasicValue( std::vector<BasicValue> )>;
		using BasicUnaryOperand = std::function<BasicValue( BasicValue )>;
		using BasicBinaryOperand = std::function<BasicValue( BasicValue, BasicValue )>;
		using BasicKeyword = std::function<bool( std::string )>;
		using ProgramLine = std::pair<integer, std::string>;
		using ProgramType = std::vector<ProgramLine>;
		
		class BasicException: public std::runtime_error {
		public:
			explicit BasicException( const std::string& msg, ErrorTypes errorType ): runtime_error( msg ) { }
			explicit BasicException( const char* msg, ErrorTypes errorType ): runtime_error( msg ) { }
			ErrorTypes error_type;
		};
		
		class Basic {
		private:
			struct ConstantType {
				std::string description;
				BasicValue value;
				ConstantType( ) { }
				ConstantType( std::string Description, BasicValue Value ): description( Description ), value( Value ) { }
			};

			struct FunctionType {
				std::string description;
				BasicFunction func;
				FunctionType( ) { }
				FunctionType( std::string Description, BasicFunction Function ): description( Description ), func( Function ) { }
			};

			class BasicArray {
			private:
				std::vector<size_t> m_dimensions;
				std::vector<BasicValue> m_values;
			public:
				BasicArray( );
				BasicArray( std::vector<size_t> dimensions );
				BasicArray( const BasicArray& other );
				BasicArray( BasicArray&& other );

				BasicArray& operator=(BasicArray other);
				bool operator==(const BasicArray& rhs) const;

				BasicValue& operator() ( std::vector<size_t> dimensions );
				const BasicValue& operator() ( std::vector<size_t> dimensions ) const;

				std::vector<size_t> dimensions( ) const;
				size_t total_items( ) const;
			};
		
			std::unique_ptr<Basic> m_basic;
			std::unordered_map<std::string, std::function<bool( std::string )>> m_keywords;
			std::unordered_map<std::string, BasicBinaryOperand> m_binary_operators;
			std::unordered_map<std::string, BasicUnaryOperand> m_unary_operators;
			std::unordered_map<std::string, BasicValue> m_variables;
			std::unordered_map<std::string, BasicArray> m_arrays;
			std::unordered_map<std::string, ConstantType> m_constants;
			std::unordered_map<std::string, FunctionType> m_functions;
			std::vector<ProgramType::iterator> m_program_stack;	// GOSUB/RETURN

			class LoopStackType {
			public:
				class LoopType {
				protected:
					LoopType( ) = default;
				public:
					virtual ~LoopType( ) = 0;
					virtual bool can_enter_loop_body( ) = 0;
				};				

				class ForLoop: public LoopType {
				private:
					std::string m_variable_name;
					daw::MostlyImmutable<BasicValue> m_start_value;
					daw::MostlyImmutable<BasicValue> m_end_value;
					daw::MostlyImmutable<BasicValue> m_step_value;
					ForLoop( std::string variable_name, BasicValue start_value, BasicValue end_value, BasicValue step_value );
				public:
					static std::shared_ptr<LoopType> create_for_loop( ProgramType::iterator program_line );
					virtual bool can_enter_loop_body( );
				};


			private:
				struct LoopStackValueType {
					std::shared_ptr<LoopType> loop_control;
					ProgramType::iterator start_of_loop;
					LoopStackValueType( ) = delete;
					LoopStackValueType( std::shared_ptr<LoopType> loopControl, ProgramType::iterator program_line );
					bool can_enter_loop_body( );
				};
				std::vector<LoopStackValueType> loop_stack;
				LoopStackValueType& peek_full( );
			public:
				ProgramType::iterator peek( );
				ProgramType::iterator pop( );
				bool empty( ) const;				
				size_t size( ) const;
				void push( std::shared_ptr<LoopType> type_of_loop, ProgramType::iterator start_of_loop );
			} m_loop_stack;

			std::vector<BasicValue> m_data_array;

			std::vector<BasicValue> evaluate_parameters( std::string value );

			BasicException create_basic_exception( ErrorTypes error_type, std::string msg );
			BasicValue exec_function( std::string name, std::vector<BasicValue> arguments );
			BasicValue& get_variable( std::string name );
			BasicValue& get_array_variable( std::string name, std::vector<BasicValue> params );
			BasicValue& get_array_variable( std::string name );
			std::pair<std::string, std::vector<BasicValue>> split_arrayfunction_from_string( std::string value, bool throw_on_missing_bracket = true );
			ProgramType m_program;
			ProgramType::iterator find_line( integer line_number );
			ProgramType::iterator first_line( );
			ProgramType::iterator m_program_it;

			enum class RunMode { IMMEDIATE, DEFERRED };
			RunMode m_run_mode;

			bool continue_run( );

			bool is_array( std::string name );
			bool is_binary_operator( std::string oper );
			bool is_unary_operator( std::string oper );
			bool let_helper( std::string parse_string, bool show_error = true );
			bool m_exiting;
			bool m_has_syntax_error;
			bool run( integer line_number = -1 );
			static std::vector<std::string> split( std::string text, std::string delimiter );
			static std::vector<std::string> split( std::string text, char delimiter );
			void add_array_variable( std::string name, std::vector<BasicValue> dimensions );
			void clear_program( );
			void clear_variables( );
			void init( );
			void reset( );
			void set_program_it( integer line_number, integer offset = 0 );
			void sort_program_code( );

		public:
			Basic( );
			Basic( std::string program_code );

			std::string list_constants( );
			std::string list_functions( );
			std::string list_keywords( );
			std::string list_variables( );
			BasicValue evaluate( std::string value );
			BasicValue& get_variable_constant( std::string name );
			bool is_constant( std::string name );
			bool is_function( std::string name );
			bool is_keyword( std::string name );
			bool is_variable( std::string name );
			bool parse_line( const std::string& parse_string, bool show_ready = true );
			void add_constant( std::string name, std::string description, BasicValue value );
			void add_function( std::string name, std::string description, BasicFunction func );
			void add_line( integer line_number, std::string line );
			void add_variable( std::string name, BasicValue value );
			void remove_array( std::string name, bool throw_on_nonexist = true );
			void remove_constant( std::string name, bool throw_on_nonexist );
			void remove_line( integer line );
			void remove_variable( std::string name, bool throw_on_nonexist = true );
		};
	}
}



#endif	//HAS_DAWBASIC_HEADER