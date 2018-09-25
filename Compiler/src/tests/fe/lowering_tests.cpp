#include <catch2/catch.hpp>
#include "fe/data/ext_ast.h"
#include "fe/data/ast_data.h"
#include "fe/pipeline/lowering_stage.h"

TEST_CASE("lower id", "[lowering]")
{
	using namespace fe;
	ext_ast::ast ast;

	auto ns = ast.create_name_scope();
	auto ts = ast.create_type_scope();

	auto id = ast.create_node(ext_ast::node_type::IDENTIFIER);
	auto& id_node = ast.get_node(id);
	ast.set_root_id(id);
	
	id_node.name_scope_id = ns;
	id_node.type_scope_id = ts;

	ast.get_data<ext_ast::identifier>(id_node.data_index) = { "a", {}, 0, {} };

	auto res = fe::ext_ast::lower(ast);
}
