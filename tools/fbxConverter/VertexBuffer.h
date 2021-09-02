#pragma once

namespace reng
{

	static const uint32_t kMaxVertexElems = 16;
	static const uint32_t kMaxStreamCount = 2;

	struct VertexElement					//48 bytes
	{
		uint8_t			m_index;
		int8_t			m_size;
		uint16_t		m_type;
		uint8_t			m_normalized;
		uint8_t			m_offset;			//CLR - is this big enough for MultiDrawIndirect?
	};
	/*
		struct VertexStream
		{
			uint8_t			m_bufferType;
			uint8_t			m_numElements;
			uint16_t		m_stride;
			VertexElement	*m_elements;
		};
	*/
	namespace Import
	{
		struct VertexElement
		{
			uint8_t			m_index;
			int8_t			m_size;
			uint16_t		m_type;
			uint8_t			m_normalized;
			uint8_t			m_offset;			//CLR - is this big enough for MultiDrawIndirect?
		};

		struct VertexStream
		{
			VertexStream();
			void addElement(uint8_t index, int8_t size, uint16_t type, uint8_t normalized, uint8_t offset);

			uint32_t				m_glBufferId;
			uint8_t					m_bufferType;
			uint8_t					m_numElements;
			uint16_t				m_stride;
			uint32_t				m_dataOffset;
			Import::VertexElement	m_elements[kMaxVertexElems];
		};

		struct VertexBuffer
		{
			VertexBuffer();

			inline VertexStream* getVertexStreams() { return m_streams; }

			uint32_t		m_numStreams;
			VertexStream	m_streams[kMaxStreamCount];
		};
	}
}
