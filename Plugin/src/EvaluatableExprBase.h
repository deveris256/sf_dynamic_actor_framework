#pragma once
#include "exprtk.hpp"

namespace utils
{
	template<typename _Evaluated_Result_T>
	class Evaluatable
	{
	public:
		using _Parser_T = exprtk::parser<_Evaluated_Result_T>;
		using _Expression_T = exprtk::expression<_Evaluated_Result_T>;
		using _SymbolTable_T = exprtk::symbol_table<_Evaluated_Result_T>;

		_SymbolTable_T*		symbol_table{ nullptr };
		_Expression_T       expr;
		_Evaluated_Result_T default_value;

		std::string compiler_error;
		bool        use_expr{ false };

		Evaluatable()
			: default_value(_Evaluated_Result_T())
		{}
		
		explicit Evaluatable(_SymbolTable_T& symbol_table)
			: symbol_table(&symbol_table)
		{}

		explicit Evaluatable(const _Evaluated_Result_T& default_value)
			: default_value(default_value)
		{}

		Evaluatable(_Evaluated_Result_T const_val, _SymbolTable_T& symbol_table)
			: default_value(const_val), symbol_table(&symbol_table)
		{}

		Evaluatable(const std::string& expr_str, _SymbolTable_T& symbol_table) 
			: symbol_table(&symbol_table)
		{
			if (Parse(expr_str, symbol_table)) {
				default_value = expr.value();
			}
		}

		Evaluatable(const std::string& expr_str, _SymbolTable_T& symbol_table, const _Evaluated_Result_T& default_value)
			: default_value(default_value), symbol_table(&symbol_table)
		{
			Parse(expr_str, symbol_table);
		}

		virtual ~Evaluatable() = default;

		// Overridable. Handles use_expr flag
		virtual bool Parse(const std::string& expr_str, _SymbolTable_T& symbol_table)
		{
			expr.register_symbol_table(symbol_table);
			_Parser_T parser;
			if (!parser.compile(expr_str, expr)) {
				compiler_error = parser.error();
				use_expr = false;
				return false;
			}
			use_expr = true;
			return true;
		}

		virtual _Evaluated_Result_T Evaluate()
		{
			if (!use_expr) {
				return default_value;
			}
			return expr.value();
		}

		// Set to constant value
		bool Set(const _Evaluated_Result_T& value) {
			use_expr = false;
			default_value = value;
			return true;
		}

		// Set to constant value, move semantics
		bool Set(_Evaluated_Result_T&& value)
		{
			use_expr = false;
			default_value = std::move(value);
			return true;
		}

		// Set to expression
		bool Set(const std::string& expr_str)
		{
			if (symbol_table == nullptr) {
				compiler_error = "Evaluatable: Symbol table is not set";
				return false;
			}
			return Parse(expr_str, *symbol_table);
		}

		const std::string& GetCompilerError() const
		{
			return compiler_error;
		}

		// Operator overloads
		_Evaluated_Result_T operator()()
		{
			return Evaluate();
		}
	};
}