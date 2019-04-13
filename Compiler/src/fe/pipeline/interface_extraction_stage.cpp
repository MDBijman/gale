#include "fe/pipeline/interface_extraction_stage.h"
#include "fe/pipeline/resolution_stage.h"
#include "fe/pipeline/typechecker_stage.h"
#include "fe/data/interface.h"
#include "fe/data/ext_ast.h"

namespace fe::ext_ast
{
	fe::interface extract_interface(ast &ast)
	{
		fe::interface res;
		auto helper = ast_helper(ast);

		std::string module_name;
		helper.for_all_t(node_type::MODULE_DECLARATION, [&ast, &module_name](auto &n) {
			auto &module_id_node = ast[ast.get_children(n.children_id)[0]];
			auto &id_data = ast.get_data<identifier>(module_id_node.data_index);
			module_name = id_data.name;
		});
		res.name = module_name;

		ast_helper(ast).for_all_t(node_type::FUNCTION, [&ast, &res](node &n) {
			auto &children = ast.children_of(n);
			auto &lhs_node = ast[children[0]];
			auto &type_node = ast[children[1]];

			auto &id_data = ast.get_data<identifier>(lhs_node.data_index);
			auto fun_type = typeof(type_node, ast);

			res.names.declare_variable(id_data.name, lhs_node.id);
			res.names.define_variable(id_data.name);
			res.types.set_type(id_data.name, std::move(fun_type));
		});

		return res;
	}
} // namespace fe::ext_ast