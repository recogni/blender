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

#include "COM_RecogniObjectIDOperation.h"

RecogniObjectIDOperation::RecogniObjectIDOperation() : NodeOperation()
{
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_COLOR);
  
  this->m_inputValueProgram = NULL;
  setResolutionInputSocketIndex(0);
}

void RecogniObjectIDOperation::initExecution()
{
  this->m_inputValueProgram = this->getInputSocketReader(0);
}

void RecogniObjectIDOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  /*
   *  Sample the input value which should be the object id information which
   *  is replicated across all channels - we grab it from channel 0 to be safe.
   */
  float oid[4];
  this->m_inputValueProgram->readSampled(oid, x, y, sampler);
  const int value = int(oid[0]);
  
  /*
   *  The top 8 bits of the value represent the InstanceID.
   *  The bottom 8 bits of the value represent the ClassID.
   *  The alpha channel is always set to 1.0 and the red channel is always set 
   *  to 0.0.
   */
  const uint instanceId = ((uint)(value >> 8)) & 0xFF;
  const uint classId = ((uint)value) & 0xFF;

  output[0] = 0.0f;
  output[1] = ((float)instanceId) / 256.0f;
  output[2] = ((float)classId) / 256.0f;
  output[3] = 1.0f;
}

void RecogniObjectIDOperation::deinitExecution()
{
  this->m_inputValueProgram = NULL;
}
