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

#include <array>
#include <boost/any.hpp>
#include <boost/utility/string_ref.hpp>
#include <cstdint>
#include <functional>
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
		using BasicKeyword = std::function<bool( boost::string_ref )>;
		using ProgramLine = std::pair<integer, boost::string_ref>;
		using ProgramType = std::vector<ProgramLine>;

		struct BasicException : public std::runtime_error {
			BasicException( ) = delete;
			~BasicException( );
			BasicException( BasicException const & ) = default;
			BasicException( BasicException && ) = default;
			BasicException &operator=( BasicException const & ) = default;
			BasicException &operator=( BasicException && ) = default;

			explicit BasicException( std::string const &msg, ErrorTypes errorType );
			explicit BasicException( char const *msg, ErrorTypes errorType );
			ErrorTypes error_type;
		}; // struct BasicException

		class Basic {
		private:
			struct ConstantType {
				std::string description;
				BasicValue value;
				ConstantType( ) {}
				ConstantType( std::string Description, BasicValue Value ) : description( Description ), value( Value ) {}
			};

			struct FunctionType {
				std::string description;
				BasicFunction func;
				FunctionType( ) {}
				FunctionType( std::string Description, BasicFunction Function )
				  : description( Description ), func( Function ) {}
			};

			class BasicArray {
				std::vector<size_t> m_dimensions;
				std::vector<BasicValue> m_values;

			public:
				BasicArray( );
				BasicArray( std::vector<size_t> dimensions );
				BasicArray( BasicArray const &other );
				BasicArray( BasicArray &&other );

				BasicArray &operator=( BasicArray other );
				bool operator==( BasicArray const &rhs ) const;

				BasicValue &operator( )( std::vector<size_t> dimensions );
				BasicValue const &operator( )( std::vector<size_t> dimensions ) const;

				std::vector<size_t> dimensions( ) const;
				size_t total_items( ) const;
			}; // class BasicArray

			std::unique_ptr<Basic> m_basic;
			std::unordered_map<std::string, std::function<bool( boost::string_ref )>> m_keywords;
			std::unordered_map<std::string, BasicBinaryOperand> m_binary_operators;
			std::unordered_map<std::string, BasicUnaryOperand> m_unary_operators;
			std::unordered_map<std::string, BasicValue> m_variables;
			std::unordered_map<std::string, BasicArray> m_arrays;
			std::unordered_map<std::string, ConstantType> m_constants;
			std::unordered_map<std::string, FunctionType> m_functions;
			std::vector<ProgramType::iterator> m_program_stack; // GOSUB/RETURN

			struct LoopStackType {
				class LoopType {
				protected:
					LoopType( ) = default;

				public:
					virtual ~LoopType( );
					virtual bool can_enter_loop_body( ) = 0;
				};

				class ForLoop : public LoopType {
					std::string m_variable_name;
					daw::MostlyImmutable<BasicValue> m_start_value;
					daw::MostlyImmutable<BasicValue> m_end_value;
					daw::MostlyImmutable<BasicValue> m_step_value;
					ForLoop( boost::string_ref variable_name, BasicValue start_value, BasicValue end_value,
					         BasicValue step_value );

				public:
					static std::shared_ptr<LoopType> create_for_loop( ProgramType::iterator program_line );
					bool can_enter_loop_body( ) override;
					~ForLoop( ) = default;
					ForLoop( ) = delete;
					ForLoop( ForLoop const & ) = default;
					ForLoop( ForLoop && ) = default;
					ForLoop &operator=( ForLoop const & ) = default;
					ForLoop &operator=( ForLoop && ) = default;
				}; // class ForLoop

			private:
				struct LoopStackValueType {
					std::shared_ptr<LoopType> loop_control;
					ProgramType::iterator start_of_loop;

					LoopStackValueType( std::shared_ptr<LoopType> loopControl, ProgramType::iterator program_line );
					LoopStackValueType( ) = delete;
					~LoopStackValueType( ) = default;
					LoopStackValueType( LoopStackValueType const & ) = default;
					LoopStackValueType( LoopStackValueType && ) = default;
					LoopStackValueType &operator=( LoopStackValueType const & ) = default;
					LoopStackValueType &operator=( LoopStackValueType && ) = default;
					bool can_enter_loop_body( );
				}; // struct LoopStackValueType

				std::vector<LoopStackValueType> loop_stack;
				LoopStackValueType &peek_full( );

			public:
				ProgramType::iterator peek( );
				ProgramType::iterator pop( );
				bool empty( ) const;
				size_t size( ) const;
				void push( std::shared_ptr<LoopType> type_of_loop, ProgramType::iterator start_of_loop );
			} m_loop_stack;

			std::vector<BasicValue> m_data_array;

			std::vector<BasicValue> evaluate_parameters( boost::string_ref value );

			BasicException create_basic_exception( ErrorTypes error_type, std::string msg );
			BasicValue exec_function( boost::string_ref name, std::vector<BasicValue> arguments );
			BasicValue &get_variable( boost::string_ref name );
			BasicValue &get_array_variable( boost::string_ref name, std::vector<BasicValue> params );
			BasicValue &get_array_variable( boost::string_ref name );
			std::pair<boost::string_ref, std::vector<BasicValue>>
			split_arrayfunction_from_string( boost::string_ref value, bool throw_on_missing_bracket = true );
			ProgramType m_program;
			ProgramType::iterator find_line( integer line_number );
			ProgramType::iterator first_line( );
			ProgramType::iterator m_program_it;

			enum class RunMode { IMMEDIATE, DEFERRED };
			RunMode m_run_mode;

			bool continue_run( );

			bool is_array( boost::string_ref name );
			bool is_binary_operator( boost::string_ref oper );
			bool is_unary_operator( boost::string_ref oper );
			bool let_helper( boost::string_ref parse_string, bool show_error = true );
			bool m_exiting;
			bool m_has_syntax_error;
			bool run( integer line_number = -1 );
			static std::vector<std::string> split( std::string text, std::string delimiter );
			static std::vector<std::string> split( std::string text, char delimiter );
			void add_array_variable( boost::string_ref name, std::vector<BasicValue> dimensions );
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
			BasicValue evaluate( boost::string_ref value );
			BasicValue &get_variable_constant( boost::string_ref name );
			bool is_constant( boost::string_ref name );
			bool is_function( boost::string_ref name );
			bool is_keyword( boost::string_ref name );
			bool is_variable( boost::string_ref name );
			bool parse_line( boost::string_ref parse_string, bool show_ready = true );
			void add_constant( boost::string_ref name, std::string description, BasicValue value );
			void add_function( boost::string_ref name, std::string description, BasicFunction func );
			void add_line( integer line_number, boost::string_ref line );
			void add_variable( boost::string_ref name, BasicValue value );
			void remove_array( boost::string_ref name, bool throw_on_nonexist = true );
			void remove_constant( boost::string_ref name, bool throw_on_nonexist );
			void remove_line( integer line );
			void remove_variable( boost::string_ref name, bool throw_on_nonexist = true );
		};
	} // namespace basic
} // namespace daw
