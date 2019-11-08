
#include <string>
#include <fstream>
#include <vector>

#include "WM_api.h"
#include "Python.h"
#include "numpy/arrayobject.h"

#include "COM_PythonOperation.h"

extern "C"
void initCompositorPython()
{
	if (PyArray_API == NULL) {
		//This does not seem to be the best way to do this...
		if (_import_array() < 0) {
			if (PyErr_Occurred()) {
				PyErr_Print();
			}
			Py_FatalError("Can't import numpy array");
		}
	}
}

void fatal(const char *message)
{
	if (PyErr_Occurred()) {
		PyErr_Print();
	}
	Py_FatalError(message);
}

PythonOperation::PythonOperation() : SingleThreadedOperation()
{
	for (int i = 0; i < COM_PYTHON_INPUT_IMAGES; i++) {
		addInputSocket(COM_DT_COLOR);
	}
	for (int i = 0; i < COM_PYTHON_INPUT_VALUES; i++) {
		addInputSocket(COM_DT_VALUE);
	}
	addOutputSocket(COM_DT_COLOR);
	setResolutionInputSocketIndex(0);

	m_data = NULL;
}

void PythonOperation::initExecution()
{
	SingleThreadedOperation::initExecution();
}

void PythonOperation::deinitExecution()
{
	SingleThreadedOperation::deinitExecution();
}

std::string readPythonFileContent(const std::string& path)
{
	std::ifstream input(path);
	std::stringstream buffer;
	buffer << input.rdbuf();
	return buffer.str();
}

PyObject *createScriptModule(const std::string& content, const std::string& path)
{
	PyObject *pyModule = PyModule_New("blender_com_python");
	if (pyModule) {
		PyModule_AddStringConstant(pyModule, "__file__", path.c_str());

		PyObject *locals = PyModule_GetDict(pyModule); //Borrowed ref
		PyObject *builtins = PyEval_GetBuiltins(); //Borrowed ref
		PyDict_SetItemString(locals, "__builtins__", builtins);

		PyObject *pyValue = PyRun_String(content.c_str(), Py_file_input, locals, locals);
		if (pyValue) {
			Py_DECREF(pyValue);
		}
		else {
			PyErr_Print();
		}
	}
	return pyModule;
}

bool callMethod(PyObject* pyModule, const char *name, PyObject* context)
{
	bool ok = true; //Assume no function is defined with the given name

	PyObject* callback = PyDict_GetItemString(PyModule_GetDict(pyModule), name); //Borrowed ref
	if (callback) {
		ok = false; //Function found

		PyObject* args = Py_BuildValue("(O)", context);
		if (args) {
			PyObject *tmp = PyObject_CallObject(callback, args);
			if (tmp) {
				ok = true;
				Py_CLEAR(tmp);
			}
			else {
				PyErr_Print();
			}
			Py_CLEAR(args);
		}
		else {
			PyErr_Print();
		}
	}
	else {
		PyErr_Print();
	}
	return ok;
}

void addBuffersToContext(PyObject *context, const char *name, std::vector<MemoryBuffer*> *buffers)
{
	const int count = buffers->size();
	PyObject *results = PyList_New(count);

	for (int i = 0; i < count; i++) {
		MemoryBuffer *buffer = buffers->at(i);

		npy_intp dims[3];
		dims[0] = buffer->getHeight(); //Height first dimension
		dims[1] = buffer->getWidth();
		dims[2] = 4;
		PyObject *arr = PyArray_SimpleNewFromData(3, dims, NPY_FLOAT32, buffer->getBuffer());
		if (arr) {
			PyList_SetItem(results, i, arr); //Steals ref
		}
		else {
			fatal("Can't create a numpy array from a buffer");
		}
	}

	PyDict_SetItemString(context, name, results);
	Py_DECREF(results);
}

void addValuesToContext(PyObject *context, const char *name, std::vector<float> *values)
{
	const int count = values->size();
	PyObject *results = PyList_New(count);

	for (int i = 0; i < count; i++) {
		PyObject *pyFloat = PyFloat_FromDouble(values->at(i));
		if (pyFloat) {
			PyList_SET_ITEM(results, i, pyFloat); //Steals ref
		}
		else {
			fatal("Can't create a value float");
		}
	}

	PyDict_SetItemString(context, name, results);
	Py_DECREF(results);
}

