#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_python_in[] = {
	{ SOCK_RGBA,  N_("Image 0"),      1.0f, 1.0f, 1.0f, 1.0f },
	{ SOCK_RGBA,  N_("Image 1"),      1.0f, 1.0f, 1.0f, 1.0f },
	{ SOCK_RGBA,  N_("Image 2"),      1.0f, 1.0f, 1.0f, 1.0f },
	{ SOCK_RGBA,  N_("Image 3"),      1.0f, 1.0f, 1.0f, 1.0f },
	{ SOCK_FLOAT, N_("Input 0"),      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX },
	{ SOCK_FLOAT, N_("Input 1"),      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX },
	{ SOCK_FLOAT, N_("Input 2"),      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX },
	{ SOCK_FLOAT, N_("Input 3"),      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX },
	{ SOCK_FLOAT, N_("Input 4"),      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX },
	{ SOCK_FLOAT, N_("Input 5"),      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX },
	{ SOCK_FLOAT, N_("Input 6"),      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX },
	{ SOCK_FLOAT, N_("Input 7"),      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX },
	{ -1, "" }
};

static bNodeSocketTemplate cmp_node_python_out[] = {
	{ SOCK_RGBA, N_("Output 0") },
	{ -1, "" }
};

static void node_composit_init_python(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodePython *data = MEM_callocN(sizeof(NodePython), "python node");
	memset(data->filepath, 0, sizeof(data->filepath));

	node->storage = data;
}

void register_node_type_cmp_python(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_PYTHON, "Python Script", NODE_CLASS_OP_FILTER, 0);
	node_type_socket_templates(&ntype, cmp_node_python_in, cmp_node_python_out);
	node_type_init(&ntype, node_composit_init_python);
	node_type_storage(&ntype, "NodePython", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
