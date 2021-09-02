/*
*  VertexStream.cpp
*  ion
*
*  Created by Claire Rogers on 14/09/2016.
*  Copyright 2016 Guilty Dog Productions Ltd. All rights reserved.
*
*/
#include "stdafx.h"
#include "VertexBuffer.h"

namespace reng
{
	namespace Import
	{
		VertexStream::VertexStream()
		{
			m_numElements = 0;
			m_stride = 0;
		}

		void VertexStream::addElement(uint8_t index, int8_t size, uint16_t type, uint8_t normalized, uint8_t offset)
		{
			uint32_t elem = m_numElements++;
			m_elements[elem].m_index = index;
			m_elements[elem].m_size = size;
			m_elements[elem].m_type = type;
			m_elements[elem].m_normalized = normalized;
			m_elements[elem].m_offset = offset;

			switch (type)
			{
			case kFloat:
				m_stride += sizeof(float) * size;
				break;
			default:
				assert(0);
			}

		}

		VertexBuffer::VertexBuffer()
			: m_numStreams(0)
		{
			for (uint32_t i = 0; i < kMaxStreamCount; ++i)
			{
				m_streams[i].m_numElements = 0;
				m_streams[i].m_stride = 0;
			}
		}
	}
}
