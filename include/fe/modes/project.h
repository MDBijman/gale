#pragma once
#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"
#include "utils/reading/reader.h"
#include "fe/data/extended_ast.h"
#include "fe/modes/module.h"
#include "module.h"
#include "fe/libraries/core/core_operations.h"
#include "fe/libraries/std/std_ui.h"
#include "fe/libraries/std/std_output.h"
#include "fe/libraries/std/std_types.h"

#include <filesystem>
#include <string>

namespace fe
{
	class code_file
	{
	public:
		code_file(extended_ast::unique_node root) : root(std::move(root))
		{
			if (auto lines = dynamic_cast<extended_ast::tuple*>(this->root.get()))
			{
				for (auto& line : lines->children)
				{
					if (auto mod_declaration = dynamic_cast<extended_ast::module_declaration*>(line.get()))
						name = std::vector<std::string>{ mod_declaration->name.to_string() };

					if (auto imp_declaration = dynamic_cast<extended_ast::import_declaration*>(line.get()))
					{
						for (const extended_ast::identifier& module_import : imp_declaration->modules)
						{
							imports.push_back(module_import.segments);
						}
					}
				}
			}

			imports.push_back(module_name{ "core" });
		}

		code_file(const code_file& other) : root(extended_ast::unique_node(other.root->copy())), name(other.name),
			imports(other.imports) {}

		extended_ast::unique_node root;
		module_name name;
		std::vector<module_name> imports;
	};

	class module_graph
	{
	public:
		module_graph() {}
		module_graph(std::vector<code_file> modules, module_name root_module)
		{
			nodes.push_back(std::shared_ptr<native_module>(core::operations::load_as_module()));
			nodes.push_back(std::shared_ptr<native_module>(stdlib::ui::load_as_module()));
			nodes.push_back(std::shared_ptr<native_module>(stdlib::typedefs::load_as_module()));
			nodes.push_back(std::make_shared<native_module>(stdlib::output::load()));

			// At offset, only code modules are in the nodes vector, before that are all the native modules
			auto offset = nodes.size();

			for (auto& mod : modules)
			{
				auto new_module = std::make_shared<code_module>(code_module(mod.name, std::move(mod.root)));

				if (mod.name == root_module)
					root = new_module;

				nodes.push_back(new_module);
			}

			for (auto i = 0; i < modules.size(); i++)
			{
				auto& mod = modules.at(i);
				auto code_mod = static_cast<code_module*>(nodes.at(i + offset).get());

				for (const auto& import : mod.imports)
				{
					auto imported_module = std::find_if(nodes.begin(), nodes.end(), [&](std::shared_ptr<module>& node) { 
						return node->get_name() == import; 
					});

					if (imported_module == nodes.end())
						throw std::runtime_error("Unknown module");

					code_mod->imports.push_back(*imported_module);
				}
			}
		}

		std::shared_ptr<module> root;
		std::vector<std::shared_ptr<module>> nodes;
	};

	class project
	{
	public:
		project(const std::string folder, module_name main_module, fe::pipeline pipeline) : pl(std::move(pipeline))
		{
			graph = parse_project(folder, std::move(main_module));
		}

		std::tuple<type_environment, runtime_environment, scope_environment> interp()
		{
			return graph.root->interp(pl);
		}


	private:
		module_graph parse_project(std::string folder, module_name main)
		{
			std::vector<std::string> code_files;
			for (auto& item : std::experimental::filesystem::recursive_directory_iterator(folder))
			{
				auto path = item.path();
				if (path.filename().extension() != ".fe") continue;

				auto file_or_error = utils::files::read_file(path.string());
				if (std::holds_alternative<std::exception>(file_or_error))
				{
					std::cout << "File not found\n";
					continue;
				}
				code_files.push_back(std::move(std::get<std::string>(file_or_error)));
			}

			std::vector<code_file> parsed_files;
			for (auto&& text_file : code_files)
			{
				auto lexed = pl.lex(std::move(text_file));
				auto parsed = pl.parse(std::move(lexed));
				parsed_files.push_back(code_file(std::move(parsed)));
			}

			return module_graph(std::move(parsed_files), main);
		}


		module_graph graph;
		fe::pipeline pl;
	};
}