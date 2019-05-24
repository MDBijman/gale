#pragma once
#include "fe/data/bytecode.h"
#include "fe/data/interface.h"
#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/pipeline/bytecode_optimization_stage.h"
#include "fe/pipeline/bytecode_printing_stage.h"
#include "fe/pipeline/error.h"
#include "fe/pipeline/file_reader.h"
#include "fe/pipeline/interface_extraction_stage.h"
#include "fe/pipeline/lexer_stage.h"
#include "fe/pipeline/linker_stage.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/pipeline/parser_stage.h"
#include "fe/pipeline/resolution_stage.h"
#include "fe/pipeline/typechecker_stage.h"

#include <memory>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>

namespace fe
{
	class pipeline
	{
	      public:
		pipeline() : lexer(lexing_stage{}), parser(parsing_stage{})
		{
		}

		std::string read(const std::string &filename)
		{
			auto res = fe::read_file(filename);

			if (!res)
			{
				throw fe::other_error("Could not read file: " + filename);
			}

			return *res;
		}

		ext_ast::ast parse(const std::string &code)
		{
			auto res = lexer.lex(code);

			if (std::holds_alternative<lexing::error>(res))
				throw fe::other_error("Lexing error: " +
						      std::get<lexing::error>(res).message);

			auto parse_output = parser.parse(std::get<std::vector<lexing::token>>(res));

			if (std::holds_alternative<fe::parse_error>(parse_output))
				throw std::get<fe::parse_error>(parse_output);

			return std::move(std::get<fe::ext_ast::ast>(parse_output));
		}

		interface extract_interface(ext_ast::ast &ast) const
		{
			return ext_ast::extract_interface(ast);
		}

		void typecheck(ext_ast::ast &ast, const interfaces &ifaces) const
		{
			ext_ast::resolve(ast, ifaces);
			ext_ast::typecheck(ast, ifaces);
		}

		core_ast::ast lower(ext_ast::ast &ast) const
		{
			return ext_ast::lower(ast);
		}

		vm::module generate(core_ast::ast &ast, std::string module) const
		{
			return vm::generate_bytecode(ast, module);
		}

		void optimize_module(vm::module &e) const
		{
			vm::optimization_settings s;
			vm::optimize_module(e, s);
		}

		vm::executable link(std::unordered_map<std::string, vm::module> &modules, const std::string& main) const
		{
			return vm::link(modules, main);
		}

		void optimize_executable(vm::executable &e) const
		{
			vm::optimization_settings s;
			vm::optimize_executable(e, s);
		}

		void print_bytecode(const std::string &filename, vm::executable &e) const
		{
			vm::print_bytecode(filename, e);
		}

	      private:
		lexing_stage lexer;
		parsing_stage parser;
	};
} // namespace fe