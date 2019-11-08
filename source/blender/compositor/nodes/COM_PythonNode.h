#ifndef _COM_PythonNode_h_
#define _COM_PythonNode_h_

#include "COM_Node.h"

class PythonNode : public Node {
public:
	PythonNode(bNode *editorNode);
	void convertToOperations(NodeConverter &converter, const CompositorContext &context) const;
};

#endif // _COM_PythonNode_h_
