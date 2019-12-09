/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_RecogniObjectIDNode.h"
#include "COM_RecogniObjectIDOperation.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"

RecogniObjectIDNode::RecogniObjectIDNode(bNode *editorNode) : Node(editorNode)
{
}

void RecogniObjectIDNode::convertToOperations(NodeConverter &converter,
                                     const CompositorContext & /*context*/) const
{
  NodeInput *inputSock = this->getInputSocket(0);
  NodeOutput *outputSock = this->getOutputSocket(0);
  RecogniObjectIDOperation *operation = new RecogniObjectIDOperation();

  converter.addOperation(operation);
  converter.mapInputSocket(inputSock, operation->getInputSocket(0));
  converter.mapOutputSocket(outputSock, operation->getOutputSocket(0));
}
