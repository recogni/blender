#include "COM_PythonNode.h"

#include "COM_PythonOperation.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BLI_path_util.h"

PythonNode::PythonNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void PythonNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	const NodePython* rna = (NodePython*)getbNode()->storage;

	char absolute[FILE_MAX] = { 0 };
	strcpy(absolute, rna->filepath);
	BLI_path_abs(absolute, G.main->name);

	PythonOperation *operation = new PythonOperation();
	operation->setData(rna);
	operation->setPath(absolute);

	converter.addOperation(operation);
	for (int i = 0; i < COM_PYTHON_INPUT_IMAGES + COM_PYTHON_INPUT_VALUES; i++) {
		converter.mapInputSocket(getInputSocket(i), operation->getInputSocket(i));
	}
	converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
}