void clearBuffer(MemoryBuffer *buffer)
{
	const float invalid[] = { 1, 0, 0, 1 };
	const int width = buffer->getWidth();
	const int height = buffer->getHeight();

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			buffer->writePixel(x, y, invalid);
		}
	}
}

struct ThreadData {
	ThreadCondition condition;
	ThreadMutex *mutex;

	PyObject *context;
	PyObject *pyModule;
	std::string scriptPath;
	std::string scriptContent;
	bool startOk;
};

void pythonMainThreadCallback(void* userData)
{
	BLI_assert(BLI_thread_is_main());

	PyGILState_STATE gstate = PyGILState_Ensure();

	ThreadData* data = (ThreadData*)userData;

	data->pyModule = createScriptModule(data->scriptContent, data->scriptPath);
	if (data->pyModule) {
		data->startOk = callMethod(data->pyModule, "on_main", data->context);
	}
	PyGILState_Release(gstate);

	BLI_condition_notify_all(&data->condition);
}

MemoryBuffer *PythonOperation::createMemoryBuffer(rcti *source)
{
	rcti rect;
	rect.xmin = 0;
	rect.ymin = 0;
	rect.xmax = source->xmax;
	rect.ymax = source->ymax;
	MemoryBuffer *result = new MemoryBuffer(COM_DT_COLOR, &rect);

	std::vector<MemoryBuffer*> inputs;
	for (int i = 0; i < COM_PYTHON_INPUT_IMAGES; i++) {
		MemoryBuffer *tile = (MemoryBuffer*)getInputSocketReader(i)->initializeTileData(source);
		inputs.push_back(tile);
	}

	std::vector<MemoryBuffer*> outputs;
	outputs.push_back(result);

	for (int i = 0; i < outputs.size(); i++) {
		clearBuffer(outputs[i]);
	}

	const std::string content = readPythonFileContent(m_path);

	// Script import and main callback
	PyGILState_STATE gstate;
	gstate = PyGILState_Ensure();

	PyObject* context = PyDict_New();
	addBuffersToContext(context, "images", &inputs);
	addBuffersToContext(context, "outputs", &outputs);

	std::vector<float> values;
	float value[4] = { 0 };
	for (int i = 0; i < COM_PYTHON_INPUT_VALUES; i++) {
		getInputSocketReader(COM_PYTHON_INPUT_IMAGES + i)->readSampled(value, 0, 0, COM_PS_NEAREST);
		values.push_back(value[0]);
	}
	addValuesToContext(context, "inputs", &values);

	PyGILState_Release(gstate);

	ThreadData threadData;
	threadData.condition = {};
	threadData.mutex = BLI_mutex_alloc();
	BLI_condition_init(&threadData.condition);

	threadData.context = context;
	threadData.pyModule = NULL;
	threadData.scriptContent = content;
	threadData.scriptPath = m_path;
	threadData.startOk = false;

	// Run Python init in the main thread and wait until it is ready
	WM_run_in_main_thread(pythonMainThreadCallback, &threadData);
	BLI_condition_wait(&threadData.condition, threadData.mutex);

	// Async callback
	gstate = PyGILState_Ensure();

	bool endOk = false;
	if (threadData.pyModule) {
		endOk = callMethod(threadData.pyModule, "on_async", context);
	}
	Py_CLEAR(context);
	Py_CLEAR(threadData.pyModule);

	PyGILState_Release(gstate);

	if (!threadData.startOk || !endOk) {
		// A callback failed, erase all output buffers
		for (int i = 0; i < outputs.size(); i++) {
			clearBuffer(outputs[i]);
		}
	}

	BLI_condition_end(&threadData.condition);
	BLI_mutex_free(threadData.mutex);

	return result;
}

bool PythonOperation::determineDependingAreaOfInterest(rcti * /*input*/, ReadBufferOperation *readOperation, rcti *output)
{
	if (isCached()) {
		return false;
	}
	else {
		rcti newInput;
		newInput.xmin = 0;
		newInput.ymin = 0;
		newInput.xmax = getWidth();
		newInput.ymax = getHeight();
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}