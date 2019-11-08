
#ifndef _COM_PythonOperation_h_
#define _COM_PythonOperation_h_

#include <string>

#include "COM_NodeOperation.h"
#include "COM_SingleThreadedOperation.h"

#define COM_PYTHON_INPUT_IMAGES 4
#define COM_PYTHON_INPUT_VALUES 8

class PythonOperation : public SingleThreadedOperation {
private:
	const NodePython *m_data;
	std::string m_path;
public:
	PythonOperation();

	void initExecution();
	void deinitExecution();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	void setData(const NodePython *data) { this->m_data = data; }
	void setPath(const std::string& path) { this->m_path = path; }

protected:

	MemoryBuffer *createMemoryBuffer(rcti *rect);
};

#endif