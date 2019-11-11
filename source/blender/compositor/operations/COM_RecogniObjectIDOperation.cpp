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
  setResolutionInputSocketIndex(1);
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
   *  The top 8 bits of the value represent the ClassID.
   *  The bottom 16 bits of the value represent the InstanceID.
   *  The alpha channel is always set to 1.0.
   *  ClassID and InstanceID of 0 is reserved for the background.
   */
  const uint classID = ((uint)(value >> 16)) & 0xFF;
  const uint instanceID = ((uint)value) & 0xFFFF;

  output[0] = ((float)classID) / 256.0f; 
  output[1] = (((float)((instanceID >> 8) & 0xFF))) / 256.0f;
  output[2] = (((float)((instanceID >> 0) & 0xFF))) / 256.0f;
  output[3] = 1.0f;
}

void RecogniObjectIDOperation::deinitExecution()
{
  this->m_inputValueProgram = NULL;
}
